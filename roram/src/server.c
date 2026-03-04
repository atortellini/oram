#include "server.h"

#include "globals.h"
#include "types.h"

ENCRYPTED_BUCKET *SERVER_read_bucket(uint32_t bucket_id) {
  return &server_storage[bucket_id];
}

void SERVER_write_bucket(uint32_t bucket_id, ENCRYPTED_BUCKET *bucket) {
  server_storage[bucket_id] = *bucket;
}
