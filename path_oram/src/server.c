#include "server.h"

#include "globals.h"
#include "types.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static FILE *server_storage;
static bool serverStorageIsInitialized;

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

void SERVER_read_bucket(uint32_t bucket_id, EncryptedBucket *bucket_out) {
  if (!serverStorageIsInitialized) {
    fprintf(stderr, "Attempting to read from uninitialized server storage\n");
    exit(EXIT_FAILURE);
  }

  const size_t bucket_byte_offset = bucket_id * sizeof(EncryptedBucket);
  if (fseek(server_storage, bucket_byte_offset, SEEK_SET)) {
    server_failed_seek(bucket_id);
  }

  if (!fread(bucket_out, sizeof(EncryptedBucket), 1, server_storage)) {
    char *error_string = NULL;
    asprintf(&error_string, "Failed to read bucket %u from server storage",
             bucket_id);
    assert(error_string);
    server_failure(error_string);
  }
}

void SERVER_write_bucket(uint32_t bucket_id, EncryptedBucket *bucket) {
  if (!serverStorageIsInitialized) {
    fprintf(stderr, "Attempting to write to uninitialized server storage\n");
    SERVER_init();
  }

  const size_t bucket_byte_offset = bucket_id * sizeof(EncryptedBucket);
  if (fseek(server_storage, bucket_byte_offset, SEEK_SET)) {
    server_failed_seek(bucket_id);
  }

  if (!fwrite(bucket, sizeof(EncryptedBucket), 1, server_storage)) {
    char *error_string = NULL;
    asprintf(&error_string, "Failed to write bucket %u to server storage",
             bucket_id);
    assert(error_string);
    server_failure(error_string);
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
