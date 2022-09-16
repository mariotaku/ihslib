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
#include <string.h>

#include "ihs_udp.h"

char *IHS_IPAddressToString(const IHS_IPAddress *address) {
    switch (address->family) {
        case IHS_IPAddressFamilyIPv4: {
            char *result = calloc(16, 1);
            snprintf(result, 16, "%u.%u.%u.%u", address->v4.data[0], address->v4.data[1],
                     address->v4.data[2], address->v4.data[3]);
            return result;
        }
        default: {
            abort();
        }
    }
}

int IHS_IPAddressCompare(const IHS_IPAddress *a, const IHS_IPAddress *b) {
    if (a->family != b->family) {
        return (int) a->family - (int) b->family;
    }
    switch (a->family) {
        case IHS_IPAddressFamilyIPv4: {
            return memcmp(&a->v4.data, &b->v4.data, 4);
        }
        case IHS_IPAddressFamilyIPv6: {
            return memcmp(&a->v6.data, &b->v6.data, 16);
        }
        default: {
            abort();
        }
    }
}