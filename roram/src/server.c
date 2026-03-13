#include "server.h"

#include "globals.h"
#include "types.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static FILE *server_storage;
static bool serverStorageIsInitialized;

static void seek_to_bucket(uint32_t bucket_id);
static void server_failure(char *error_string);
static void server_failed_seek(const uint32_t bucket_id);

void SERVER_init(void) {
  if (serverStorageIsInitialized) {
    fprintf(stderr, "Server storage already initialized; Ignoring\n");
    return;
  }

  server_storage = tmpfile();
  assert(server_storage);
  serverStorageIsInitialized = true;
}

void SERVER_read_range(uint32_t bucket_id, uint32_t num_buckets,
                       ENCRYPTED_BUCKET *buckets_out) {
  if (!serverStorageIsInitialized) {
    fprintf(stderr, "Attempting to read from uninitialized server storage\n");
    exit(EXIT_FAILURE);
  }

  seek_to_bucket(bucket_id);

  if (fread(buckets_out, sizeof(ENCRYPTED_BUCKET), num_buckets,
            server_storage) < num_buckets) {
    char *error_string = NULL;
    asprintf(&error_string,
             "Failed to read bucket range [%u, %u + %u) from server storage",
             bucket_id, bucket_id, num_buckets);
    assert(error_string);
    server_failure(error_string);
  }
}

void SERVER_write_range(uint32_t bucket_id, uint32_t num_buckets,
                        ENCRYPTED_BUCKET *buckets) {
  if (!serverStorageIsInitialized) {
    fprintf(stderr, "Attempting to write to uninitialized server storage\n");
    SERVER_init();
  }

  seek_to_bucket(bucket_id);

  if (fwrite(buckets, sizeof(ENCRYPTED_BUCKET), num_buckets, server_storage) <
      num_buckets) {
    char *error_string = NULL;
    asprintf(&error_string,
             "Failed to write bucket range [%u, %u + %u) to server storage",
             bucket_id, bucket_id, num_buckets);
    assert(error_string);
    server_failure(error_string);
  }
}

static void seek_to_bucket(uint32_t bucket_id) {
  const size_t bucket_byte_offset = bucket_id * sizeof(ENCRYPTED_BUCKET);
  if (fseek(server_storage, bucket_byte_offset, SEEK_SET)) {
    server_failed_seek(bucket_id);
  }
}

static void server_failed_seek(const uint32_t bucket_id) {
  char *error_string = NULL;
  asprintf(&error_string, "Failed to seek in server storage to bucket %u\n",
           bucket_id);
  assert(asprintf);
  server_failure(error_string);
}

static void server_failure(char *error_string) {
  perror(error_string);
  if (server_storage) {
    fclose(server_storage);
  }

  if (error_string) {
    free(error_string);
  }

  exit(EXIT_FAILURE);
}
