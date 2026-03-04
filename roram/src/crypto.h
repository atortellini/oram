#ifndef CRYPTO_HELPERS_H
#define CRYPTO_HELPERS_H

#include "types.h"

void CRYPTO_init(void);

void CRYPTO_encrypt_block(const BLOCK *block_to_encrypt,
                          ENCRYPTED_BLOCK *encrypted_block_target);

void CRYPTO_decrypt_block(const ENCRYPTED_BLOCK *block_to_decrypt,
                          BLOCK *decrypted_block_target);

#endif
