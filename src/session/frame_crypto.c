/*
 *  _____  _   _  _____  _  _  _     
 * |_   _|| | | |/  ___|| |(_)| |     Steam    
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Ningyuan Li <https://github.com/mariotaku>.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <malloc.h>
#include <mbedtls/md.h>

#include "frame.h"
#include "endianness.h"
#include "crypto.h"


int IHS_SessionFrameEncrypt(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen,
                            uint64_t sequence) {
    int ret;

    size_t plainLen = sizeof(uint64_t) + inLen;
    uint8_t *plain = malloc(plainLen);
    IHS_WriteUInt64LE(plain, sequence);
    memcpy(&plain[sizeof(uint64_t)], in, inLen);

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    uint8_t *iv = out;
    unsigned char ivLen = mbedtls_md_get_size(md);

    const uint8_t *key = session->state.config.sessionKey;
    const size_t keyLen = session->state.config.sessionKeyLen;

    if ((ret = mbedtls_md_hmac(md, key, keyLen, plain, plainLen, iv)) != 0) {
        goto exit;
    }

    size_t encLen = *outLen - ivLen;
    if ((ret = IHS_CryptoSymmetricEncryptWithIV(plain, plainLen, iv, ivLen, key, keyLen, false,
                                                &out[ivLen], &encLen)) != 0) {
        goto exit;
    }
    *outLen = ivLen + encLen;
    exit:
    free(plain);
    return ret;
}

int IHS_SessionFrameDecrypt(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen,
                            uint64_t expectedSequence) {
    const uint8_t *iv = in;

    const uint8_t *key = session->state.config.sessionKey;
    const size_t keyLen = session->state.config.sessionKeyLen;
    int ret;
    if ((ret = IHS_CryptoSymmetricDecryptWithIV(&in[16], inLen - 16, iv, 16, key, keyLen, out, outLen)) != 0) {
        goto exit;
    }

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    uint8_t hash[16];
    if ((ret = mbedtls_md_hmac(md, key, keyLen, out, *outLen, hash)) != 0) {
        fprintf(stderr, "HMAC failed: %x\n", -ret);
        goto exit;
    }
    if (memcmp(hash, iv, 16) != 0) {
        ret = -1;
        fprintf(stderr, "HMAC mismatch\n");
        goto exit;
    }

    *outLen -= 8;
    uint64_t sequence;
    IHS_ReadUInt64LE(out, &sequence);
    memmove(out, &out[8], *outLen);
    if (sequence != expectedSequence) {
        ret = -1;
        fprintf(stderr, "Sequence mismatch: %llu, expect %llu\n", sequence, expectedSequence);
        goto exit;
    }
    exit:
    return ret;
}

int IHS_SessionFrameHMACSHA256(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen) {
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    unsigned char mdSize = mbedtls_md_get_size(md);
    if (*outLen < mdSize) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    const uint8_t *key = session->state.config.sessionKey;
    const size_t keyLen = session->state.config.sessionKeyLen;
    int ret;
    if ((ret = mbedtls_md_hmac(md, key, keyLen, in, inLen, out)) != 0) {
        return ret;
    }
    *outLen = mdSize;
    return ret;
}