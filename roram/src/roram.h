#ifndef RORAM_H
#define RORAM_H

#include "suboram.h"

typedef struct roram_t {
  SUBORAM subORAMs[NUM_SUBORAMS];
} RORAM;

void RORAM_init(RORAM *oram);

#endif
