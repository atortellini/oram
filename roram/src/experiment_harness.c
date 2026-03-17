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

typedef struct access_buffer_t {
  ACCESS accesses[NUM_EXPERIMENT_ACCESSES];
  uint32_t range_size_po2;
} ACCESS_BUFFER;

static uint8_t *synthetic_dataset;
static ACCESS_BUFFER access_sequence_buffer[QUERY_RANGE_SIZE_AS_POWER_OF_2 + 1];

static void prepare_synthetic_data(void);
static void generate_synthetic_data(void);
static void insert_synthetic_data_into_server(void);

static void initialize_access_sequence_buffer(void);
static void generate_range_query_access_sequence(ACCESS *accesses,
                                                 const size_t num_accesses,
                                                 const uint32_t range_size);
static void perform_access(const ACCESS *access, const uint32_t range_size_po2);

void perform_experiments(void) {
  prepare_synthetic_data();

  initialize_access_sequence_buffer();

  struct timespec elapsed_time;
  for (size_t access_buff = 0;
       access_buff < (QUERY_RANGE_SIZE_AS_POWER_OF_2 + 1); access_buff++) {
    const ACCESS_BUFFER *curr_buff = &access_sequence_buffer[access_buff];
    const uint32_t curr_range_size = 1U << curr_buff->range_size_po2;

    fprintf(stdout, "Performing accesses for range %u...\n", curr_range_size);

    for (size_t i = 0; i < NUM_EXPERIMENT_ACCESSES; i++) {
      TIME_VOID_FUNC(&elapsed_time, perform_access(&curr_buff->accesses[i],
                                                   curr_buff->range_size_po2));
      fprintf(stdout, "Access %lu took: %ld s %ld ns\n", i, elapsed_time.tv_sec,
              elapsed_time.tv_nsec);
    }
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

static void initialize_access_sequence_buffer(void) {
  for (size_t i = 0; i < (QUERY_RANGE_SIZE_AS_POWER_OF_2 + 1); i++) {
    ACCESS_BUFFER *curr_access_buffer = &access_sequence_buffer[i];
    const uint32_t curr_range_size = 1U << i;

    curr_access_buffer->range_size_po2 = i;

    generate_range_query_access_sequence(
        curr_access_buffer->accesses, NUM_EXPERIMENT_ACCESSES, curr_range_size);
  }
}

static void generate_range_query_access_sequence(ACCESS *accesses,
                                                 const size_t num_accesses,
                                                 const uint32_t range_size) {
  const uint32_t last_valid_base_address = NUM_TOTAL_REAL_BLOCKS - range_size;
  const uint32_t base_address_upper_bound = last_valid_base_address + 1;

  for (size_t access_index = 0; access_index < num_accesses; access_index++) {
    accesses[access_index].base_address =
        uniform_random(base_address_upper_bound);
  }
}

static void perform_access(const ACCESS *access,
                           const uint32_t range_size_po2) {
  const uint32_t query_base_address = access->base_address;
  const uint32_t range_size = 1U << range_size_po2;
  uint8_t returned_data[NUM_DATA_BYTES_PER_BLOCK * range_size];

  const uint8_t *expected_data =
      &synthetic_dataset[query_base_address * NUM_DATA_BYTES_PER_BLOCK];
  CLIENT_access(query_base_address, range_size_po2, READ, NULL, returned_data);

  if (memcmp(expected_data, returned_data,
             NUM_DATA_BYTES_PER_BLOCK * range_size) != 0) {
    fprintf(stderr,
            "Comparison of data for block range starting at: %u differs from "
            "expected.\n",
            query_base_address);
    exit(EXIT_FAILURE);
  }
}
