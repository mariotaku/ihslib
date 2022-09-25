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

#include "frame.h"

#include <assert.h>

void IHS_SessionFrameBodyInitialize(IHS_Buffer *body, bool hasCrc) {
    IHS_BufferInit(body, 2048, 2048);

    // Reserve space for serialized header
    IHS_BufferFillMem(body, 0, 0, IHS_PACKET_HEADER_SIZE);
    IHS_BufferOffsetBy(body, IHS_PACKET_HEADER_SIZE);
    assert(body->offset == IHS_PACKET_HEADER_SIZE);
    if (hasCrc) {
        IHS_BufferSetSuffixLength(body, 4);
        assert(body->suffix == 4);
    }
}
