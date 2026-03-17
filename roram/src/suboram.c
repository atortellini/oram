#include "suboram.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "constants.h"
#include "crypto.h"
#include "globals.h"
#include "rand.h"
#include "server.h"
#include "stash.h"
#include "types.h"

#define min(X, Y) ((X) < (Y) ? (X) : (Y))

static ENCRYPTED_BUCKET *encrypt_buckets(const BUCKET *plaintext_buckets,
                                         const uint32_t num_buckets);
static void fill_bucket_with_blocks_from_stash_intersecting_path_at_level(
    SUBORAM *oram, BLOCK *bucket_buffer, const uint32_t path_tag,
    const uint32_t level);
static void insert_unique_blocks_into_stash(STASH *stash,
                                            const BLOCK *new_blocks,
                                            const size_t num_blocks,
                                            bool *addresses_found_map);
static void find_and_record_blocks_found_in_range_from_stash(
    const SUBORAM *suboram, BLOCK *results, bool *addresses_found_map,
    const uint32_t base_address, const uint32_t range_size);
static BLOCK *read_buckets_at_level_along_paths(const SUBORAM *oram,
                                                const size_t level,
                                                const uint32_t range_start_tag,
                                                const uint32_t range_size,
                                                size_t *num_blocks_found);
static bool is_dummy_block(BLOCK *block);
static void fill_pmap_random_base_paths(POSITION_MAP *pm, uint32_t num_entries);
static uint32_t calculate_absolute_bucket_id(const uint32_t suboram_index,
                                             const uint32_t path,
                                             const uint32_t level);
static void record_blocks_in_address_range(
    const BLOCK *blocks, const size_t num_blocks, const uint32_t base_address,
    const uint32_t range_size, bool *address_found_map, BLOCK *filtered_results,
    const bool ignoreIfAlreadyFound);
static bool *record_block_addresses_found_in_stash(const SUBORAM *suboram);

static uint32_t read_bucket_range(const SUBORAM *suboram,
                                  uint32_t range_start_path,
                                  uint32_t range_size, uint32_t level,
                                  ENCRYPTED_BUCKET **buckets_out);
static void write_bucket_range(SUBORAM *suboram, uint32_t range_start_path,
                               uint32_t range_size, uint32_t level,
                               ENCRYPTED_BUCKET *encrypted_buckets,
                               uint32_t num_buckets);

void SUBORAM_init(SUBORAM *oram, const size_t suboram_index) {
  oram->oram_index = suboram_index;
  const uint32_t num_pmap_entries = 1 << (HEIGHT_OF_TREE - suboram_index);
  uint32_t *new_pmap = (uint32_t *)malloc(sizeof(*new_pmap) * num_pmap_entries);
  assert(new_pmap);

  oram->pm.map = new_pmap;

  fill_pmap_random_base_paths(&oram->pm, num_pmap_entries);

  STASH_init(&oram->stash);
}

// assumes that results pointer will be of size range_size (2^i for subram R_i)
uint32_t SUBORAM_read_range(SUBORAM *oram, const uint32_t address,
                            BLOCK *results, bool *addresses_found_map) {
  const uint32_t range_size = 1U << oram->oram_index;
  const uint32_t range_start_tag = SUBORAM_query_position_map(oram, address);
  const uint32_t new_range_tag = uniform_random(NUM_TOTAL_REAL_BLOCKS);
  SUBORAM_update_position_map(oram, address, new_range_tag);

  find_and_record_blocks_found_in_range_from_stash(
      oram, results, addresses_found_map, address, range_size);

  for (uint32_t level = 0; level <= HEIGHT_OF_TREE; level++) {

    ENCRYPTED_BUCKET *encrypted_server_buckets = NULL;
    uint32_t num_server_buckets = read_bucket_range(
        oram, range_start_tag, range_size, level, &encrypted_server_buckets);

    BUCKET *bucket_writeback =
        (BUCKET *)malloc(sizeof(*bucket_writeback) * num_server_buckets);
    assert(bucket_writeback);

    for (uint32_t i = 0; i < num_server_buckets; i++) {
      const ENCRYPTED_BUCKET *encrypted_bucket = &encrypted_server_buckets[i];

      for (uint32_t j = 0; j < NUM_BLOCKS_PER_BUCKET; j++) {
        const ENCRYPTED_BLOCK *encrypted_block = &encrypted_bucket->blocks[j];
        BLOCK decrypted_block;
        CRYPTO_decrypt_block(encrypted_block, &decrypted_block);

        const bool blockIsInAddressRange =
            address <= decrypted_block.block_address &&
            decrypted_block.block_address <= (address + range_size - 1);
        if (blockIsInAddressRange) {
          bucket_writeback[i].blocks[j] = DUMMY_BLOCK;

          const uint32_t address_offset =
              decrypted_block.block_address - address;

          const bool blockAlreadyFound = addresses_found_map[address_offset];
          if (blockAlreadyFound) {
            continue;
          }
          addresses_found_map[address_offset] = true;
          results[address_offset] = decrypted_block;
        } else {
          bucket_writeback[i].blocks[j] = decrypted_block;
        }
      }
    }

    ENCRYPTED_BUCKET *encrypted_bucket_writeback =
        encrypt_buckets(bucket_writeback, num_server_buckets);

    write_bucket_range(oram, range_start_tag, range_size, level,
                       encrypted_bucket_writeback, num_server_buckets);

    free(bucket_writeback);
    free(encrypted_bucket_writeback);
    free(encrypted_server_buckets);
  }

  return new_range_tag;
}

void SUBORAM_batch_evict(SUBORAM *oram, const size_t num_evictions) {
  bool *addresses_found_map = record_block_addresses_found_in_stash(oram);

  for (size_t level = 0; level <= HEIGHT_OF_TREE; level++) {
    size_t num_blocks_found = 0;
    BLOCK *blocks_found = read_buckets_at_level_along_paths(
        oram, level, EVICTION_COUNTER, num_evictions, &num_blocks_found);

    insert_unique_blocks_into_stash(&oram->stash, blocks_found,
                                    num_blocks_found, addresses_found_map);

    free(blocks_found);
  }

  free(addresses_found_map);

  for (int level = HEIGHT_OF_TREE; level >= 0; level--) {
    const uint32_t num_buckets_at_level = 1U << level;
    const uint32_t bounded_num_unique_paths =
        min(num_buckets_at_level, num_evictions);

    BUCKET *new_buckets_at_level = (BUCKET *)malloc(
        sizeof(*new_buckets_at_level) * bounded_num_unique_paths);
    assert(new_buckets_at_level);

    for (uint32_t path_offset = 0; path_offset < bounded_num_unique_paths;
         path_offset++) {
      const uint32_t path = EVICTION_COUNTER + path_offset;
      BLOCK *bucket_buffer = new_buckets_at_level[path_offset].blocks;

      fill_bucket_with_blocks_from_stash_intersecting_path_at_level(
          oram, bucket_buffer, path, level);
    }

    ENCRYPTED_BUCKET *new_encrypted_buckets =
        encrypt_buckets(new_buckets_at_level, bounded_num_unique_paths);

    write_bucket_range(oram, EVICTION_COUNTER, num_evictions, level,
                       new_encrypted_buckets, bounded_num_unique_paths);

    free(new_encrypted_buckets);
    free(new_buckets_at_level);
  }
}

uint32_t SUBORAM_query_position_map(const SUBORAM *suboram,
                                    const uint32_t address) {
  const uint32_t range_size = 1U << suboram->oram_index;
  const uint32_t range_number_containing_address = address / range_size;
  const uint32_t range_base_address =
      range_number_containing_address * range_size;
  const uint32_t address_offset = address - range_base_address;

  const uint32_t range_base_path =
      suboram->pm.map[range_number_containing_address];

  return range_base_path + address_offset;
}

void SUBORAM_update_position_map(SUBORAM *suboram, const uint32_t address,
                                 const uint32_t new_path) {
  const uint32_t range_size = 1U << suboram->oram_index;
  const uint32_t range_number_containing_address = address / range_size;
  const uint32_t range_base_address =
      range_number_containing_address * range_size;
  const uint32_t address_offset = address - range_base_address;

  const uint32_t new_range_base_path = new_path - address_offset;
  suboram->pm.map[range_number_containing_address] = new_range_base_path;
}

static ENCRYPTED_BUCKET *encrypt_buckets(const BUCKET *plaintext_buckets,
                                         const uint32_t num_buckets) {
  ENCRYPTED_BUCKET *encrypted_buckets =
      (ENCRYPTED_BUCKET *)malloc(sizeof(*encrypted_buckets) * num_buckets);
  assert(encrypted_buckets);

  for (uint32_t i = 0; i < num_buckets; i++) {
    for (uint32_t j = 0; j < NUM_BLOCKS_PER_BUCKET; j++) {
      CRYPTO_encrypt_block(&plaintext_buckets[i].blocks[j],
                           &encrypted_buckets[i].blocks[j]);
    }
  }

  return encrypted_buckets;
}

static uint32_t calculate_absolute_bucket_id(const uint32_t suboram_index,
                                             const uint32_t path,
                                             const uint32_t level) {
  const uint32_t num_buckets_at_level = 1U << level;
  const uint32_t num_buckets_above_level = (1U << level) - 1;
  const uint32_t bit_reversed_bucket_id_at_level = path % num_buckets_at_level;
  const uint32_t target_bucket_id_in_suboram =
      num_buckets_above_level + bit_reversed_bucket_id_at_level;
  const uint32_t suboram_start_index = suboram_index * NUM_BUCKETS_PER_ORAM;

  return suboram_start_index + target_bucket_id_in_suboram;
}

static void fill_bucket_with_blocks_from_stash_intersecting_path_at_level(
    SUBORAM *oram, BLOCK *bucket_buffer, const uint32_t path_tag,
    const uint32_t level) {
  STASH *stash = &oram->stash;
  const uint32_t num_buckets_at_level = 1U << level;
  const uint32_t evicted_path_at_level = path_tag % num_buckets_at_level;

  size_t found_block_counter = 0;

  for (size_t i = 0; i < stash->num_blocks;) {
    const BLOCK *candidate_block = &stash->stash[i];
    const uint32_t candidate_path_at_level =
        candidate_block->path_tags[oram->oram_index] % num_buckets_at_level;
    const bool candidatePathIntersectsEvictedPath =
        candidate_path_at_level == evicted_path_at_level;

    if (candidatePathIntersectsEvictedPath) {
      bucket_buffer[found_block_counter++] = *candidate_block;
      STASH_remove_block(&oram->stash, i);
      const bool bucketBufferIsFull =
          found_block_counter == NUM_BLOCKS_PER_BUCKET;
      if (bucketBufferIsFull) {
        break;
      }
    } else {
      i++;
    }
  }

  const bool bucketIsUnderfilled = found_block_counter < NUM_BLOCKS_PER_BUCKET;
  if (bucketIsUnderfilled) {
    for (size_t i = found_block_counter; i < NUM_BLOCKS_PER_BUCKET; i++) {
      bucket_buffer[i] = DUMMY_BLOCK;
    }
  }
}

static void insert_unique_blocks_into_stash(STASH *stash,
                                            const BLOCK *new_blocks,
                                            const size_t num_blocks,
                                            bool *addresses_found_map) {
  for (size_t i = 0; i < num_blocks; i++) {
    const BLOCK *current_block = &new_blocks[i];
    const uint32_t current_block_address = current_block->block_address;
    const bool blockAlreadyFound = addresses_found_map[current_block_address];
    if (blockAlreadyFound) {
      continue;
    } else {
      addresses_found_map[current_block_address] = true;
      STASH_add_block(stash, current_block);
    }
  }
}

static void find_and_record_blocks_found_in_range_from_stash(
    const SUBORAM *suboram, BLOCK *results, bool *addresses_found_map,
    const uint32_t base_address, const uint32_t range_size) {

  const STASH *stash = &suboram->stash;
  const BLOCK *stash_blocks = stash->stash;
  const size_t num_blocks = stash->num_blocks;

  record_blocks_in_address_range(stash_blocks, num_blocks, base_address,
                                 range_size, addresses_found_map, results,
                                 false);
}

static bool *record_block_addresses_found_in_stash(const SUBORAM *suboram) {

  bool *addresses_found_map =
      (bool *)calloc(NUM_TOTAL_REAL_BLOCKS, sizeof(*addresses_found_map));
  assert(addresses_found_map);
  record_blocks_in_address_range(
      suboram->stash.stash, suboram->stash.num_blocks, 0, NUM_TOTAL_REAL_BLOCKS,
      addresses_found_map, NULL, false);

  return addresses_found_map;
}

static void record_blocks_in_address_range(
    const BLOCK *blocks, const size_t num_blocks, const uint32_t base_address,
    const uint32_t range_size, bool *address_found_map, BLOCK *filtered_results,
    const bool ignoreIfAlreadyFound) {

  const uint32_t end_address = base_address + (range_size - 1);
  const bool wantsFilteredBlocks = filtered_results != NULL;
  for (size_t i = 0; i < num_blocks; i++) {
    const BLOCK *current_block = &blocks[i];
    const uint32_t current_block_address = current_block->block_address;
    const bool blockIsInRange = base_address <= current_block_address &&
                                current_block_address <= end_address;

    if (blockIsInRange) {
      const uint32_t address_offset = current_block_address - base_address;

      const bool blockAlreadyFound = address_found_map[address_offset];

      if (blockAlreadyFound && ignoreIfAlreadyFound) {
        continue;
      }
      address_found_map[address_offset] = true;
      if (wantsFilteredBlocks) {
        filtered_results[address_offset] = *current_block;
      }
    }
  }
}

static BLOCK *read_buckets_at_level_along_paths(const SUBORAM *oram,
                                                const size_t level,
                                                const uint32_t range_start_tag,
                                                const uint32_t range_size,
                                                size_t *num_blocks_found) {

  *num_blocks_found = 0;

  ENCRYPTED_BUCKET *encrypted_buckets = NULL;

  uint32_t num_buckets_read = read_bucket_range(
      oram, range_start_tag, range_size, level, &encrypted_buckets);

  BLOCK *blocks_found = (BLOCK *)malloc(
      sizeof(*blocks_found) * NUM_BLOCKS_PER_BUCKET * num_buckets_read);
  assert(blocks_found);

  for (uint32_t bkt_id = 0; bkt_id < num_buckets_read; bkt_id++) {
    const ENCRYPTED_BUCKET *encrypted_bucket = &encrypted_buckets[bkt_id];

    for (uint32_t blk_idx = 0; blk_idx < NUM_BLOCKS_PER_BUCKET; blk_idx++) {
      const ENCRYPTED_BLOCK *encrypted_block =
          &encrypted_bucket->blocks[blk_idx];

      BLOCK decrypted_block;
      CRYPTO_decrypt_block(encrypted_block, &decrypted_block);
      if (is_dummy_block(&decrypted_block)) {
        continue;
      } else {
        blocks_found[(*num_blocks_found)++] = decrypted_block;
      }
    }
  }

  free(encrypted_buckets);

  return blocks_found;
}

static bool is_dummy_block(BLOCK *block) {
  return block->block_address == DUMMY_BLOCK.block_address;
}

static void fill_pmap_random_base_paths(POSITION_MAP *pm,
                                        uint32_t num_entries) {
  for (uint32_t i = 0; i < num_entries; i++) {
    pm->map[i] = uniform_random(NUM_TOTAL_REAL_BLOCKS);
  }
}

static uint32_t read_bucket_range(const SUBORAM *suboram,
                                  uint32_t range_start_path,
                                  uint32_t range_size, uint32_t level,
                                  ENCRYPTED_BUCKET **buckets_out) {
  const uint32_t num_buckets_at_level = 1U << level;
  range_size = min(num_buckets_at_level, range_size);

  *buckets_out = NULL;
  ENCRYPTED_BUCKET *encrypted_buckets =
      (ENCRYPTED_BUCKET *)malloc(sizeof(*encrypted_buckets) * range_size);
  assert(encrypted_buckets);

  uint32_t num_buckets_read = 0;

  const uint32_t range_start_path_at_level =
      range_start_path % num_buckets_at_level;

  const uint32_t range_end_path_at_level =
      range_start_path_at_level + range_size - 1;

  const bool rangeWrapsaround = range_end_path_at_level >= num_buckets_at_level;
  if (rangeWrapsaround) {
    const uint32_t num_buckets_over =
        range_end_path_at_level - num_buckets_at_level + 1;

    const uint32_t first_range_size = range_size - num_buckets_over;
    const uint32_t second_range_size = num_buckets_over;

    const uint32_t first_range_server_bucket_id = calculate_absolute_bucket_id(
        suboram->oram_index, range_start_path, level);
    const uint32_t second_range_server_bucket_id =
        calculate_absolute_bucket_id(suboram->oram_index, 0, level);

    ENCRYPTED_BUCKET *first_range_bucket_buffer = encrypted_buckets;
    ENCRYPTED_BUCKET *second_range_bucket_buffer =
        encrypted_buckets + first_range_size;

    SERVER_read_range(second_range_server_bucket_id, second_range_size,
                      second_range_bucket_buffer);
    SERVER_read_range(first_range_server_bucket_id, first_range_size,
                      first_range_bucket_buffer);

  } else {
    const uint32_t server_bucket_id = calculate_absolute_bucket_id(
        suboram->oram_index, range_start_path, level);
    SERVER_read_range(server_bucket_id, range_size, encrypted_buckets);
  }

  num_buckets_read = range_size;
  *buckets_out = encrypted_buckets;
  return num_buckets_read;
}

static void write_bucket_range(SUBORAM *suboram, uint32_t range_start_path,
                               uint32_t range_size, uint32_t level,
                               ENCRYPTED_BUCKET *encrypted_buckets,
                               uint32_t num_buckets) {
  const uint32_t num_buckets_at_level = 1U << level;
  range_size = min(num_buckets_at_level, range_size);

  assert(num_buckets == range_size);

  const uint32_t range_start_path_at_level =
      range_start_path % num_buckets_at_level;

  const uint32_t range_end_path_at_level =
      range_start_path_at_level + range_size - 1;

  const bool rangeWrapsaround = range_end_path_at_level >= num_buckets_at_level;
  if (rangeWrapsaround) {
    const uint32_t num_buckets_over =
        range_end_path_at_level - num_buckets_at_level + 1;

    const uint32_t first_range_size = range_size - num_buckets_over;
    const uint32_t second_range_size = num_buckets_over;

    const uint32_t first_range_server_bucket_id = calculate_absolute_bucket_id(
        suboram->oram_index, range_start_path, level);
    const uint32_t second_range_server_bucket_id =
        calculate_absolute_bucket_id(suboram->oram_index, 0, level);

    ENCRYPTED_BUCKET *first_range_buckets = encrypted_buckets;
    ENCRYPTED_BUCKET *second_range_buckets =
        encrypted_buckets + first_range_size;

    SERVER_write_range(second_range_server_bucket_id, second_range_size,
                       second_range_buckets);
    SERVER_write_range(first_range_server_bucket_id, first_range_size,
                       first_range_buckets);

  } else {
    const uint32_t server_bucket_id = calculate_absolute_bucket_id(
        suboram->oram_index, range_start_path, level);
    SERVER_write_range(server_bucket_id, range_size, encrypted_buckets);
  }
}
