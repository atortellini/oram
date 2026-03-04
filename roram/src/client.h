#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>

#include "roram.h"

void CLIENT_init(void);

void CLIENT_access(const uint32_t address, const uint32_t range_size_power_of_2,
                   const OPERATION op, const uint8_t *new_data,
                   uint8_t *old_data);

#endif
