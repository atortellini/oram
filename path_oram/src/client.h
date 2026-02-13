#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>

typedef enum { READ, WRITE } OP_t;

void client_setup(void);

void client_access(uint32_t target_block_id, OP_t operation, uint8_t new_data[], uint8_t old_data[]);

#endif