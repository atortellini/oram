#include "roram.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "constants.h"
#include "suboram.h"

void RORAM_init(RORAM *oram) {
  for (size_t i = 0; i < NUM_SUBORAMS; i++) {
    SUBORAM *current_suboram = &oram->subORAMs[i];
    SUBORAM_init(current_suboram, i);
  }
}

void RORAM_access() {}

