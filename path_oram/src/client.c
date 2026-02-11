#include "client.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crypto.h"
#include "globals.h"
#include "server.h"
#include "stash.h"
#include "types.h"

typedef enum {
  READ,
  WRITE,
} OP;

static uint32_t bucket_id_at_level_from_path(uint32_t path_id, uint32_t level);
static void Client_WriteBucket(uint32_t bucket_id, Block blocks[],
                               size_t num_blocks);
static size_t Client_ReadBucket(uint32_t bucket_id, Block blocks_found[]);
static bool blockIsDummyBlock(Block *block);
static Block *find_block_in_stash(uint32_t target_block_id);
static void eviction(uint32_t path_id);

static uint32_t bucket_id_at_level_from_path(uint32_t path_id, uint32_t level) {
  const uint32_t num_buckets_above_level = (1 << level) - 1;
  const uint32_t height_of_subtree_rooted_at_level = HEIGHT_OF_TREE - level;
  const uint32_t num_leaves_per_subtree = 1
                                          << height_of_subtree_rooted_at_level;
  const uint32_t node_number_at_level_in_path =
      path_id / num_leaves_per_subtree;
  const uint32_t bucket_id =
      num_buckets_above_level + node_number_at_level_in_path;

  return bucket_id;
}

static void Client_WriteBucket(uint32_t bucket_id, Block blocks[],
                               size_t num_blocks) {
  const bool bucketIsOverfilled = num_blocks > NUM_BLOCKS_PER_BUCKET;
  if (bucketIsOverfilled) {
    fpritnf(stderr,
            "Client is attempting to write a bucket with too many blocks");
    exit(1);
  }
  Block blocks_to_encrypt[NUM_BLOCKS_PER_BUCKET];
  for (size_t i = 0; i < num_blocks; i++) {
    blocks_to_encrypt[i] = blocks[i];
  }

  const bool bucketIsUnderfilled = num_blocks < NUM_BLOCKS_PER_BUCKET;
  if (bucketIsUnderfilled) {
    size_t dummies_to_make = NUM_BLOCKS_PER_BUCKET - num_blocks;
    size_t dummy_index = num_blocks;
    while (dummies_to_make) {
      blocks_to_encrypt[dummy_index] = DUMMY_BLOCK;
      dummies_to_make--;
      dummy_index++;
    }
  }
  EncryptedBucket encrypted_bucket;

  for (size_t block_index = 0; block_index < NUM_BLOCKS_PER_BUCKET;
       block_index++) {
    const Block *block_to_encrypt = &blocks_to_encrypt[block_index];
    EncryptedBlock *encrypted_block_target =
        &encrypted_bucket.blocks[block_index];
    encrypt_block(block_to_encrypt, encrypted_block_target);
  }
  Server_WriteBucket(bucket_id, &encrypted_bucket);
}

// Can either have this implementation decrypt the entire bucket and return all
// of the decrypted blocks or can decrypt block by block
static size_t Client_ReadBucket(uint32_t bucket_id, Block blocks_found[]) {
  EncryptedBucket *encrypted_bucket = Server_ReadBucket(bucket_id);
  //   Bucket decrypted_bucket;
  //   decrypt_bucket(encrypted_bucket, &decrypted_bucket);

  size_t num_blocks_found = 0;
  for (size_t i = 0; i < NUM_BLOCKS_PER_BUCKET; i++) {
    const EncryptedBlock *encrypted_block = &encrypted_bucket->blocks[i];
    Block decrypted_block;
    decrypt_block(encrypted_block, &decrypted_block);
    if (blockIsDummyBlock(&decrypted_block)) {
      continue;
    } else {
      blocks_found[num_blocks_found++] = decrypted_block;
    }
  }

  return num_blocks_found;
}

static bool blockIsDummyBlock(Block *block) {
  return block->block_id = DUMMY_BLOCK.block_id;
}

static Block *find_block_in_stash(uint32_t target_block_id) {
  for (size_t i = 0; i < client_stash.num_blocks; i++) {
    const Block *current_block = &client_stash.stash[i];
    if (current_block->block_id == target_block_id) {
      return current_block;
    }
  }
  return NULL;
}

static void eviction(uint32_t path_id) {
  for (ssize_t level = HEIGHT_OF_TREE; level >= 0; level--) {
    uint32_t indexes_of_blocks_to_add[NUM_BLOCKS_PER_BUCKET];
    size_t number_of_blocks_to_add = 0;

    const uint32_t evicted_bucket_id =
        bucket_id_at_level_from_path(path_id, level);

    for (size_t i = 0; i < client_stash.num_blocks; i++) {
      const Block *current_block = &client_stash.stash[i];
      const uint32_t bucket_id_to_contain_current_block =
          bucket_id_at_level_from_path(current_block->path_id, level);
      const bool blockPathIntersectsEvictedPath =
          evicted_bucket_id == bucket_id_to_contain_current_block;

      if (blockPathIntersectsEvictedPath) {
        indexes_of_blocks_to_add[number_of_blocks_to_add++] = i;
        const bool bucketAtLevelIsFull =
            number_of_blocks_to_add == NUM_BLOCKS_PER_BUCKET;
        if (bucketAtLevelIsFull) {
          break;
        }
      }
    }

    Block blocks_to_add[NUM_BLOCKS_PER_BUCKET];

    for (size_t i = 0; i < number_of_blocks_to_add; i++) {
      const uint32_t index_of_block = indexes_of_blocks_to_add[i];
      blocks_to_add[i] = client_stash.stash[index_of_block];
      Stash_RemoveBlock(index_of_block);
    }

    Client_WriteBucket(evicted_bucket_id, blocks_to_add,
                       number_of_blocks_to_add);
  }
}

void access(uint32_t target_block_id, OP operation, uint8_t new_data[],
            uint8_t old_data[]) {
  const uint32_t target_path_id = client_position_map[target_block_id];
  const uint32_t new_path_id = uniform_random(0, NUM_TOTAL_REAL_BLOCKS);
  client_position_map[target_block_id] = new_path_id;

  for (size_t level = 0; level < HEIGHT_OF_TREE; level++) {
    Block blocks_found[NUM_BLOCKS_PER_BUCKET];
    const target_bucket_id =
        bucket_id_at_level_from_path(target_path_id, level);
    const size_t num_real_blocks =
        Client_ReadBucket(target_bucket_id, blocks_found);

    for (size_t i = 0; i < num_real_blocks; i++) {
      Stash_AddBlock(&blocks_found[i]);
    }
  }
  // Need to think about how the old data is returned to the caller, have to
  // either malloc it or memcpy to an argument passed by the caller
  Block *target_block = find_block_in_stash(target_block_id);
  const bool blockNotFound = target_block == NULL;
  const bool operationIsWrite = operation == WRITE;
  const bool firstTimeAccess = blockNotFound && operationIsWrite;

  if (blockNotFound) {
    if (firstTimeAccess) {
      fprintf(stdout, "First time accessing block id: %d, adding to stash...",
              target_block_id);
      Block new_block = {target_block_id, new_path_id, new_data};
      Stash_AddBlock(&new_block);
      memset(old_data, 0, NUM_BYTES_PER_BLOCK);
    } else {
      fprintf(stderr,
              "Invariant violated: Attempted read on block id: %d that was not "
              "found in stash...",
              target_block_id);
      exit(1);
    }
  } else {
    memcpy(old_data, target_block->data, NUM_BYTES_PER_BLOCK);
    target_block->path_id = new_path_id;
    if (operationIsWrite) {
      memcpy(target_block->data, new_data, NUM_BYTES_PER_BLOCK);
    }
  }

  eviction(target_path_id);
}