#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "../../parameters/parameters.h"

#define NUM_SUBORAMS (QUERY_RANGE_SIZE_AS_POWER_OF_2 + 1U)

#define NUM_TOTAL_REAL_BLOCKS (1U << NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2)
#define HEIGHT_OF_TREE NUM_REAL_DATA_BLOCKS_AS_POWER_OF_2
#define NUM_BUCKETS_PER_ORAM ((1U << (HEIGHT_OF_TREE + 1U)) - 1)
#define NUM_TOTAL_BUCKETS (NUM_BUCKETS_PER_ORAM * NUM_SUBORAMS)
#define STASH_SIZE                                                             \
  ((303U + HEIGHT_OF_TREE) *                                                   \
   QUERY_RANGE_SIZE) // FIXME: FIX THE STASH SIZE IDK WHAT IT SHOULD BE

#define AES_KEY_SIZE_BYTES 32U
#define AES_IV_SIZE_BYTES 12U
#define AES_TAG_SIZE_BYTES 16U
#define NUM_BYTES_PER_ENCRYPTED_BLOCK                                          \
  (AES_IV_SIZE_BYTES + sizeof(BLOCK) + AES_TAG_SIZE_BYTES)

#endif
