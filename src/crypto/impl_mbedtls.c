#include "ihslib/crypto.h"
#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/cipher.h>
#include <string.h>

static int
IHS_CryptoAES_CBC_PKCS7Pad(const uint8_t *in, size_t inLen, const uint8_t *iv, size_t ivLen, const uint8_t *key,
                           size_t keyLen, uint8_t *out, size_t *outLen, bool enc);

int IHS_CryptoSymmetricEncrypt(const uint8_t *in, size_t inLen, const uint8_t *key, size_t keyLen, uint8_t *out,
                               size_t *outLen) {
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    uint8_t iv[16];
    mbedtls_ctr_drbg_random(&ctr_drbg, iv, 16);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    return IHS_CryptoSymmetricEncryptWithIV(in, inLen, iv, 16, key, keyLen, true, out, outLen);
}

int IHS_CryptoSymmetricEncryptWithIV(const uint8_t *in, size_t inLen, const uint8_t *iv, size_t ivLen,
                                     const uint8_t *key, size_t keyLen, bool withIV, uint8_t *out, size_t *outLen) {
    int ret = IHS_CryptoAES_CBC_PKCS7Pad(in, inLen, iv, ivLen, key, keyLen, out, outLen, true);
    if (ret != 0) return ret;
    if (withIV) {
        memcpy(&out[*outLen], iv, ivLen);
        *outLen += ivLen;
    }
    return ret;
}

int IHS_CryptoSymmetricDecrypt(const uint8_t *in, size_t inLen, const uint8_t *key, size_t keyLen, uint8_t *out,
                               size_t *outLen) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key, keyLen * 8);
    uint8_t iv[16];
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, in, iv);
    mbedtls_aes_free(&aes);
    return IHS_CryptoSymmetricDecryptWithIV(&in[16], inLen - 16, iv, 16, key, keyLen, out, outLen);
}

int IHS_CryptoSymmetricDecryptWithIV(const uint8_t *in, size_t inLen, const uint8_t *iv, size_t ivLen,
                                     const uint8_t *key, size_t keyLen, uint8_t *out, size_t *outLen) {
    return IHS_CryptoAES_CBC_PKCS7Pad(in, inLen, iv, ivLen, key, keyLen, out, outLen, false);
}


int IHS_CryptoRSAEncrypt(const uint8_t *in, size_t inLen, const uint8_t *key, size_t keyLen, uint8_t *out,
                         size_t *outLen) {
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);

    int ret = 0;
    if ((ret = mbedtls_pk_parse_public_key(&pk, key, keyLen)) != 0) {
        goto exit;
    }
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(pk);
    if (*outLen < rsa->len) {
        ret = MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE;
        goto exit;
    }
    ret = mbedtls_rsa_rsaes_oaep_encrypt(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PUBLIC,
                                         NULL, 0, inLen, in, out);
    if (ret == 0) {
        *outLen = rsa->len;
    }
    exit:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_pk_free(&pk);
    return ret;
}

static int IHS_CryptoAES_CBC_PKCS7Pad(const uint8_t *in, size_t inLen, const uint8_t *iv, size_t ivLen,
                                      const uint8_t *key, size_t keyLen, uint8_t *out, size_t *outLen, bool enc) {
    mbedtls_cipher_context_t cipher;
    int ret = 0;
    mbedtls_cipher_init(&cipher);
    mbedtls_cipher_set_padding_mode(&cipher, MBEDTLS_PADDING_PKCS7);
    mbedtls_cipher_setup(&cipher, mbedtls_cipher_info_from_values(MBEDTLS_CIPHER_ID_AES,
                                                                  (int) keyLen * 8, MBEDTLS_MODE_CBC));
    mbedtls_cipher_setkey(&cipher, key, (int) keyLen * 8, enc ? MBEDTLS_ENCRYPT : MBEDTLS_DECRYPT);
    mbedtls_cipher_set_iv(&cipher, iv, ivLen);
    mbedtls_cipher_reset(&cipher);
    ret = mbedtls_cipher_update(&cipher, in, inLen, out, outLen);
    mbedtls_cipher_free(&cipher);
    return ret;
}