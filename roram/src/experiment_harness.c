#include "experiment_harness.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../parameters/parameters.h"
#include "client.h"
#include "constants.h"
#include "globals.h"
#include "rand.h"
#include "timer.h"

#define min(X, Y) ((X) < (Y) ? (X) : (Y))

typedef struct access_t {
  uint32_t base_address;
} ACCESS;

static uint8_t *synthetic_dataset;
static ACCESS access_sequence_buffer[NUM_EXPERIMENT_ACCESSES];

static void prepare_synthetic_data(void);
static void generate_synthetic_data(void);
static void insert_synthetic_data_into_server(void);

static void generate_range_query_access_sequence(void);
static void perform_access(const uint32_t access_index);

void perform_experiments(void) {
  prepare_synthetic_data();
  generate_range_query_access_sequence();

  struct timespec elapsed_time;

  for (size_t access_index = 0; access_index < NUM_EXPERIMENT_ACCESSES;
       access_index++) {
    TIME_VOID_FUNC(&elapsed_time, perform_access(access_index));
    fprintf(stdout, "Access %lu took: %ld s %ld ns\n", access_index,
            elapsed_time.tv_sec, elapsed_time.tv_nsec);
  }
}

static void prepare_synthetic_data(void) {
  synthetic_dataset = (uint8_t *)malloc(
      sizeof(uint8_t) * NUM_TOTAL_REAL_BLOCKS * NUM_DATA_BYTES_PER_BLOCK);
  assert(synthetic_dataset);

  struct timespec e;
  TIME_VOID_FUNC(&e, generate_synthetic_data());
  fprintf(stdout, "Finished generating synthetic data in: %ld s %ld ns\n",
          e.tv_sec, e.tv_nsec);
  TIME_VOID_FUNC(&e, insert_synthetic_data_into_server());
  fprintf(stdout, "Finished inserting synthetic data in: %ld s %ld ns\n",
          e.tv_sec, e.tv_nsec);
}

static void generate_synthetic_data(void) {
  generate_random_bytes(synthetic_dataset,
                        NUM_TOTAL_REAL_BLOCKS * NUM_DATA_BYTES_PER_BLOCK);
}

static void insert_synthetic_data_into_server(void) {
  for (size_t i = 0; i < NUM_TOTAL_REAL_BLOCKS; i++) {
    const uint32_t block_data_index = i * NUM_DATA_BYTES_PER_BLOCK;
    CLIENT_access(i, 0, WRITE, &synthetic_dataset[block_data_index], NULL);
  }
}

static void generate_range_query_access_sequence(void) {
  const uint32_t last_valid_base_address =
      NUM_TOTAL_REAL_BLOCKS - QUERY_RANGE_SIZE;
  const uint32_t base_address_upper_bound = last_valid_base_address + 1;

  for (size_t access_index = 0; access_index < NUM_EXPERIMENT_ACCESSES;
       access_index++) {
    access_sequence_buffer[access_index].base_address =
        uniform_random(base_address_upper_bound);
  }
}

static void perform_access(const uint32_t access_index) {
  const uint32_t query_base_address =
      access_sequence_buffer[access_index].base_address;
  uint8_t returned_data[NUM_DATA_BYTES_PER_BLOCK * QUERY_RANGE_SIZE];

  const uint8_t *expected_data =
      &synthetic_dataset[query_base_address * NUM_DATA_BYTES_PER_BLOCK];
  CLIENT_access(query_base_address, QUERY_RANGE_SIZE_AS_POWER_OF_2, READ, NULL,
                returned_data);

  if (memcmp(expected_data, returned_data,
             NUM_DATA_BYTES_PER_BLOCK * QUERY_RANGE_SIZE) != 0) {
    fprintf(stderr,
            "Comparison of data for block range starting at: %u differs from "
            "expected.\n",
            query_base_address);
    exit(EXIT_FAILURE);
  }
}
