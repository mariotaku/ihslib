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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "crypto.h"

/**
 * Device side of the PIN-masked key exchange current Steam uses to pair devices, as done by the official Steam Link
 * client (reversed from Steam Link 1.3.32, the code logging "BHandleKeyExchangeAuthorization").
 *
 * The device sends its X25519 public key XOR SHA256(PIN) as auth_key in CMsgRemoteDeviceAuthorizationRequest. Once
 * the user has typed the PIN, the host answers with its own public key masked the same way, plus device_token =
 * SymmetricEncrypt(u64le device ID) under secret = SHA256(X25519(private key, peer public key)). The device checks
 * the token, replies with CMsgRemoteDeviceAuthorizationConfirmed and from then on uses the secret as the secret key
 * for that host. Hosts no longer complete a pairing with the key escrow ticket alone: they answer Failed.
 */
typedef struct IHS_AuthorizationKeyExchange {
    uint8_t privateKey[IHS_CRYPTO_X25519_KEY_SIZE];
    uint8_t publicKey[IHS_CRYPTO_X25519_KEY_SIZE];
    uint8_t pinHash[IHS_CRYPTO_SHA256_SIZE];
} IHS_AuthorizationKeyExchange;

/**
 * Generate a key pair for one authorization attempt.
 */
bool IHS_AuthorizationKeyExchangeInit(IHS_AuthorizationKeyExchange *kx, const char *pin);

/**
 * Our public key XOR SHA256(PIN), for CMsgRemoteDeviceAuthorizationRequest.auth_key.
 */
void IHS_AuthorizationKeyExchangeMaskedKey(const IHS_AuthorizationKeyExchange *kx,
                                           uint8_t out[IHS_CRYPTO_X25519_KEY_SIZE]);

/**
 * Unmask the host's auth_key, derive the shared secret and check the host's device_token decrypts to our device ID.
 * @return true and the new secret key on success
 */
bool IHS_AuthorizationKeyExchangeComplete(const IHS_AuthorizationKeyExchange *kx,
                                          const uint8_t *hostAuthKey, size_t hostAuthKeyLen,
                                          const uint8_t *hostDeviceToken, size_t hostDeviceTokenLen,
                                          uint64_t deviceId, uint8_t secretKey[IHS_CRYPTO_SHA256_SIZE]);

void IHS_AuthorizationKeyExchangeClear(IHS_AuthorizationKeyExchange *kx);
