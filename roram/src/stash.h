#ifndef STASH_H
#define STASH_H

#include "types.h"
#include <stdint.h>

typedef struct stash_t {
  size_t num_blocks;
  BLOCK *stash;
} STASH;

void STASH_init(STASH *stash);

void STASH_remove_block(STASH *stash, const uint32_t index);

void STASH_add_block(STASH *stash, const BLOCK *block);

#endif
