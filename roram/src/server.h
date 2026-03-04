#ifndef SERVER_H
#define SERVER_H

#include "types.h"

ENCRYPTED_BUCKET *SERVER_read_bucket(uint32_t bucket_id);

void SERVER_write_bucket(uint32_t bucket_id, ENCRYPTED_BUCKET *bucket);

#endif
