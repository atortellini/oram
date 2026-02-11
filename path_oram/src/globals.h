#ifndef GLOBALS_H
#define GLOBALS_H

#include "types.h"
#include <stdint.h>

// CLIENT GLOBALS
extern Stash client_stash;
extern uint32_t client_position_map[NUM_TOTAL_REAL_BLOCKS];
extern Block DUMMY_BLOCK;

// SERVER GLOBALS
extern EncryptedBucket server_storage[NUM_TOTAL_BLOCKS];


#endif