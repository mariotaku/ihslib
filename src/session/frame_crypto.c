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

#include <stdlib.h>
#include <mbedtls/md.h>

#include "frame.h"
#include "endianness.h"
#include "crypto.h"

#include "session_pri.h"

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

    const uint8_t *key = session->info.sessionKey;
    const size_t keyLen = session->info.sessionKeyLen;

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

IHS_SessionFrameDecryptResult IHS_SessionFrameDecrypt(IHS_Session *session, const IHS_Buffer *in, IHS_Buffer *out,
                                                      uint64_t expectSequence, uint64_t *actualSequence) {
    const uint8_t *key = session->info.sessionKey;
    const size_t keyLen = session->info.sessionKeyLen;
    IHS_SessionFrameDecryptResult result = IHS_SessionFrameDecryptFailed;
    IHS_BufferEnsureMaxSizeExact(out, in->size);
    size_t outLen = IHS_BufferMaxSize(out);
    if (IHS_CryptoSymmetricDecryptWithIV(IHS_BufferPointerAt(in, 16), in->size - 16,
                                         IHS_BufferPointerAt(in, 0), 16, key, keyLen,
                                         IHS_BufferPointerAt(out, 0), &outLen) != 0) {
        goto exit;
    }
    out->size = outLen;

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    uint8_t hash[16];
    int hmacRet;
    if ((hmacRet = mbedtls_md_hmac(md, key, keyLen, IHS_BufferPointerAt(out, 0), out->size, hash)) != 0) {
        result = IHS_SessionFrameDecryptFailed;
        IHS_SessionLog(session, IHS_LogLevelWarn, "Crypto", "HMAC failed: %x\n", -hmacRet);
        goto exit;
    }
    if (memcmp(hash, IHS_BufferPointerAt(in, 0), 16) != 0) {
        result = IHS_SessionFrameDecryptHashMismatch;
        IHS_SessionLog(session, IHS_LogLevelWarn, "Crypto", "HMAC mismatch\n");
        goto exit;
    }

    uint64_t actual;
    IHS_ReadUInt64LE(IHS_BufferPointerAt(out, 0), &actual);
    IHS_BufferOffsetBy(out, 8);
    if (actualSequence != NULL) {
        *actualSequence = actual;
    }
    // Sequence may smaller than expected, in that case we just ignore the frame
    if (actual != expectSequence) {
        result = actual < expectSequence ? IHS_SessionFrameDecryptOldSequence :
                 IHS_SessionFrameDecryptSequenceMismatch;
        goto exit;
    }
    result = IHS_SessionFrameDecryptOK;
    exit:
    return result;
}

int IHS_SessionFrameHMACSHA256(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen) {
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    unsigned char mdSize = mbedtls_md_get_size(md);
    if (*outLen < mdSize) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    const uint8_t *key = session->info.sessionKey;
    const size_t keyLen = session->info.sessionKeyLen;
    int ret;
    if ((ret = mbedtls_md_hmac(md, key, keyLen, in, inLen, out)) != 0) {
        return ret;
    }
    *outLen = mdSize;
    return ret;
}

void IHS_SessionFrameClear(IHS_SessionFrame *frame, bool freeData) {
    IHS_BufferClear(&frame->body, freeData);
}