#include "server.h"

#include "globals.h"
#include "types.h"

EncryptedBucket *Server_ReadBucket(uint32_t bucket_id) {
  return &server_storage[bucket_id];
}

void Server_WriteBucket(uint32_t bucket_id, EncryptedBucket *bucket) {
    server_storage[bucket_id] = *bucket;
}