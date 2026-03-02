#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "../../parameters/parameters.h"
#include "constants.h"

typedef struct block_t {
  uint32_t block_address;
  uint32_t path_tags[NUM_SUBORAMS];
  uint8_t data[NUM_DATA_BYTES_PER_BLOCK];
} BLOCK;

typedef struct bucket_t {
  BLOCK blocks[NUM_BLOCKS_PER_BUCKET];
} BUCKET;

typedef struct encrypted_block_t {
  uint8_t block[NUM_BYTES_PER_ENCRYPTED_BLOCK];
} ENCRYPTED_BLOCK;

typedef struct encrypted_bucket_t {
  ENCRYPTED_BLOCK blocks[NUM_BLOCKS_PER_BUCKET];
} ENCRYPTED_BUCKET;

typedef struct position_map_t {
  uint32_t *map; // this will be a pointer to an array of size N/(2^i)
} POSITION_MAP;

typedef struct stash_t {
  size_t num_blocks;
  BLOCK stash[STASH_SIZE];
} STASH;




#endif
