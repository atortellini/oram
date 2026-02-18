#ifndef TYPES_H
#define TYPES_H

#include "constants.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t block_id;
  uint32_t path_id;
  uint8_t data[NUM_BYTES_PER_BLOCK];
} Block;

typedef struct {
  Block blocks[NUM_BLOCKS_PER_BUCKET];
} Bucket;

typedef struct {
  size_t num_blocks;
  Block stash[STASH_SIZE];
} Stash;

typedef struct {
  uint8_t data[NUM_BYTES_PER_ENCRYPTED_BLOCK];
} EncryptedBlock;

typedef struct {
  EncryptedBlock blocks[NUM_BLOCKS_PER_BUCKET];
} EncryptedBucket;

#endif
