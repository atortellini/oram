#include "client.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crypto.h"
#include "globals.h"
#include "rand.h"
#include "server.h"
#include "stash.h"
#include "types.h"

static void initialize_dummy_block(void);
static void fill_position_map_with_random_paths(void);
static void fill_server_with_dummies(void);

static void fetch_path_buckets_into_stash(uint32_t target_path_id);
static Block *find_block_in_stash(uint32_t target_block_id);
static void evict_path_from_stash(uint32_t path_id);

static uint32_t get_bucket_id(uint32_t path_id, uint32_t level);
static size_t read_bucket_from_server(uint32_t bucket_id, Block blocks_found[]);

static bool is_dummy_block(Block *block);

static void write_bucket_to_server(uint32_t bucket_id, Block blocks[],
                                   size_t num_blocks);

void CLIENT_setup() {
  initialize_dummy_block();
  fill_position_map_with_random_paths();
  fill_server_with_dummies();
}

void CLIENT_access(uint32_t target_block_id, OP_t operation, uint8_t new_data[],
                   uint8_t old_data[]) {
  const uint32_t target_path_id = client_position_map[target_block_id];
  const uint32_t new_path_id = uniform_random(NUM_TOTAL_REAL_BLOCKS);
  client_position_map[target_block_id] = new_path_id;

  fetch_path_buckets_into_stash(target_path_id);

  Block *target_block = find_block_in_stash(target_block_id);
  const bool blockNotFound = target_block == NULL;
  const bool operationIsWrite = operation == WRITE;
  const bool wantsOldData = old_data != NULL;

  if (blockNotFound) {
    if (operationIsWrite) {
      fprintf(stdout, "First time accessing block id: %d, adding to stash...",
              target_block_id);

      Block new_block;
      new_block.block_id = target_block_id;
      new_block.path_id = new_path_id;
      memcpy(new_block.data, new_data, NUM_BYTES_PER_BLOCK);

      STASH_add_block(&new_block);
      if (wantsOldData) {
        memset(old_data, 0, NUM_BYTES_PER_BLOCK);
      }
    } else {
      fprintf(stderr,
              "Invariant violated: Attempted read on block id: %d that was not "
              "found in stash...",
              target_block_id);
      exit(1);
    }
  } else {
    if (wantsOldData) {
      memcpy(old_data, target_block->data, NUM_BYTES_PER_BLOCK);
    }
    target_block->path_id = new_path_id;
    if (operationIsWrite) {
      memcpy(target_block->data, new_data, NUM_BYTES_PER_BLOCK);
    }
  }

  evict_path_from_stash(target_path_id);
}

static void initialize_dummy_block(void) {
  DUMMY_BLOCK.block_id = UINT32_MAX;
  DUMMY_BLOCK.path_id = 0;
  generate_random_bytes(DUMMY_BLOCK.data, NUM_BYTES_PER_BLOCK);
}

static void fill_position_map_with_random_paths(void) {
  for (size_t i = 0; i < NUM_TOTAL_REAL_BLOCKS; i++) {
    const uint32_t random_path = uniform_random(NUM_TOTAL_REAL_BLOCKS);
    client_position_map[i] = random_path;
  }
}

static void fill_server_with_dummies(void) {
  for (size_t i = 0; i < NUM_TOTAL_BUCKETS; i++) {
    write_bucket_to_server(i, NULL, 0);
  }
}

static void fetch_path_buckets_into_stash(uint32_t target_path_id) {
  for (size_t level = 0; level <= HEIGHT_OF_TREE; level++) {
    const uint32_t target_bucket_id = get_bucket_id(target_path_id, level);

    Block blocks_found[NUM_BLOCKS_PER_BUCKET];
    const size_t num_real_blocks =
        read_bucket_from_server(target_bucket_id, blocks_found);

    for (size_t i = 0; i < num_real_blocks; i++) {
      STASH_add_block(&blocks_found[i]);
    }
  }
}

static Block *find_block_in_stash(uint32_t target_block_id) {
  for (size_t i = 0; i < client_stash.num_blocks; i++) {
    Block *current_block = &client_stash.stash[i];
    if (current_block->block_id == target_block_id) {
      return current_block;
    }
  }
  return NULL;
}

static void evict_path_from_stash(uint32_t path_id) {
  for (int level = HEIGHT_OF_TREE; level >= 0; level--) {

    Block bucket_buffer[NUM_BLOCKS_PER_BUCKET];
    size_t buffer_count = 0;

    const uint32_t evicted_bucket_id = get_bucket_id(path_id, level);

    for (size_t i = 0; i < client_stash.num_blocks;) {
      const Block *candidate_block = &client_stash.stash[i];
      const uint32_t candidate_bucket_at_level =
          get_bucket_id(candidate_block->path_id, level);
      const bool candidateBlockPathIntersectsEvictedPath =
          evicted_bucket_id == candidate_bucket_at_level;

      if (candidateBlockPathIntersectsEvictedPath) {
        bucket_buffer[buffer_count++] = *candidate_block;
        STASH_remove_block(i);
        const bool bucketAtLevelIsFull = buffer_count == NUM_BLOCKS_PER_BUCKET;
        if (bucketAtLevelIsFull) {
          break;
        }
      } else {
        i++;
      }
    }
    write_bucket_to_server(evicted_bucket_id, bucket_buffer, buffer_count);
  }
}

static uint32_t get_bucket_id(uint32_t path_id, uint32_t level) {
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

// Can either have this implementation decrypt the entire bucket and return all
// of the decrypted blocks or can decrypt block by block
static size_t read_bucket_from_server(uint32_t bucket_id,
                                      Block blocks_found[]) {

  EncryptedBucket *encrypted_bucket = SERVER_read_bucket(bucket_id);

  size_t num_blocks_found = 0;
  for (size_t i = 0; i < NUM_BLOCKS_PER_BUCKET; i++) {
    const EncryptedBlock *encrypted_block = &encrypted_bucket->blocks[i];
    Block decrypted_block;
    decrypt_block(encrypted_block, &decrypted_block);
    if (is_dummy_block(&decrypted_block)) {
      continue;
    } else {
      blocks_found[num_blocks_found++] = decrypted_block;
    }
  }

  return num_blocks_found;
}

static bool is_dummy_block(Block *block) {
  return block->block_id == DUMMY_BLOCK.block_id;
}

static void write_bucket_to_server(uint32_t bucket_id, Block blocks[],
                                   size_t num_blocks) {
  const bool bucketIsOverfilled = num_blocks > NUM_BLOCKS_PER_BUCKET;
  if (bucketIsOverfilled) {
    fprintf(stderr,
            "Client is attempting to write a bucket with too many blocks");
    exit(1);
  }
  Block plaintext_blocks[NUM_BLOCKS_PER_BUCKET];
  for (size_t i = 0; i < num_blocks; i++) {
    plaintext_blocks[i] = blocks[i];
  }

  const bool bucketIsUnderfilled = num_blocks < NUM_BLOCKS_PER_BUCKET;
  if (bucketIsUnderfilled) {
    for (size_t i = num_blocks; i < NUM_BLOCKS_PER_BUCKET; i++) {
      plaintext_blocks[i] = DUMMY_BLOCK;
    }
  }

  EncryptedBucket encrypted_bucket;

  for (size_t i = 0; i < NUM_BLOCKS_PER_BUCKET; i++) {
    const Block *block_to_encrypt = &plaintext_blocks[i];
    EncryptedBlock *encrypted_block_output = &encrypted_bucket.blocks[i];
    encrypt_block(block_to_encrypt, encrypted_block_output);
  }
  SERVER_write_bucket(bucket_id, &encrypted_bucket);
}
