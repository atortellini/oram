#include "server.h"

#include "globals.h"
#include "types.h"

EncryptedBucket *SERVER_read_bucket(uint32_t bucket_id) {
  return &server_storage[bucket_id];
}

void SERVER_write_bucket(uint32_t bucket_id, EncryptedBucket *bucket) {
    server_storage[bucket_id] = *bucket;
}
