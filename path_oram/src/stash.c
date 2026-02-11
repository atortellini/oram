#include "stash.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "types.h"
#include "globals.h"

void Stash_RemoveBlock(uint32_t index) {
  const size_t num_blocks = client_stash.num_blocks;
  const bool stashIsEmpty = num_blocks == 0;
  if (stashIsEmpty) {
    fpritnf(stderr, "Attempt to remove block from an empty client stash");
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
    client_stash.num_blocks--;
    return;
  }

  const Block target_block = client_stash.stash[index];
  const uint32_t last_block_index = client_stash.num_blocks - 1;
  const uint32_t block_to_swap_index = last_block_index;
  client_stash.stash[index] = client_stash.stash[block_to_swap_index];
  client_stash.stash[last_block_index] = target_block;
  client_stash.num_blocks--;
}

void Stash_AddBlock(Block *block) {
  client_stash.stash[client_stash.num_blocks++] = *block;
}
