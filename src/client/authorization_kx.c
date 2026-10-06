/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2026 Simone Caronni <https://github.com/scaronni>.
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

#include "authorization_kx.h"
#include "endianness.h"

#include <string.h>

bool IHS_AuthorizationKeyExchangeInit(IHS_AuthorizationKeyExchange *kx, const char *pin) {
    memset(kx, 0, sizeof(*kx));
    if (IHS_CryptoRandomBytes(kx->privateKey, sizeof(kx->privateKey)) != 0) {
        return false;
    }
    if (IHS_CryptoX25519PublicKey(kx->privateKey, kx->publicKey) != 0) {
        return false;
    }
    return IHS_CryptoSHA256((const uint8_t *) pin, strlen(pin), kx->pinHash) == 0;
}

void IHS_AuthorizationKeyExchangeMaskedKey(const IHS_AuthorizationKeyExchange *kx,
                                           uint8_t out[IHS_CRYPTO_X25519_KEY_SIZE]) {
    for (size_t i = 0; i < IHS_CRYPTO_X25519_KEY_SIZE; i++) {
        out[i] = kx->publicKey[i] ^ kx->pinHash[i];
    }
}

bool IHS_AuthorizationKeyExchangeComplete(const IHS_AuthorizationKeyExchange *kx,
                                          const uint8_t *hostAuthKey, size_t hostAuthKeyLen,
                                          const uint8_t *hostDeviceToken, size_t hostDeviceTokenLen,
                                          uint64_t deviceId, uint8_t secretKey[IHS_CRYPTO_SHA256_SIZE]) {
    if (hostAuthKey == NULL || hostAuthKeyLen != IHS_CRYPTO_X25519_KEY_SIZE || hostDeviceToken == NULL) {
        return false;
    }
    uint8_t hostPublicKey[IHS_CRYPTO_X25519_KEY_SIZE];
    for (size_t i = 0; i < IHS_CRYPTO_X25519_KEY_SIZE; i++) {
        hostPublicKey[i] = hostAuthKey[i] ^ kx->pinHash[i];
    }
    uint8_t secret[IHS_CRYPTO_SHA256_SIZE];
    if (IHS_CryptoKeyExchange(kx->privateKey, hostPublicKey, secret) != 0) {
        return false;
    }

    // The host proves it derived the same secret by encrypting our device ID with it.
    uint8_t token[64];
    size_t tokenLen = sizeof(token);
    bool ok = hostDeviceTokenLen <= sizeof(token) + IHS_CRYPTO_AES_BLOCK_SIZE &&
              IHS_CryptoSymmetricDecrypt(hostDeviceToken, hostDeviceTokenLen, secret, sizeof(secret), token,
                                         &tokenLen) == 0;
    uint64_t tokenDeviceId = 0;
    if (ok && tokenLen == 8) {
        IHS_ReadUInt64LE(token, &tokenDeviceId);
    }
    ok = ok && tokenLen == 8 && tokenDeviceId == deviceId;
    if (ok) {
        memcpy(secretKey, secret, sizeof(secret));
    }
    memset(secret, 0, sizeof(secret));
    return ok;
}

void IHS_AuthorizationKeyExchangeClear(IHS_AuthorizationKeyExchange *kx) {
    volatile uint8_t *p = (volatile uint8_t *) kx;
    for (size_t i = 0; i < sizeof(*kx); i++) {
        p[i] = 0;
    }
}
