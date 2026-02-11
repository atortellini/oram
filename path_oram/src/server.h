#ifndef SERVER_H
#define SERVER_H

#include "types.h"

EncryptedBucket *Server_ReadBucket(uint32_t bucket_id);

void Server_WriteBucket(uint32_t bucket_id, EncryptedBucket *bucket);


#endif