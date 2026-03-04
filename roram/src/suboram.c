#include "suboram.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "constants.h"
#include "globals.h"
#include "rand.h"
#include "server.h"
#include "stash.h"
#include "types.h"

#define min(X, Y) ((X) < (Y) ? (X) : (Y))

void SUBORAM_init(SUBORAM *oram, const size_t suboram_index) {
  oram->oram_index = suboram_index;
  const uint32_t num_pmap_entries = 1 << (HEIGHT_OF_TREE - suboram_index);
  uint32_t *new_pmap = (uint32_t *)malloc(sizeof(*new_pmap) * num_pmap_entries);
  assert(new_pmap);

  oram->pm.map = new_pmap;

  fill_pmap_random_base_paths(&oram->pm, num_pmap_entries);
}

// assumes that results pointer will be of size range_size (2^i for subram R_i)
uint32_t SUBORAM_read_range(SUBORAM *oram, const uint32_t address,
                            BLOCK *results, bool *addresses_found_map) {

  const uint32_t range_size = 1U << oram->oram_index;
  const uint32_t range_start_tag = SUBORAM_query_position_map(oram, address);
  const uint32_t new_range_tag = uniform_random(NUM_TOTAL_REAL_BLOCKS);
  SUBORAM_update_position_map(oram, address, new_range_tag);

  find_and_record_blocks_found_in_range_from_stash(
      &oram->stash, results, addresses_found_map, address, range_size);

  fetch_and_record_new_blocks_in_range_from_server(
      oram, results, addresses_found_map, address, range_start_tag, range_size);

  return new_range_tag;
}

void SUBORAM_batch_evict(SUBORAM *oram, const size_t num_evictions) {

  for (size_t level = 0; level <= HEIGHT_OF_TREE; level++) {
    const uint32_t num_max_buckets = 1 << level;
    const uint32_t bounded_num_buckets = min(num_max_buckets, num_evictions);
    const uint32_t bounded_num_blocks =
        bounded_num_buckets * NUM_BLOCKS_PER_BUCKET;

    BLOCK *blocks_found =
        (BLOCK *)malloc(sizeof(*blocks_found) * bounded_num_blocks);
    assert(blocks_found);

    const size_t num_blocks_found = read_buckets_at_level_along_paths(
        oram, level, EVICTION_COUNTER, num_evictions, blocks_found);

    insert_new_blocks_into_stash(&oram->stash, blocks_found, num_blocks_found);

    free(blocks_found);
  }

  // I think I'll just write back the buckets back to the server per level
  // rather than collecting the entire set of buckets that need to be written
  // across all levels then doing it all at once the reason is I can't really
  // think of a structure to store the buckets in that would be easy to access
  // afterwards when going to write all the buckets back, also to actually
  // insert the buckets into the structure. Also a thing to note like my other
  // functions I can limit the number of iterations over the stash per level to
  // just be min(num_evictions, num_buckets_at_level). This is because each
  // eviction path is sequential and is modulus the number of buckets in the
  // level. Therefore, each subsequent eviction path will be unique and, if the
  // number of evictions were to be greater than the number of buckets in the
  // level, all unique buckets will have been exhausted.
  for (int level = HEIGHT_OF_TREE; level >= 0; level--) {
    const uint32_t num_buckets_at_level = 1 << level;
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

    write_bucket_range_to_server(oram, new_buckets_at_level, EVICTION_COUNTER,
                                 bounded_num_unique_paths, level);

    free(new_buckets_at_level);
  }
}

const uint32_t SUBORAM_query_position_map(const SUBORAM *suboram,
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

static void write_bucket_range_to_server(const SUBORAM *oram,
                                         const BUCKET *buckets,
                                         const uint32_t base_path,
                                         const uint32_t num_buckets,
                                         const uint32_t level) {
  const uint32_t num_buckets_at_level = 1U << level;
  const uint32_t num_buckets_above_level = (1U << level) - 1;

  ENCRYPTED_BUCKET *encrypted_buckets =
      (ENCRYPTED_BUCKET *)malloc(sizeof(*encrypted_buckets) * num_buckets);
  assert(encrypted_buckets);

  encrypt_buckets(buckets, encrypted_buckets, num_buckets);

  for (uint32_t i = 0; i < num_buckets; i++) {
    const uint32_t bit_reversed_bucket_id_at_level =
        (base_path + i) % num_buckets_at_level;
    const uint32_t target_bucket_id_in_suboram =
        bit_reversed_bucket_id_at_level + num_buckets_above_level;
    const uint32_t absolute_bucket_id =
        oram->oram_index * NUM_BUCKETS_PER_ORAM + target_bucket_id_in_suboram;

    SERVER_write_bucket(absolute_bucket_id, &encrypted_buckets[i]);
  }

  free(encrypted_buckets);
}

static void encrypt_buckets(const BUCKET *plaintext_buckets,
                            ENCRYPTED_BUCKET *encrypted_buckets,
                            const uint32_t num_buckets) {
  for (uint32_t i = 0; i < num_buckets; i++) {
    for (uint32_t j = 0; j < NUM_BLOCKS_PER_BUCKET; j++) {
      CRYPTO_encrypt_block(&plaintext_buckets[i].blocks[j],
                           &encrypted_buckets[i].blocks[j]);
    }
  }
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

static void insert_new_blocks_into_stash(STASH *stash, BLOCK *new_blocks,
                                         const size_t num_blocks) {

  for (size_t i = 0; i < num_blocks; i++) {
    const BLOCK *block_to_add = &new_blocks[i];
    if (is_block_in_stash(stash, block_to_add)) {
      continue;
    } else {
      STASH_add(stash, block_to_add);
    }
  }
}

static bool is_block_in_stash(const STASH *stash, const BLOCK *block) {
  const target_block_address = block->block_address;
  for (size_t i = 0; i < stash->num_blocks; i++) {
    const BLOCK *current_block = &stash->stash[i];
    if (current_block->block_address == target_block_address)
      return true;
  }
  return false;
}
// ESSENTIALLY THE SAME CODE AS THAT USED FOR CHECKING FOR BLOCKS IN RANGE THAT
// WERE FETCHED FROM THE SERVER, COULD PROBABLY COMPACT IT
static void find_and_record_blocks_found_in_range_from_stash(
    const STASH *stash, BLOCK *results, bool *addresses_found_map,
    const uint32_t base_address, const uint32_t range_size) {

  const uint32_t end_address = base_address + (range_size - 1);
  for (size_t i = 0; i < stash->num_blocks; i++) {

    const BLOCK *current_block = &stash->stash[i];
    const uint32_t current_block_address = current_block->block_address;

    if (current_block_address >= base_address &&
        current_block_address <= end_address) {
      const uint32_t address_offset = current_block_address - base_address;

      if (addresses_found_map[address_offset]) {
        fprintf(stderr,
                "Block for address: %u was somehow already found.\nSuggests "
                "possibly duplicates of the same block in the stash not "
                "overwriting each other.\n",
                current_block_address);
      }
      addresses_found_map[address_offset] = true;
      results[address_offset] = *current_block;
    }
  }
}

static void fetch_and_record_new_blocks_in_range_from_server(
    SUBORAM *oram, BLOCK *results, bool *addresses_found_map,
    const uint32_t base_address, const uint32_t range_start_tag,
    const uint32_t range_size) {

  for (size_t level = 0; level <= HEIGHT_OF_TREE; level++) {
    const uint32_t num_buckets_at_level = 1U << level;
    const uint32_t num_buckets_to_access =
        min(range_size, num_buckets_at_level);

    BLOCK *blocks_found = (BLOCK *)malloc(
        sizeof(*blocks_found) * NUM_BLOCKS_PER_BUCKET * num_buckets_to_access);
    assert(blocks_found);
    const size_t num_real_blocks = read_buckets_at_level_along_paths(
        oram, level, range_start_tag, range_size, blocks_found);

    filter_and_record_new_blocks(blocks_found, num_real_blocks, results,
                                 base_address, range_size, addresses_found_map);

    free(blocks_found);
  }
}

static size_t read_buckets_at_level_along_paths(const SUBORAM *oram,
                                                const size_t level,
                                                const uint32_t range_start_tag,
                                                const uint32_t range_size,
                                                BLOCK *blocks_found) {

  size_t num_blocks_found = 0;
  const uint32_t num_buckets_at_level = 1U << level;
  const uint32_t num_buckets_above_level = (1U << level) - 1;
  const uint32_t num_buckets_to_access = min(range_size, num_buckets_at_level);

  ENCRYPTED_BUCKET **encrypted_buckets = (ENCRYPTED_BUCKET **)malloc(
      sizeof(*encrypted_buckets) * num_buckets_to_access);
  assert(encrypted_buckets);

  for (uint32_t i = 0; i < num_buckets_to_access; i++) {
    const uint32_t bit_reversed_bucket_id_at_level =
        (range_start_tag + i) % num_buckets_at_level;
    const uint32_t target_bucket_id_in_suboram =
        bit_reversed_bucket_id_at_level + num_buckets_above_level;
    const uint32_t absolute_bucket_id =
        oram->oram_index * NUM_BUCKETS_PER_ORAM + target_bucket_id_in_suboram;

    encrypted_buckets[i] = SERVER_read_bucket(absolute_bucket_id);
  }

  for (uint32_t bucket_idx = 0; bucket_idx < num_buckets_to_access;
       bucket_idx++) {
    const ENCRYPTED_BUCKET *encrypted_bucket = encrypted_buckets[bucket_idx];
    for (uint32_t block_idx = 0; block_idx < NUM_BLOCKS_PER_BUCKET;
         block_idx++) {
      const ENCRYPTED_BLOCK *encrypted_block =
          &encrypted_bucket->blocks[block_idx];
      BLOCK decrypted_block;
      CRYPTO_decrypt_block(encrypted_block, &decrypted_block);
      if (is_dummy_block(&decrypted_block)) {
        continue;
      } else {
        blocks_found[num_blocks_found++] = decrypted_block;
      }
    }
  }

  free(encrypted_buckets);

  return num_blocks_found;
}

static bool is_dummy_block(BLOCK *block) {
  return block->block_address == DUMMY_BLOCK.block_address;
}

static void filter_and_record_new_blocks(BLOCK *blocks, const size_t num_blocks,
                                         BLOCK *results,
                                         const uint32_t base_address,
                                         const uint32_t range_size,
                                         bool *addresses_found_map) {

  const uint32_t end_address = base_address + (range_size - 1);
  for (size_t i = 0; i < num_blocks; i++) {
    const BLOCK *current_block = &blocks[i];
    const uint32_t current_block_address = current_block->block_address;
    const bool blockAddressIsInRange = current_block_address >= base_address &&
                                       current_block_address <= end_address;
    if (blockAddressIsInRange) {
      const uint32_t address_offset = current_block_address - base_address;
      const bool blockAddressAlreadyFound = addresses_found_map[address_offset];

      if (blockAddressAlreadyFound) {
        continue;
      } else {
        addresses_found_map[address_offset] = true;
        results[address_offset] = *current_block;
      }
    }
  }
}

static void fill_pmap_random_base_paths(POSITION_MAP *pm,
                                        uint32_t num_entries) {
  for (uint32_t i = 0; i < num_entries; i++) {
    pm->map[i] = uniform_random(NUM_TOTAL_REAL_BLOCKS);
  }
}
