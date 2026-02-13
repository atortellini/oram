#ifndef SERVER_H
#define SERVER_H

#include "types.h"

EncryptedBucket *server_read_bucket(uint32_t bucket_id);

void server_write_bucket(uint32_t bucket_id, EncryptedBucket *bucket);

#endif