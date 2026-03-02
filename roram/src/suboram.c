#include "suboram.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "constants.h"
#include "rand.h"
#include "types.h"

#define min(X, Y) ((X) < (Y) ? (X) : (Y))

void SUBORAM_init(SUBORAM *oram, const size_t suboram_index) {
  oram->oram_index = suboram_index;
  const size_t num_pmap_entries =
      NUM_TOTAL_REAL_BLOCKS /
      (1 << suboram_index); // or could just do height of tree minus suboram
                            // index.
  uint32_t *new_pmap = (uint32_t *)malloc(sizeof(*new_pmap) * num_pmap_entries);
  assert(new_pmap);
  oram->pm.map = new_pmap;
}

// assumes that results pointer will be of size range_size (2^i for subram R_i)
uint32_t SUBORAM_read_range(SUBORAM *oram, const uint32_t address,
                            BLOCK *results) {
  const uint32_t range_size = 1U << oram->oram_index;
  const uint32_t range_start_tag = oram->pm.map[address];
  const uint32_t new_range_tag = uniform_random(NUM_TOTAL_REAL_BLOCKS);
  oram->pm.map[address] = new_range_tag;

  bool *addresses_found_map =
      (bool *)calloc(range_size, sizeof(*addresses_found_map));
  assert(addresses_found_map);

  find_and_record_blocks_found_in_range_from_stash(
      &oram->stash, results, addresses_found_map, address, range_size);

  fetch_and_record_new_blocks_in_range_from_server(
      oram, results, addresses_found_map, address, range_start_tag, range_size);

  free(addresses_found_map);
}

void SUBORAM_batch_evict(SUBORAM *oram, const size_t num_evictions) {

  for (int level = HEIGHT_OF_TREE; level >= 0; level--) {
    const uint32_t num_max_buckets = 1 << level;
    const uint32_t bounded_num_buckets = min(num_max_buckets, num_evictions);
    const uint32_t bounded_num_blocks =
        bounded_num_buckets * NUM_BLOCKS_PER_BUCKET;

    BLOCK *blocks_found =
        (BLOCK *)malloc(sizeof(*blocks_found) * bounded_num_blocks);
    assert(blocks_found);

    const size_t num_blocks_found = read_buckets_at_level_along_paths(
        oram, level, eviction_counter, num_evictions, blocks_found);

    insert_new_blocks_into_stash(oram->stash, blocks_found, num_blocks_found);

    free(blocks_found);
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
        range_start_tag + i % num_buckets_at_level;
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

static uint32_t bit_reverse_addition(const uint32_t base, const uint32_t addend,
                                     const uint32_t msb_index) {
  if (msb_index == 0)
    return 0;
  if (addend == 0)
    return base;

  return swap_msb_and_lsb((swap_msb_and_lsb(base, msb_index) + addend),
                          msb_index);
}

static uint32_t swap_msb_and_lsb(const uint32_t x, const uint32_t msb_index) {
  const uint32_t msb_flag = 1U << msb_index;
  const uint32_t lsb_flag = 1U;

  const uint32_t bits_0_to_msb_set = (1 << (msb_index + 1)) - 1;
  const uint32_t all_bits_but_msb_and_lsb_set = ~(msb_flag | lsb_flag);

  const uint32_t intermediate_bits_flag =
      all_bits_but_msb_and_lsb_set & bits_0_to_msb_set;

  return ((x & msb_flag) >> msb_index) | ((x & lsb_flag) << msb_index) |
         (x & intermediate_bits_flag);
}
