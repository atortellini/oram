#include "server.h"

#include "globals.h"
#include "types.h"

EncryptedBucket *server_read_bucket(uint32_t bucket_id) {
  return &server_storage[bucket_id];
}

void server_write_bucket(uint32_t bucket_id, EncryptedBucket *bucket) {
    server_storage[bucket_id] = *bucket;
}