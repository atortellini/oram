#include "client.h"

#include <assert.h>
#include <stdint.h>

#include "constants.h"
#include "crypto.h"
#include "globals.h"
#include "roram.h"
#include "server.h"
#include "types.h"

void CLIENT_init(void) {
  CRYPTO_init();
  RORAM_init(&rORAM);

  initialize_dummy_block();
  fill_server_with_dummies();
}

static void initialize_dummy_block(void) {
  DUMMY_BLOCK.block_address = UINT32_MAX;
  memset(DUMMY_BLOCK.path_tags, 0, NUM_SUBORAMS);
  generate_random_bytes(DUMMY_BLOCK.data, NUM_DATA_BYTES_PER_BLOCK);
}

static void fill_server_with_dummies(void) {
  for (uint32_t bucket_id = 0; bucket_id < NUM_TOTAL_BUCKETS; bucket_id++) {
    ENCRYPTED_BUCKET encrypted_dummy_bucket;
    for (size_t i = 0; i < NUM_BLOCKS_PER_BUCKET; i++) {
      CRYPTO_encrypt_block(&DUMMY_BLOCK, &encrypted_dummy_bucket.blocks[i]);
    }
    SERVER_write_bucket(bucket_id, &encrypted_dummy_bucket);
  }
}

void CLIENT_access(const uint32_t address, const uint32_t range_size_power_of_2,
                   const OPERATION op, const uint8_t *new_data,
                   uint8_t *old_data) {
  if (op == WRITE)
    assert(new_data);

  RORAM_access(&rORAM, address, range_size_power_of_2, op, new_data, old_data);
}
