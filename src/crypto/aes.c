/* Based on: https://gist.github.com/rfl890/af8edce1b69993c242aec305b848bd71 */

#include <stdint.h>
#include <wmmintrin.h>

#include "crypto/aes.h"

static inline __m128i aes_256_key_expansion_1(__m128i key, __m128i keygened) {
    keygened = _mm_shuffle_epi32(keygened, _MM_SHUFFLE(3, 3, 3, 3));
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    return _mm_xor_si128(key, keygened);
}

static inline __m128i aes_256_key_expansion_2(
    __m128i key_lower,
    __m128i key_upper
) {
    key_upper = _mm_xor_si128(key_upper, _mm_slli_si128(key_upper, 0x4));
    key_upper = _mm_xor_si128(key_upper, _mm_slli_si128(key_upper, 0x4));
    key_upper = _mm_xor_si128(key_upper, _mm_slli_si128(key_upper, 0x4));

    return _mm_xor_si128(
        key_upper,
        _mm_shuffle_epi32(_mm_aeskeygenassist_si128(key_lower, 0x00), 0xaa)
    );
}

void aes_256_key_expansion(const uint8_t* userkey, __m128i* Key_Schedule) {
    __m128i lh = _mm_loadu_si128((__m128i*) userkey);
    __m128i uh = _mm_loadu_si128((__m128i*) (userkey + 16));

    Key_Schedule[0] = lh;
    Key_Schedule[1] = uh;

    Key_Schedule[2] = aes_256_key_expansion_1(
        Key_Schedule[0],
        _mm_aeskeygenassist_si128(Key_Schedule[1], 0x01)
    );
    Key_Schedule[3] = aes_256_key_expansion_2(Key_Schedule[2], Key_Schedule[1]);

    Key_Schedule[4] = aes_256_key_expansion_1(
        Key_Schedule[2],
        _mm_aeskeygenassist_si128(Key_Schedule[3], 0x02)
    );
    Key_Schedule[5] = aes_256_key_expansion_2(Key_Schedule[4], Key_Schedule[3]);

    Key_Schedule[6] = aes_256_key_expansion_1(
        Key_Schedule[4],
        _mm_aeskeygenassist_si128(Key_Schedule[5], 0x04)
    );
    Key_Schedule[7] = aes_256_key_expansion_2(Key_Schedule[6], Key_Schedule[5]);

    Key_Schedule[8] = aes_256_key_expansion_1(
        Key_Schedule[6],
        _mm_aeskeygenassist_si128(Key_Schedule[7], 0x08)
    );
    Key_Schedule[9] = aes_256_key_expansion_2(Key_Schedule[8], Key_Schedule[7]);

    Key_Schedule[10] = aes_256_key_expansion_1(
        Key_Schedule[8],
        _mm_aeskeygenassist_si128(Key_Schedule[9], 0x10)
    );
    Key_Schedule[11] = aes_256_key_expansion_2(
        Key_Schedule[10],
        Key_Schedule[9]
    );

    Key_Schedule[12] = aes_256_key_expansion_1(
        Key_Schedule[10],
        _mm_aeskeygenassist_si128(Key_Schedule[11], 0x20)
    );
    Key_Schedule[13] = aes_256_key_expansion_2(
        Key_Schedule[12],
        Key_Schedule[11]
    );

    Key_Schedule[14] = aes_256_key_expansion_1(
        Key_Schedule[12],
        _mm_aeskeygenassist_si128(Key_Schedule[13], 0x40)
    );
}

void aes256_encrypt(
    __m128i* key_schedule,
    uint8_t* plaintext,
    uint8_t* ciphertext
) {
    __m128i plaintext_block = _mm_loadu_si128((__m128i*) plaintext);

    plaintext_block = _mm_xor_si128(plaintext_block, key_schedule[0]);
    for (uint32_t i = 1; i < 14; i++) {
        plaintext_block = _mm_aesenc_si128(plaintext_block, key_schedule[i]);
    }
    plaintext_block = _mm_aesenclast_si128(plaintext_block, key_schedule[14]);

    _mm_storeu_si128((__m128i*) ciphertext, plaintext_block);
}

void aes256_decrypt(
    __m128i* key_schedule,
    uint8_t* plaintext,
    uint8_t* ciphertext
) {
    __m128i ciphertext_block = _mm_loadu_si128((__m128i*) ciphertext);

    ciphertext_block = _mm_xor_si128(ciphertext_block, key_schedule[14]);

    for (uint32_t i = 13; i > 0; i--) {
        ciphertext_block = _mm_aesdec_si128(
            ciphertext_block,
            _mm_aesimc_si128(key_schedule[i])
        );
    }

    ciphertext_block = _mm_aesdeclast_si128(ciphertext_block, key_schedule[0]);

    _mm_storeu_si128((__m128i*) plaintext, ciphertext_block);
}
