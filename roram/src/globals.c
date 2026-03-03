#include "globals.h"

#include <stdint.h>

#include "roram.h"
#include "types.h"

BLOCK DUMMY_BLOCK;

RORAM rORAM;
uint32_t EVICTION_COUNTER;
ENCRYPTED_BUCKET server_storage[NUM_TOTAL_BUCKETS];
