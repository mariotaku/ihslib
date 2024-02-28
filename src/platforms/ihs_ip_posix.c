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

#include "ihslib/net.h"

#include <arpa/inet.h>
#include <string.h>
#include <assert.h>

bool IHS_IPAddressFromString(IHS_IPAddress *address, const char *str) {
    if (strchr(str, ':') != NULL) {
        if (inet_pton(AF_INET6, str, address->v6.data) != 1) {
            return false;
        }
        address->family = IHS_IPAddressFamilyIPv6;
    } else {
        if (inet_pton(AF_INET, str, address->v4.data) != 1) {
            return false;
        }
        address->family = IHS_IPAddressFamilyIPv4;
    }
    return true;
}

char *IHS_IPAddressToString(const IHS_IPAddress *address) {
    assert(address->family == IHS_IPAddressFamilyIPv4 || address->family == IHS_IPAddressFamilyIPv6);
    if (address->family == IHS_IPAddressFamilyIPv6) {
        char buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, address->v6.data, buf, INET6_ADDRSTRLEN);
        return strndup(buf, INET6_ADDRSTRLEN);
    } else {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, address->v4.data, buf, INET_ADDRSTRLEN);
        return strndup(buf, INET_ADDRSTRLEN);
    }
}

