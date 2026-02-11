#ifndef STASH_H
#define STASH_H

#include <stdint.h>
#include "types.h"

void Stash_RemoveBlock(uint32_t index);

void Stash_AddBlock(Block *block);

#endif