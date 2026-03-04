#include "stash.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "globals.h"
#include "types.h"

void STASH_remove_block(STASH *stash, const uint32_t index) {
  const size_t num_blocks = stash->num_blocks;
  const bool stashIsEmpty = num_blocks == 0;
  if (stashIsEmpty) {
    fprintf(stderr, "Attempt to remove block from an empty client stash");
    exit(1);
  }
  const bool indexIsOutOfRange = index > (num_blocks - 1);
  if (indexIsOutOfRange) {
    fprintf(
        stderr,
        "Attempt to remove out of range block from client stash at index: %d",
        index);
    exit(1);
  }

  const bool stashHasOneBlock = num_blocks == 1;
  if (stashHasOneBlock) {
    stash->num_blocks--;
    return;
  }

  const BLOCK target_block = stash->stash[index];
  const uint32_t last_block_index = stash->num_blocks - 1;
  const uint32_t block_to_swap_index = last_block_index;
  stash->stash[index] = stash->stash[block_to_swap_index];
  stash->stash[last_block_index] = target_block;
  stash->num_blocks--;
}

void STASH_add_block(STASH *stash, const BLOCK *block) {
  stash->stash[stash->num_blocks++] = *block;
}
