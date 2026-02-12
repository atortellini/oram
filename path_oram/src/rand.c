#include "rand.h"

#include <stdbool.h>
#include <stdlib.h>

bool coin_flip(void) { return arc4random() & 01; }

uint32_t uniform_random(uint32_t upper_bound) {
  return arc4random_uniform(upper_bound);
}

void generate_random_bytes(uint8_t *dst, const uint32_t num_bytes) {
  arc4random_buf(dst, num_bytes);
}
