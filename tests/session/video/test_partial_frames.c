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

#include "session/channels/video/partial_frames.h"

int main() {
    IHS_VideoPartialFrames frames;
    IHS_VideoPartialFramesInit(&frames);
    IHS_VideoFrameHeader header1 = {
            .sequence = 883,
            .flags = VideoFrameFlagReserved1Increment | VideoFrameFlagFrameFinish,
            .reserved1 = 0,
            .reserved2 = 0
    };
    IHS_Buffer data1 = IHS_BUFFER_INIT(16, 16);
    IHS_BufferFillMem(&data1, 0, 0, 16);
    IHS_VideoPartialFrame *partial1 = IHS_VideoPartialFramesAppend(&frames, 500, &header1, &data1);
    assert(IHS_VideoPartialFramesCount(&frames) == 1);

    IHS_VideoFrameHeader header2 = {
            .sequence = 884,
            .flags = VideoFrameFlagReserved1Increment | VideoFrameFlagFrameFinish,
            .reserved1 = 0,
            .reserved2 = 0
    };
    IHS_Buffer data2 = IHS_BUFFER_INIT(16, 16);
    IHS_BufferFillMem(&data2, 0, 0, 16);

    IHS_VideoPartialFrame *partial2 = IHS_VideoPartialFramesInsertBefore(&frames, partial1, 500, &header2, &data2);
    assert(IHS_VideoPartialFramesCount(&frames) == 2);
    assert(partial2->next == partial1);

    IHS_VideoFrameHeader header3 = {
            .sequence = 885,
            .flags = VideoFrameFlagReserved1Increment | VideoFrameFlagFrameFinish,
            .reserved1 = 0,
            .reserved2 = 0
    };
    IHS_Buffer data3 = IHS_BUFFER_INIT(16, 16);
    IHS_BufferFillMem(&data3, 0, 0, 16);

    IHS_VideoPartialFrame *partial3 = IHS_VideoPartialFramesInsertBefore(&frames, partial1, 500, &header3, &data3);
    assert(IHS_VideoPartialFramesCount(&frames) == 3);
    assert(partial3->next == partial1);
    assert(partial2->next == partial3);
    assert(frames.tail == partial1);

    IHS_VideoFrameHeader header4 = {
            .sequence = 886,
            .flags = VideoFrameFlagReserved1Increment | VideoFrameFlagFrameFinish,
            .reserved1 = 0,
            .reserved2 = 0
    };
    IHS_Buffer data4 = IHS_BUFFER_INIT(16, 16);
    IHS_BufferFillMem(&data4, 0, 0, 16);

    IHS_VideoPartialFrame *partial4 = IHS_VideoPartialFramesAppend(&frames, 500, &header4, &data4);
    assert(IHS_VideoPartialFramesCount(&frames) == 4);
    assert(frames.tail == partial4);

    IHS_VideoPartialFramesClear(&frames);
    return 0;
}