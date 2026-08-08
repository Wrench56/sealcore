#ifndef CRYPTO_AES_H
#define CRYPTO_AES_H

#include <stdint.h>
#include <wmmintrin.h>

void aes_256_key_expansion(const uint8_t* userkey, __m128i* Key_Schedule);

void aes256_decrypt(
    __m128i* key_schedule,
    uint8_t* plaintext,
    uint8_t* ciphertext
);

void aes256_encrypt(
    __m128i* key_schedule,
    uint8_t* plaintext,
    uint8_t* ciphertext
);

#endif // CRYPTO_AES_H
