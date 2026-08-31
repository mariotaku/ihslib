/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2026 Mariotaku <https://github.com/mariotaku>.
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

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @file activity.h
 * @brief What the host is currently streaming.
 *
 * The host reports this over the control channel whenever the streamed activity changes. It is the
 * only place a client learns the app id of the running game, which is what the official client uses
 * to build its recent-games list.
 */

typedef struct IHS_Session IHS_Session;

/**
 * Longest game name retained, including the terminator. Names longer than this are truncated.
 */
#define IHS_ACTIVITY_GAME_NAME_MAX 128

/**
 * Buffer size that always fits an IHS_AppImageURL result, including the terminator.
 */
#define IHS_APP_IMAGE_URL_MAX 96

typedef enum IHS_SessionActivity {
    IHS_SessionActivityIdle = 1,
    IHS_SessionActivityGame = 2,
    IHS_SessionActivityDesktop = 3,
    IHS_SessionActivitySecureDesktop = 4,
    IHS_SessionActivityMusic = 5,
} IHS_SessionActivity;

typedef struct IHS_SessionActivityInfo {
    IHS_SessionActivity activity;
    /**
     * Steam app id of the running game, or 0 when the host did not report one (which is the norm
     * for every activity other than IHS_SessionActivityGame).
     */
    uint32_t appId;
    /**
     * Steam game id, or 0 when not reported. Not the same as appId for mods and shortcuts.
     */
    uint64_t gameId;
    /**
     * Game name as reported by the host, truncated to fit. Empty string when not reported.
     */
    char gameName[IHS_ACTIVITY_GAME_NAME_MAX];
} IHS_SessionActivityInfo;

/**
 * Copy the most recent activity the host reported.
 *
 * Safe to call from any thread. The activity arrives on the control channel, so it will normally
 * only be available some time after the session connects.
 *
 * @param session Session instance
 * @param info Filled in on success; left untouched on failure
 * @return true if the host has reported an activity, false if it has not reported one yet
 */
bool IHS_SessionGetActivity(IHS_Session *session, IHS_SessionActivityInfo *info);

/**
 * Copy the app id of the most recently streamed game.
 *
 * Convenience wrapper over IHS_SessionGetActivity for the common "what game is this" case. An
 * activity that carries no app id (desktop, music, or a game the host did not identify) is not a
 * game, so this reports failure for it.
 *
 * @param session Session instance
 * @param appId Filled in on success; left untouched on failure
 * @return true if a game app id is known
 */
bool IHS_SessionGetLastGameAppID(IHS_Session *session, uint32_t *appId);

/**
 * Build the store image URL for an app id.
 *
 * This is the same asset the official client downloads for its recent-games list. Note it is the
 * 460x215 landscape store header capsule, not a square icon — size any UI accordingly.
 *
 * ihslib has no HTTP client and deliberately does not fetch this; it only formats the URL. Follow
 * it with whatever the application already uses, and cache the result, since the image for a given
 * app id does not change often.
 *
 * Behaves like snprintf: always terminates when destLen > 0, and returns the length that a large
 * enough buffer would have held, so a return value >= destLen means the output was truncated.
 * Pass a buffer of IHS_APP_IMAGE_URL_MAX to be sure it never is.
 *
 * @param appId Steam app id. Must not be 0
 * @param dest Destination buffer
 * @param destLen Size of dest in bytes
 * @return Length of the full URL, or -1 if appId is 0 or dest is NULL with a non-zero destLen
 */
int IHS_AppImageURL(uint32_t appId, char *dest, size_t destLen);
