#include "globals.h"
#include "types.h"

#include <stdint.h>

Stash client_stash;
uint32_t client_position_map[NUM_TOTAL_REAL_BLOCKS];
Block DUMMY_BLOCK;

EncryptedBucket server_storage[NUM_TOTAL_BUCKETS];

uint8_t synthetic_dataset[NUM_TOTAL_REAL_BLOCKS * NUM_BYTES_PER_BLOCK];
uint32_t access_sequence_buffer[NUM_EXPERIMENT_ACCESSES];