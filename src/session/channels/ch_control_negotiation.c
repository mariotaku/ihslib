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
#include <stdio.h>

#include "ch_control.h"

#include "protobuf/pb_utils.h"
#include "protobuf/discovery.pb-c.h"

static void OnNegotiationInit(IHS_SessionChannel *channel, const CNegotiationInitMsg *message, uint16_t packetId);

static void OnNegotiationSetConfig(IHS_SessionChannel *channel, const CNegotiationSetConfigMsg *message,
                                   uint16_t packetId);

void IHS_SessionChannelControlOnNegotiation(IHS_SessionChannel *channel, EStreamControlMessage type,
                                            const uint8_t *payload, size_t payloadLen,
                                            const IHS_SessionPacketHeader *header) {
    switch (type) {
        case k_EStreamControlNegotiationInit: {
            CNegotiationInitMsg *message = cnegotiation_init_msg__unpack(NULL, payloadLen, payload);
            OnNegotiationInit(channel, message, header->packetId);
            cnegotiation_init_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlNegotiationSetConfig: {
            CNegotiationSetConfigMsg *message = cnegotiation_set_config_msg__unpack(NULL, payloadLen, payload);
            OnNegotiationSetConfig(channel, message, header->packetId);
            cnegotiation_set_config_msg__free_unpacked(message, NULL);
            break;
        }
        default: {
            abort();
        }
    }
}

static void OnNegotiationInit(IHS_SessionChannel *channel, const CNegotiationInitMsg *message, uint16_t packetId) {
    EStreamAudioCodec audioCodec = k_EStreamAudioCodecNone;
    EStreamVideoCodec videoCodec = k_EStreamVideoCodecNone;
    for (int i = 0; i < message->n_supported_audio_codecs; i++) {
        EStreamAudioCodec codec = message->supported_audio_codecs[i];
        if (codec == k_EStreamAudioCodecAAC) {
            audioCodec = codec;
        }
    }
    for (int i = 0; i < message->n_supported_video_codecs; i++) {
        EStreamVideoCodec codec = message->supported_video_codecs[i];
        if (codec == k_EStreamVideoCodecH264) {
            videoCodec = codec;
        }
    }

    CNegotiatedConfig config = CNEGOTIATED_CONFIG__INIT;
    PROTOBUF_C_SET_VALUE(config, reliable_data, false);

    config.has_selected_audio_codec = audioCodec != k_EStreamAudioCodecNone;
    config.selected_audio_codec = audioCodec;

    config.has_selected_video_codec = videoCodec != k_EStreamVideoCodecNone;
    config.selected_video_codec = videoCodec;

    CStreamVideoMode availableVideoMode = CSTREAM_VIDEO_MODE__INIT;
    availableVideoMode.width = 1920;
    availableVideoMode.height = 1080;
    PROTOBUF_C_SET_VALUE(availableVideoMode, refresh_rate_numerator, 5994);
    PROTOBUF_C_SET_VALUE(availableVideoMode, refresh_rate_denominator, 100);

    CStreamVideoMode *availableVideoModes[] = {&availableVideoMode};
    config.n_available_video_modes = 1;
    config.available_video_modes = availableVideoModes;

    CStreamingClientConfig clientConfig = CSTREAMING_CLIENT_CONFIG__INIT;

    PROTOBUF_C_SET_VALUE(clientConfig, maximum_resolution_x, 0);
    PROTOBUF_C_SET_VALUE(clientConfig, maximum_resolution_y, 0);
    PROTOBUF_C_SET_VALUE(clientConfig, enable_hardware_decoding, true);
    PROTOBUF_C_SET_VALUE(clientConfig, enable_performance_overlay, true);
    PROTOBUF_C_SET_VALUE(clientConfig, enable_audio_streaming, true);
    PROTOBUF_C_SET_VALUE(clientConfig, enable_video_streaming, true);
    PROTOBUF_C_SET_VALUE(clientConfig, audio_channels, 2);
    PROTOBUF_C_SET_VALUE(clientConfig, maximum_framerate_numerator, 5994);
    PROTOBUF_C_SET_VALUE(clientConfig, maximum_framerate_denominator, 100);
    PROTOBUF_C_SET_VALUE(clientConfig, quality, k_EStreamQualityBalanced);
    PROTOBUF_C_SET_VALUE(clientConfig, maximum_bitrate_kbps, 30000);

    CStreamingClientCaps clientCaps = CSTREAMING_CLIENT_CAPS__INIT;

    clientCaps.system_info = "\"SystemInfo\"\n{\n\t\"OSType\"\t\t\"-197\"\n\t\"CPUID\"\t\t\"ARM\"\n"
                             "\t\"CPUGhz\"\t\t\"0.000000\"\n\t\"PhysicalCPUCount\"\t\"1\"\n"
                             "\t\"LogicalCPUCount\"\t\"1\"\n\t\"SystemRAM\"\t\t\"263\"\n"
                             "\t\"VideoVendorID\"\t\"0\"\n\t\"VideoDeviceID\"\t\"0\"\n"
                             "\t\"VideoRevision\"\t\"0\"\n\t\"VideoRAM\"\t\t\"0\"\n"
                             "\t\"VideoDisplayX\"\t\"1920\"\n\t\"VideoDisplayY\"\t\"1080\"\n"
                             "\t\"VideoDisplayNameID\"\t\"JN-MD133BFHDR\"\n}\n";
    PROTOBUF_C_SET_VALUE(clientCaps, system_can_suspend, true);
    PROTOBUF_C_SET_VALUE(clientCaps, maximum_decode_bitrate_kbps, 30000);
    PROTOBUF_C_SET_VALUE(clientCaps, maximum_burst_bitrate_kbps, 90000);
    PROTOBUF_C_SET_VALUE(clientCaps, supports_video_hevc, false);
    PROTOBUF_C_SET_VALUE(clientCaps, form_factor, k_EStreamDeviceFormFactorTV);

    CNegotiationSetConfigMsg response = CNEGOTIATION_SET_CONFIG_MSG__INIT;
    response.config = &config;
    response.streaming_client_config = &clientConfig;
    response.streaming_client_caps = &clientCaps;

    IHS_SessionChannelControlSend(channel, k_EStreamControlNegotiationSetConfig,
                                  (const ProtobufCMessage *) &response, packetId);
}

static void OnNegotiationSetConfig(IHS_SessionChannel *channel, const CNegotiationSetConfigMsg *message,
                                   uint16_t packetId) {
    CNegotiationCompleteMsg response = CNEGOTIATION_COMPLETE_MSG__INIT;
    IHS_SessionChannelControlSend(channel, k_EStreamControlNegotiationComplete,
                                  (const ProtobufCMessage *) &response, packetId);
    IHS_SessionChannelControlStartHeartbeat(channel);

    IHS_Session *session = channel->session;
    if (session->callbacks.session && session->callbacks.session->connected) {
        session->callbacks.session->connected(session, session->callbackContexts.session);
    }
}