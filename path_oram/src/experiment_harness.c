#include "experiment_harness.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "constants.h"
#include "globals.h"
#include "rand.h"
#include "timer.h"

#define min(X, Y) ((X) < (Y) ? (X) : (Y))

typedef struct {
  uint32_t queries[QUERY_RANGE_SIZE];
} Access;

static uint8_t synthetic_dataset[NUM_TOTAL_REAL_BLOCKS * NUM_BYTES_PER_BLOCK];
static Access access_sequence_buffer[NUM_EXPERIMENT_ACCESSES];

static void prepare_synthetic_data(void);
static void generate_synthetic_data(void);
static void insert_synthetic_data_into_server(void);

static void generate_ranged_point_query_access_sequence(void);
static void perform_access(const uint32_t access_index);
// also need to think about for the experiments if I would want it to be a
// random number of writes/reads being performed on random blocks or if there
// should be parameters to specify how many reads/writes, what blocks to access,
// etc.
// Also question of whether I should generate all of the operations to be
// performed prior to beginning the experiments as I expect the generation to
// have impacts on the observed experiment times. Could have the entire access
// order pre generated, along with predetermining whether a read/write should
// occur and if a write, already generating the new data that should be written.

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
                        NUM_TOTAL_REAL_BLOCKS * NUM_BYTES_PER_BLOCK);
}

static void insert_synthetic_data_into_server(void) {
  for (size_t i = 0; i < NUM_TOTAL_REAL_BLOCKS; i++) {
    const uint32_t block_data_index = i * NUM_BYTES_PER_BLOCK;
    CLIENT_access(i, WRITE, &synthetic_dataset[block_data_index], NULL);
  }
}

static void generate_ranged_point_query_access_sequence(void) {
  for (size_t access_index = 0; access_index < NUM_EXPERIMENT_ACCESSES;
       access_index++) {
    const uint32_t start_block_range_id = uniform_random(NUM_TOTAL_REAL_BLOCKS);
    for (size_t query_num = 0; query_num < QUERY_RANGE_SIZE; query_num++) {
      access_sequence_buffer[access_index].queries[query_num] =
          (start_block_range_id + query_num) % NUM_TOTAL_REAL_BLOCKS;
    }
  }
}

static void perform_access(const uint32_t access_index) {
  const Access *access = &access_sequence_buffer[access_index];
  uint8_t returned_data[NUM_BYTES_PER_BLOCK];

  for (size_t query_index = 0; query_index < QUERY_RANGE_SIZE; query_index++) {
    const uint32_t query_block_id = access->queries[query_index];
    const uint8_t *expected_data =
        &synthetic_dataset[query_block_id * NUM_BYTES_PER_BLOCK];
    CLIENT_access(query_block_id, READ, NULL, returned_data);

    if (memcmp(expected_data, returned_data, NUM_BYTES_PER_BLOCK) != 0) {
      fprintf(stderr,
              "Comparison of data for block: %u differs from expected.\n",
              query_block_id);
      exit(EXIT_FAILURE);
    }
  }
}
// I have an array of NUM_EXPERIMENT_ACCESSES size where, on each step, I want
// to fill out a range of [1, QUERY_SIZE]. thinking of this recursively I have
// something like this: fill_array(curr_index, arr_size_left, query_size) if
// (arr_size_left == 0) return; const max_query_size = min(arr_size_left,
// query_size) range_query_size = uniform_random(max_query_size) + 1; const
// end_index = curr_index + range_query_size;
//
// for (curr_index < end_index; curr_index++) {
//  access_sequence_buffer[curr_index] = uniform_random(NUM_TOTAL_REAL_BlOCKS) +
//  1;
//}
// const new_arr_size_left = arr_size_left - range_query_size;
// fill_array(curr_index, new_arr_size_left, query_size);

// UNUSUED FUNCTIONS FOR EXPERIMENTS THAT MAY BE USED IF I DECIDE TO DO
// EXPERIMENTS IN A DIFFERENT WAY e.g. I DECIDE TO ALLOW EXPERIMENTS TO PERFORM
// WRITE OPERATIONS ON THE DATA. THIS CURRENTLY ISN'T IMPLEMENTED FOR A COUPLE
// REASONS ONE I WOULD HAVE TO DEAL WITH HOW I WOULD ACQUIRE THE NEW DATA BYTES
// TO BE WRITTEN, THIS CAN BE DONE ON THE FLY BUT THEN THAT WOULD MEAN EXTRA
// OVERHEAD ON THE TIMING THAT ISN'T DIRECTLY RELATED TO HOW THE SCHEME
// PERFORMS. TWO IF I WERE TO WANT TO MINIMIZE THE OVERHEAD (ONLY REQUIRING THE
// COPYING OVER OF BYTES TO THE SYNETHIC DATASET ARRAY) IT WOULD REQUIRE ME TO
// PREGENERATE ALL OF THE WRITE DATA THAT IS TO OCCUR DURING PARTICULAR
// ACCESSES. THIS WOULD PROBABLY REQUIRE CHANGING THE ACCESS BUFFER TO BE OF
// STRUCTS WHICH CONTAIN BOTH THE ID TO BE ACCSESED ALONG WITH WHETHER IT IS A
// WRITE, THEN A SECOND ARRAY OF SYNTHETIC DATA LARGE ENOUGH TO COPE WITH ALL
// THE WRITES THAT ARE TO BE PERFORMED.

// void perform_experiments(const uint32_t num_accesses) {
//   prepare_synthetic_data();

//   for (size_t i = 0; i < num_accesses; i++) {
//     const uint32_t block_to_access = uniform_random(NUM_TOTAL_REAL_BLOCKS);
//     const bool performWrite = coin_flip();
//     if (performWrite) {
//       uint8_t new_block_data[NUM_BYTES_PER_BLOCK];
//       generate_random_bytes(new_block_data, NUM_BYTES_PER_BLOCK);
//       // probably want to change this so I can give the option of whether the
//       // old data should be saved, for example if i want to have the option
//       to
//       // ensure correctness by comparing prev data
//       CLIENT_access(block_to_access, WRITE, new_block_data, NULL);
//       update_synthetic_block_data(block_to_access, new_block_data,
//                                   NUM_BYTES_PER_BLOCK);

//     } else {
//       // again should probably have an option here to specify whether old
//       data
//       // should be kept if wanting to do some correctness checking
//       CLIENT_access(block_to_access, READ, NULL, NULL);
//     }
//   }
// }

// static void update_synthetic_block_data(const uint32_t block_id,
//                                         const uint8_t new_data[],
//                                         const uint32_t num_bytes) {
//   const bool tooManyBytes = num_bytes > NUM_BYTES_PER_BLOCK;
//   const bool tooFewBytes = num_bytes < NUM_BYTES_PER_BLOCK;

//   if (tooManyBytes || tooFewBytes) {
//     fprintf(stderr, "Attempted to update synthetic block data with incorrect
//     "
//                     "number of bytes.");
//     exit(1);
//   }

//   const uint32_t synthetic_data_index = block_id * NUM_BYTES_PER_BLOCK;

//   mempcy(&synthetic_dataset[synthetic_data_index], num_bytes);
// }
