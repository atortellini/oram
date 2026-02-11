#ifndef CRYPTO_HELPERS_H
#define CRYPTO_HELPERS_H

#include "types.h"

void encrypt_block(const Block *block_to_encrypt, EncryptedBlock *encrypted_block_target);

void decrypt_block(const EncryptedBlock *block_to_decrypt, Block *decrypted_block_target);

#endif