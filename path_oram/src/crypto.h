#ifndef CRYPTO_HELPERS_H
#define CRYPTO_HELPERS_H

#include "types.h"

void CRYPTO_init(void);

void CRYPTO_encrypt_block(const Block *block_to_encrypt,
                          EncryptedBlock *encrypted_block_target);

void CRYPTO_decrypt_block(const EncryptedBlock *block_to_decrypt,
                          Block *decrypted_block_target);

#endif