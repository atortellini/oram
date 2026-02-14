#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>

const uint32_t NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2 = 20UL;
const uint32_t NUM_BLOCKS_PER_BUCKET = 4UL;
const uint32_t NUM_BYTES_PER_BLOCK = 8UL;

const uint32_t NUM_TOTAL_REAL_BLOCKS = 1 << NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2;
const uint32_t HEIGHT_OF_TREE = NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2;
const uint32_t NUM_TOTAL_BUCKETS = (1 << (HEIGHT_OF_TREE + 1)) - 1;
const uint32_t STASH_SIZE = 303UL + NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2;
// 303 was taken from Section 6.4 Fig. 5 as the required max size for a 256-bit
// security parameter excluding the path initially retrieved from an access.

const uint32_t NUM_BYTES_PER_ENCRYPTED_BLOCK = NUM_BYTES_PER_BLOCK;

const uint32_t NUM_EXPERIMENT_ACCESSES = 1 << 18UL;

#endif