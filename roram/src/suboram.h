#ifndef SUBORAM_H
#define SUBORAM_H

#include "types.h"

typedef struct suboram_t {
  STASH stash;
  POSITION_MAP pm;
  uint32_t oram_index;
} SUBORAM;

#endif