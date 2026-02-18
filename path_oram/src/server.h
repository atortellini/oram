#ifndef SERVER_H
#define SERVER_H

#include "types.h"

EncryptedBucket *SERVER_read_bucket(uint32_t bucket_id);

void SERVER_write_bucket(uint32_t bucket_id, EncryptedBucket *bucket);

#endif
