#ifndef SUBORAM_H
#define SUBORAM_H

#include <stdbool.h>
#include <stdint.h>

#include "types.h"

typedef struct suboram_t {
  STASH stash;
  POSITION_MAP pm;
  uint32_t oram_index;
} SUBORAM;

void SUBORAM_init(SUBORAM *oram, const size_t suboram_index);

uint32_t SUBORAM_read_range(SUBORAM *oram, const uint32_t address,
                            BLOCK *results, bool *addresses_found_map);

void SUBORAM_batch_evict(SUBORAM *oram, const size_t num_evictions);

#endif
