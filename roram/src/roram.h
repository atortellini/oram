#ifndef RORAM_H
#define RORAM_H

#include <stdint.h>

#include "suboram.h"

typedef struct roram_t {
  SUBORAM subORAMs[NUM_SUBORAMS];
} RORAM;

void RORAM_init(RORAM *roram);

typedef enum operation_t { READ, WRITE } OPERATION;

// The paper specifies that non-power of two ranges are supported by just
// choosing the smallest, larger power of 2 range to use. However, since the
// experiment harness is only going to use powers of two for the range the
// access will be implemented with the assupmtion that the range is a power
// of 2.
void RORAM_access(RORAM *roram, const uint32_t address,
                  const uint32_t range_size_power_of_two, const OPERATION op,
                  const uint8_t *new_data, const uint8_t *old_data);

#endif
