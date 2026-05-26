/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Mariotaku <https://github.com/mariotaku>.
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

#include "common.h"
#include "audio.h"
#include "video.h"
#include "input.h"
#include "hid.h"

typedef struct IHS_Session IHS_Session;

typedef struct IHS_SessionInfo {
    IHS_SocketAddress address;
    uint8_t sessionKey[32];
    size_t sessionKeyLen;
    uint64_t steamId;
} IHS_SessionInfo;

typedef struct IHS_SessionConfig {
    bool enableAudio;
    bool enableHevc;
} IHS_SessionConfig;

typedef struct IHS_StreamSessionCallbacks {
    void (*initialized)(IHS_Session *session, void *context);

    void (*connecting)(IHS_Session *session, void *context);

    void (*configuring)(IHS_Session *session, IHS_SessionConfig *config, void *context);

    void (*connected)(IHS_Session *session, void *context);

    void (*disconnected)(IHS_Session *session, void *context);

    /**
     * All the resources and states has been destroyed. Nothing can be used beyond this call.
     * @param session Session pointer
     * @param context Callback context
     */
    void (*finalized)(IHS_Session *session, void *context);
} IHS_StreamSessionCallbacks;

/*
 * Lifecycle functions
 */

/**
 * Create session instance
 * @param clientConfig Client info
 * @param sessionInfo Session info
 * @return Session instance
 */
IHS_Session *IHS_SessionCreate(const IHS_ClientConfig *clientConfig, const IHS_SessionInfo *sessionInfo);

/**
 * Start receive and send thread, and send connect request
 * @param session Session instance
 * @return
 */
bool IHS_SessionConnect(IHS_Session *session);

/**
 * Send disconnect request
 * @param session Session instance
 */
void IHS_SessionDisconnect(IHS_Session *session);

/**
 * Wait for all threads to finish
 * @param session Session instance
 */
void IHS_SessionThreadedJoin(IHS_Session *session);

/**
 * Release all resources of the session and free the pointer
 * @param session Session instance
 */
void IHS_SessionDestroy(IHS_Session *session);

/*
 * Callback related functions
 */

void IHS_SessionSetSessionCallbacks(IHS_Session *session, const IHS_StreamSessionCallbacks *callbacks, void *context);

void IHS_SessionSetAudioCallbacks(IHS_Session *session, const IHS_StreamAudioCallbacks *callbacks, void *context);

void IHS_SessionSetVideoCallbacks(IHS_Session *session, const IHS_StreamVideoCallbacks *callbacks, void *context);

void IHS_SessionSetInputCallbacks(IHS_Session *session, const IHS_StreamInputCallbacks *callbacks, void *context);

/**
 * Register microphone (client → server) callbacks. Setting non-NULL callbacks here
 * advertises the `enable_microphone_streaming` capability to the server during
 * negotiation; the server may then issue Start/Stop microphone control messages.
 *
 * Must be called before IHS_SessionConnect — capability negotiation happens once and
 * cannot be re-advertised mid-session.
 */
void IHS_SessionSetMicrophoneCallbacks(IHS_Session *session, const IHS_StreamMicrophoneCallbacks *callbacks,
                                       void *context);

/**
 * Submit one encoded microphone frame for transmission. Call from the capture / encode
 * thread, once per Opus packet (or once per PCM chunk when codec=Raw). The payload is
 * wrapped in a data-frame header and queued for unreliable send on the mic data channel.
 *
 * Returns false if no microphone channel is currently active (the server hasn't issued
 * Start, or has issued Stop, or the session isn't connected); the caller can use this
 * as the cue to drop the frame.
 */
bool IHS_SessionSendMicrophoneData(IHS_Session *session, const uint8_t *data, size_t len);

/**
 * One of the four client-owned stages of a video frame's lifecycle that the
 * application reports back to ihslib via IHS_SessionReportVideoFrameStage.
 * Values match Steam's EStreamFrameEvent so the wire encoding is a direct
 * cast — do not renumber.
 */
typedef enum IHS_VideoFrameStage {
    IHS_VideoFrameStageDecodeBegin = 14,
    IHS_VideoFrameStageDecodeEnd = 15,
    IHS_VideoFrameStageUploadBegin = 16,
    IHS_VideoFrameStageUploadEnd = 17,
} IHS_VideoFrameStage;

/**
 * Per-frame outcome reported by the application. Values match Steam's
 * EStreamFrameResult — do not renumber.
 */
typedef enum IHS_VideoFrameResult {
    IHS_VideoFrameResultPending = 0,
    IHS_VideoFrameResultDisplayed = 1,
    IHS_VideoFrameResultDroppedNetworkSlow = 2,
    IHS_VideoFrameResultDroppedNetworkLost = 3,
    IHS_VideoFrameResultDroppedDecodeSlow = 4,
    IHS_VideoFrameResultDroppedDecodeCorrupt = 5,
    IHS_VideoFrameResultDroppedLate = 6,
    IHS_VideoFrameResultDroppedReset = 7,
} IHS_VideoFrameResult;

/**
 * Report a per-frame decode/upload milestone. ihslib accumulates these into a
 * 1 Hz CFrameStatsListMsg that drives the host's adaptive-bitrate / framerate
 * loop. `timestamp` is in IHS_SessionPacketTimestamp units (1/65536 second);
 * pass `0` to use the current session clock.
 *
 * Safe to call from any thread.
 */
void IHS_SessionReportVideoFrameStage(IHS_Session *session, uint16_t frameId,
                                      IHS_VideoFrameStage stage, uint32_t timestamp);

/**
 * Report the final outcome of a video frame (displayed or one of the drop
 * categories). Must be called exactly once per frame the application
 * processed, AFTER any IHS_SessionReportVideoFrameStage calls for it. This is
 * what advances the "last displayed" cursor that the 1 Hz flush walks; if
 * skipped, the host never gets stats for that frame.
 *
 * Safe to call from any thread.
 */
void IHS_SessionReportVideoFrameComplete(IHS_Session *session, uint16_t frameId,
                                         IHS_VideoFrameResult result);

/**
 * Debug toggle: when true, each flush packet carries per-frame CFrameStats
 * rows in addition to the aggregated stats. Default false (aggregated only,
 * matching Steam's normal operating mode). Useful for capturing wire traces.
 */
void IHS_SessionStatsSetFullReporting(IHS_Session *session, bool enabled);

void IHS_SessionSetLogFunction(IHS_Session *session, IHS_LogFunction *logFunction);

const IHS_SessionInfo *IHS_SessionGetInfo(const IHS_Session *session);