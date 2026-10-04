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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "crypto.h"
#include "endianness.h"
#include "client/authorization_kx.h"

/**
 * Plays the host's side: unmask the device key with the PIN typed on the host, answer with our own masked key and
 * the device ID encrypted with the shared secret.
 */
typedef struct FakeHost {
    uint8_t secret[IHS_CRYPTO_SHA256_SIZE];
    uint8_t authKey[IHS_CRYPTO_X25519_KEY_SIZE];
    uint8_t deviceToken[64];
    size_t deviceTokenLen;
} FakeHost;

static void FakeHostRespond(FakeHost *host, const uint8_t deviceAuthKey[IHS_CRYPTO_X25519_KEY_SIZE],
                            const char *pin, uint64_t deviceId) {
    uint8_t pinHash[IHS_CRYPTO_SHA256_SIZE];
    assert(IHS_CryptoSHA256((const uint8_t *) pin, strlen(pin), pinHash) == 0);

    uint8_t devicePublicKey[IHS_CRYPTO_X25519_KEY_SIZE];
    for (size_t i = 0; i < sizeof(devicePublicKey); i++) {
        devicePublicKey[i] = deviceAuthKey[i] ^ pinHash[i];
    }
    uint8_t privateKey[IHS_CRYPTO_X25519_KEY_SIZE], publicKey[IHS_CRYPTO_X25519_KEY_SIZE];
    assert(IHS_CryptoRandomBytes(privateKey, sizeof(privateKey)) == 0);
    assert(IHS_CryptoX25519PublicKey(privateKey, publicKey) == 0);
    assert(IHS_CryptoKeyExchange(privateKey, devicePublicKey, host->secret) == 0);
    for (size_t i = 0; i < sizeof(publicKey); i++) {
        host->authKey[i] = publicKey[i] ^ pinHash[i];
    }

    uint8_t id[8];
    IHS_WriteUInt64LE(id, deviceId);
    host->deviceTokenLen = sizeof(host->deviceToken);
    assert(IHS_CryptoSymmetricEncrypt(id, sizeof(id), host->secret, sizeof(host->secret), host->deviceToken,
                                      &host->deviceTokenLen) == 0);
}

static const uint64_t deviceId = 0x1122334455667788ull;

static void test_key_exchange(void) {
    IHS_AuthorizationKeyExchange kx;
    assert(IHS_AuthorizationKeyExchangeInit(&kx, "4115"));
    uint8_t authKey[IHS_CRYPTO_X25519_KEY_SIZE];
    IHS_AuthorizationKeyExchangeMaskedKey(&kx, authKey);
    // Nobody without the PIN sees the plain public key.
    assert(memcmp(authKey, kx.publicKey, sizeof(authKey)) != 0);

    FakeHost host;
    FakeHostRespond(&host, authKey, "4115", deviceId);
    uint8_t secretKey[IHS_CRYPTO_SHA256_SIZE];
    assert(IHS_AuthorizationKeyExchangeComplete(&kx, host.authKey, sizeof(host.authKey), host.deviceToken,
                                                host.deviceTokenLen, deviceId, secretKey));
    assert(memcmp(secretKey, host.secret, sizeof(secretKey)) == 0);

    // Token for another device
    assert(!IHS_AuthorizationKeyExchangeComplete(&kx, host.authKey, sizeof(host.authKey), host.deviceToken,
                                                 host.deviceTokenLen, deviceId + 1, secretKey));
    // Truncated key
    assert(!IHS_AuthorizationKeyExchangeComplete(&kx, host.authKey, sizeof(host.authKey) - 1, host.deviceToken,
                                                 host.deviceTokenLen, deviceId, secretKey));
    IHS_AuthorizationKeyExchangeClear(&kx);
}

static void test_wrong_pin(void) {
    IHS_AuthorizationKeyExchange kx;
    assert(IHS_AuthorizationKeyExchangeInit(&kx, "4115"));
    uint8_t authKey[IHS_CRYPTO_X25519_KEY_SIZE];
    IHS_AuthorizationKeyExchangeMaskedKey(&kx, authKey);

    // The user typed another PIN on the host: both sides unmask garbage and the secrets differ.
    FakeHost host;
    FakeHostRespond(&host, authKey, "4116", deviceId);
    uint8_t secretKey[IHS_CRYPTO_SHA256_SIZE];
    assert(!IHS_AuthorizationKeyExchangeComplete(&kx, host.authKey, sizeof(host.authKey), host.deviceToken,
                                                 host.deviceTokenLen, deviceId, secretKey));
    IHS_AuthorizationKeyExchangeClear(&kx);
}

int main(void) {
    test_key_exchange();
    test_wrong_pin();
    printf("authorization tests OK\n");
    return 0;
}
