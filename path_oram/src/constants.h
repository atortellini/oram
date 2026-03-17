#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "../../parameters/parameters.h"

#define NUM_TOTAL_REAL_BLOCKS (1U << NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2)
#define HEIGHT_OF_TREE NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2
#define NUM_TOTAL_BUCKETS ((1U << (HEIGHT_OF_TREE + 1)) - 1)
#define STASH_SIZE (303U + NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2)
// 303 was taken from Section 6.4 Fig. 5 as the required max size for a 256-bit
// security parameter excluding the path initially retrieved from an access.

#define NUM_BYTES_PER_BLOCK_AND_METADATA sizeof(Block)

#define AES_KEY_SIZE_BYTES 32U
#define AES_IV_SIZE_BYTES 12U
#define AES_TAG_SIZE_BYTES 16U
#define NUM_BYTES_PER_ENCRYPTED_BLOCK                                          \
  (AES_IV_SIZE_BYTES + sizeof(Block) + AES_TAG_SIZE_BYTES)

#endif
