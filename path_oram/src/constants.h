#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>

#define NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2 20UL
#define NUM_BLOCKS_PER_BUCKET 4UL
#define NUM_BYTES_PER_BLOCK 8UL

#define NUM_TOTAL_REAL_BLOCKS (1 << NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2)
#define HEIGHT_OF_TREE NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2
#define NUM_TOTAL_BUCKETS ((1 << (HEIGHT_OF_TREE + 1)) - 1)
#define STASH_SIZE (303UL + NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2)
// 303 was taken from Section 6.4 Fig. 5 as the required max size for a 256-bit
// security parameter excluding the path initially retrieved from an access.

#define NUM_BYTES_FOR_PATH_ID 4U
#define NUM_BYTES_FOR_BLOCK_ID 4U
#define NUM_BYTES_PER_ENCRYPTED_BLOCK                                          \
  (NUM_BYTES_FOR_PATH_ID + NUM_BYTES_FOR_BLOCK_ID + NUM_BYTES_PER_BLOCK)

#define NUM_EXPERIMENT_ACCESSES (1 << 18UL)

#endif