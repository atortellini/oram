#include "crypto.h"

#include "constants.h"
#include "rand.h"
#include "types.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static OSSL_LIB_CTX *libctx = NULL;
static const char *propery_query = NULL;
static EVP_CIPHER *cipher = NULL;

static uint8_t key[AES_KEY_SIZE_BYTES];

static bool cryptoIsInitialized = false;

void CRYPTO_init(void) {
  if (cryptoIsInitialized) {
    fprintf(stderr, "Crypto already initialized...\n");
    return;
  }
  if ((cipher = EVP_CIPHER_fetch(libctx, "AES-256-GCM", propery_query)) ==
      NULL) {
    fprintf(stderr, "Failed to fetch AES-256-GCM cipher\n");
    exit(EXIT_FAILURE);
  }

  generate_random_bytes(key, AES_KEY_SIZE_BYTES);
  cryptoIsInitialized = true;
}

void CRYPTO_encrypt_block(const Block *block_to_encrypt,
                   EncryptedBlock *encrypted_block_output) {

  if (!cryptoIsInitialized) {
    fprintf(stderr,
            "Invoked encryption without initializing crypto first. Performing "
            "initialization...\n");
    CRYPTO_init();
  }
  EVP_CIPHER_CTX *cipher_ctx = NULL;
  OSSL_PARAM params[2] = {OSSL_PARAM_END, OSSL_PARAM_END};

  uint8_t iv[AES_IV_SIZE_BYTES];
  uint8_t auth_tag[AES_TAG_SIZE_BYTES];
  uint8_t outbuffer[NUM_BYTES_PER_BLOCK_AND_METADATA];
  int outbuffer_count = 0;
  int tmplen = 0;

  if ((cipher_ctx = EVP_CIPHER_CTX_new()) == NULL) {
    fprintf(stderr, "Error creating cipher context\n");
    exit(EXIT_FAILURE);
  }
  generate_random_bytes(iv, AES_IV_SIZE_BYTES);

  if (!EVP_EncryptInit_ex2(cipher_ctx, cipher, key, iv, params)) {
    fprintf(stderr, "Failed to initialize cipher context for encryption.\n");
    exit(EXIT_FAILURE);
  }

  const uint8_t *bytes_to_encrypt = (const uint8_t *)block_to_encrypt;
  const size_t num_bytes_to_encrypt = NUM_BYTES_PER_BLOCK_AND_METADATA;

  if (!EVP_EncryptUpdate(cipher_ctx, outbuffer, &outbuffer_count,
                         bytes_to_encrypt, num_bytes_to_encrypt)) {
    fprintf(stderr, "Failed to encrypt plaintext.\n");
    exit(EXIT_FAILURE);
  }
  if (!EVP_EncryptFinal_ex(cipher_ctx, outbuffer + outbuffer_count, &tmplen)) {
    fprintf(stderr, "Failed final encrypt.\n");
    exit(EXIT_FAILURE);
  }

  params[0] = OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_AEAD_TAG,
                                                auth_tag, AES_TAG_SIZE_BYTES);

  if (!EVP_CIPHER_CTX_get_params(cipher_ctx, params)) {
    fprintf(stderr, "Failed to retrieve tag.\n");
    exit(EXIT_FAILURE);
  }

  uint8_t *iv_ptr = encrypted_block_output->data;
  uint8_t *ciphertext_ptr = iv_ptr + AES_IV_SIZE_BYTES;
  uint8_t *tag_ptr = ciphertext_ptr + NUM_BYTES_PER_BLOCK_AND_METADATA;

  memcpy(iv_ptr, iv, AES_IV_SIZE_BYTES);
  memcpy(ciphertext_ptr, outbuffer, NUM_BYTES_PER_BLOCK_AND_METADATA);
  memcpy(tag_ptr, auth_tag, AES_TAG_SIZE_BYTES);

  EVP_CIPHER_CTX_free(cipher_ctx);
}

void CRYPTO_decrypt_block(const EncryptedBlock *block_to_decrypt,
                   Block *decrypted_block_output) {
  if (!cryptoIsInitialized) {
    fprintf(stderr, "Invoked decryption without initializing crypto first.\n");
    exit(EXIT_FAILURE);
  }
  EVP_CIPHER_CTX *cipher_ctx = NULL;
  OSSL_PARAM params[2] = {OSSL_PARAM_END, OSSL_PARAM_END};

  const uint8_t *iv_ptr = block_to_decrypt->data;
  const uint8_t *ciphertext_ptr = iv_ptr + AES_IV_SIZE_BYTES;
  const uint8_t *tag_src_ptr = ciphertext_ptr + NUM_BYTES_PER_BLOCK_AND_METADATA;
  uint8_t tag_copy[AES_TAG_SIZE_BYTES];
  memcpy(tag_copy, tag_src_ptr, AES_TAG_SIZE_BYTES);

  uint8_t outbuffer[NUM_BYTES_PER_BLOCK_AND_METADATA];
  int outbuffer_count = 0;
  int tmplen = 0;

  if ((cipher_ctx = EVP_CIPHER_CTX_new()) == NULL) {
    fprintf(stderr, "Error creating cipher context\n");
    exit(EXIT_FAILURE);
  }

  if (!EVP_DecryptInit_ex2(cipher_ctx, cipher, key, iv_ptr, params)) {
    fprintf(stderr, "Failed to initialize cipher context for decryption.\n");
    exit(EXIT_FAILURE);
  }

  const size_t num_bytes_to_decrypt = NUM_BYTES_PER_BLOCK_AND_METADATA;
  if (!EVP_DecryptUpdate(cipher_ctx, outbuffer, &outbuffer_count,
                         ciphertext_ptr, num_bytes_to_decrypt)) {
    fprintf(stderr, "Failed to decrypt ciphertext.\n");
    exit(EXIT_FAILURE);
  }

  params[0] = OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_AEAD_TAG,
                                                tag_copy, AES_TAG_SIZE_BYTES);

  if (!EVP_CIPHER_CTX_set_params(cipher_ctx, params)) {
    fprintf(stderr, "Failed to set expected tag for decryption.\n");
    exit(EXIT_FAILURE);
  }

  if (EVP_DecryptFinal_ex(cipher_ctx, outbuffer, &tmplen) <= 0) {
    fprintf(stderr, "Failed verifying tag; corrupted plaintext.\n");
    exit(EXIT_FAILURE);
  }

  memcpy(decrypted_block_output, outbuffer, NUM_BYTES_PER_BLOCK_AND_METADATA);

  EVP_CIPHER_CTX_free(cipher_ctx);
}
