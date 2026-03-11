#ifndef SERVER_H
#define SERVER_H

#include "types.h"

void SERVER_init(void);

void SERVER_read_bucket(uint32_t bucket_id, ENCRYPTED_BUCKET *bucket_out);

void SERVER_write_bucket(uint32_t bucket_id, ENCRYPTED_BUCKET *bucket);

#endif
