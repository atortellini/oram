#include "roram.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "globals.h"
#include "stash.h"
#include "suboram.h"

void RORAM_init(RORAM *roram) {
  for (size_t i = 0; i < NUM_SUBORAMS; i++) {
    SUBORAM *current_suboram = &roram->subORAMs[i];
    SUBORAM_init(current_suboram, i);
  }
}

// The paper specifies that non-power of two ranges are supported by just
// choosing the smallest, larger power of 2 range to use. However, since the
// experiment harness is only going to use powers of two for the range the
// access will be implemented with the assupmtion that the range is a power
// of 2.
void RORAM_access(RORAM *roram, const uint32_t address,
                  const uint32_t range_size_power_of_two, const OPERATION op,
                  const uint8_t *new_data, uint8_t *old_data) {

  const uint32_t range_size = 1U << range_size_power_of_two;
  const uint32_t suboram_index = range_size_power_of_two;
  const uint32_t boundary_aligned_base_address =
      (address / range_size) * range_size;

  BLOCK *retrieved_data =
      (BLOCK *)malloc(sizeof(*retrieved_data) * range_size * 2);
  assert(retrieved_data);

  bool *addresses_found_map =
      (bool *)calloc(range_size * 2, sizeof(*addresses_found_map));
  assert(addresses_found_map);

  const SUBORAM *suboram = &roram->subORAMs[suboram_index];

  const uint32_t first_range_new_path =
      SUBORAM_read_range(suboram, boundary_aligned_base_address, retrieved_data,
                         addresses_found_map);

  const uint32_t second_range_new_path = SUBORAM_read_range(
      suboram, boundary_aligned_base_address + range_size,
      retrieved_data + range_size, addresses_found_map + range_size);

  for (uint32_t address_offset = 0; address_offset < (range_size * 2);
       address_offset++) {

    const uint32_t current_address =
        boundary_aligned_base_address + address_offset;

    const bool addressNotFound = addresses_found_map[address_offset];
    if (addressNotFound) {
      BLOCK *new_block = &retrieved_data[address_offset];
      create_new_block(roram, new_block, current_address);
    }
  }

  update_blocks_path_tags(retrieved_data, suboram_index, range_size,
                          first_range_new_path);
  update_blocks_path_tags(retrieved_data + range_size, suboram_index,
                          range_size, second_range_new_path);

  const uint32_t address_offset = address - boundary_aligned_base_address;
  BLOCK *interested_blocks = retrieved_data + address_offset;

  const bool wantsOldData = old_data != NULL;
  const bool writingNewData = op == WRITE;
  if (wantsOldData || writingNewData) {
    for (uint32_t i = 0; i < range_size; i++) {
      const uint32_t data_arr_offset = i * NUM_DATA_BYTES_PER_BLOCK;
      BLOCK *interested_block = &interested_blocks[i];
      if (wantsOldData) {
        memcpy(old_data + data_arr_offset, interested_block->data,
               NUM_DATA_BYTES_PER_BLOCK);
      }
      if (writingNewData) {
        memcpy(interested_block->data, new_data + data_arr_offset,
               NUM_DATA_BYTES_PER_BLOCK);
      }
    }
  }

  for (uint32_t i = 0; i < NUM_SUBORAMS; i++) {
    SUBORAM *suboram_to_update = &roram->subORAMs[i];
    STASH *stash_to_update = &suboram_to_update->stash;

    find_and_remove_blocks_in_range_from_stash(
        stash_to_update, boundary_aligned_base_address, range_size * 2);
    add_blocks_to_stash(stash_to_update, retrieved_data, range_size * 2);
    SUBORAM_batch_evict(suboram_to_update, range_size * 2);
  }

  EVICTION_COUNTER += (range_size * 2);

  free(retrieved_data);
  free(addresses_found_map);
}

static void create_new_block(RORAM *roram, BLOCK *block,
                             const uint32_t block_address) {
  block->block_address = block_address;
  for (uint32_t i = 0; i < NUM_SUBORAMS; i++) {
    block->path_tags[i] =
        SUBORAM_query_position_map(&roram->subORAMs[i], block_address);
  }
  memset(block->data, 0, NUM_DATA_BYTES_PER_BLOCK);
}

static void add_blocks_to_stash(STASH *stash, const BLOCK *blocks_to_add,
                                const uint32_t num_blocks) {
  for (uint32_t i = 0; i < num_blocks; i++) {
    STASH_add_block(stash, &blocks_to_add[i]);
  }
}

static void find_and_remove_blocks_in_range_from_stash(
    STASH *stash, const uint32_t base_address, const uint32_t range_size) {
  const uint32_t end_address = base_address + (range_size - 1);
  for (size_t i = 0; i < stash->num_blocks;) {
    BLOCK *current_block = &stash->stash[i];
    const uint32_t current_block_address = current_block->block_address;
    const bool blockIsInAddressRange = base_address <= current_block_address &&
                                       current_block_address <= end_address;

    if (blockIsInAddressRange) {
      STASH_remove_block(stash, i);
    } else {
      i++;
    }
  }
}

static void update_blocks_path_tags(BLOCK *blocks, const uint32_t suboram_index,
                                    const uint32_t range_size,
                                    const uint32_t new_base_path) {
  for (uint32_t i = 0; i < range_size; i++) {
    blocks[i].path_tags[suboram_index] =
        (new_base_path + i) % NUM_TOTAL_REAL_BLOCKS;
  }
}
