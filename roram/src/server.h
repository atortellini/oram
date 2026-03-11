#ifndef SERVER_H
#define SERVER_H

#include "types.h"

void SERVER_init(void);

void SERVER_read_range(uint32_t bucket_id, uint32_t num_buckets,
                       ENCRYPTED_BUCKET *buckets_out);

void SERVER_write_range(uint32_t bucket_id, uint32_t num_buckets,
                        ENCRYPTED_BUCKET *buckets);

#endif
