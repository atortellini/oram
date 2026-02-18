#ifndef STASH_H
#define STASH_H

#include <stdint.h>
#include "types.h"

void STASH_remove_block(uint32_t index);

void STASH_add_block(Block *block);

#endif
