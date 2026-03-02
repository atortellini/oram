#ifndef RAND_HELPERS_H
#define RAND_HELPERS_H

#include <stdbool.h>
#include <stdint.h>

bool coin_flip(void);

uint32_t uniform_random(const uint32_t upper_bound);

void generate_random_bytes(uint8_t *dst, const uint32_t num_bytes);

#endif
