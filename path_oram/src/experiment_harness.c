#include "experiment_harness.h"

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

typedef struct {
  uint32_t queries[QUERY_RANGE_SIZE];
} Access;

static uint8_t
    synthetic_dataset[NUM_TOTAL_REAL_BLOCKS * NUM_DATA_BYTES_PER_BLOCK];
static Access access_sequence_buffer[NUM_EXPERIMENT_ACCESSES];

static void prepare_synthetic_data(void);
static void generate_synthetic_data(void);
static void insert_synthetic_data_into_server(void);

static void generate_ranged_point_query_access_sequence(void);
static void perform_access(const uint32_t access_index);\

void perform_experiments(void) {
  prepare_synthetic_data();
  generate_ranged_point_query_access_sequence();

  struct timespec elapsed_time;

  for (size_t access_index = 0; access_index < NUM_EXPERIMENT_ACCESSES;
       access_index++) {
    TIME_VOID_FUNC(&elapsed_time, perform_access(access_index));
    fprintf(stdout, "Access %lu took: %ld s %ld ns\n", access_index,
            elapsed_time.tv_sec, elapsed_time.tv_nsec);
    // DO SOMETHING WITH THIS TIMING INFORMATION LIKE WRITING TO A LOG FILE OR
    // SOMETHING
  }
}

static void prepare_synthetic_data(void) {
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
    CLIENT_access(i, WRITE, &synthetic_dataset[block_data_index], NULL);
  }
}

static void generate_ranged_point_query_access_sequence(void) {
  for (size_t access_index = 0; access_index < NUM_EXPERIMENT_ACCESSES;
       access_index++) {
    const uint32_t start_block_range_id = uniform_random(NUM_TOTAL_REAL_BLOCKS - (QUERY_RANGE_SIZE - 1));
    for (size_t query_num = 0; query_num < QUERY_RANGE_SIZE; query_num++) {
      access_sequence_buffer[access_index].queries[query_num] =
          (start_block_range_id + query_num) % NUM_TOTAL_REAL_BLOCKS;
    }
  }
}

static void perform_access(const uint32_t access_index) {
  const Access *access = &access_sequence_buffer[access_index];
  uint8_t returned_data[NUM_DATA_BYTES_PER_BLOCK];

  for (size_t query_index = 0; query_index < QUERY_RANGE_SIZE; query_index++) {
    const uint32_t query_block_id = access->queries[query_index];
    const uint8_t *expected_data =
        &synthetic_dataset[query_block_id * NUM_DATA_BYTES_PER_BLOCK];
    CLIENT_access(query_block_id, READ, NULL, returned_data);

    if (memcmp(expected_data, returned_data, NUM_DATA_BYTES_PER_BLOCK) != 0) {
      fprintf(stderr,
              "Comparison of data for block: %u differs from expected.\n",
              query_block_id);
      exit(EXIT_FAILURE);
    }
  }
}
