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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "ihslib.h"

int main(int argc, char *argv[]) {
    IHS_IPAddress address;
    bool ret;
    char *actual;

    ret = IHS_IPAddressFromString(&address, "127.0.0.1");
    assert(ret);
    actual = IHS_IPAddressToString(&address);
    assert(strcmp("127.0.0.1", actual) == 0);
    free(actual);

    ret = IHS_IPAddressFromString(&address, "114514");
    assert(!ret);

    ret = IHS_IPAddressFromString(&address, "123.456.789.0");
    assert(!ret);

    ret = IHS_IPAddressFromString(&address, "2001:");
    assert(!ret);

    ret = IHS_IPAddressFromString(&address, "2001:db8::1234:5678");
    assert(ret);
    actual = IHS_IPAddressToString(&address);
    assert(strcmp("2001:db8::1234:5678", actual) == 0);
    free(actual);

    IHS_IPAddress a, b;
    IHS_IPAddressFromString(&a, "2001:db8:0:0:0:0:2:1");
    IHS_IPAddressFromString(&b, "2001:db8::2:1");
    assert(IHS_IPAddressCompare(&a, &b) == 0);

    IHS_IPAddressFromString(&a, "127.0.0.0");
    IHS_IPAddressFromString(&b, "127.0.0.1");
    assert(IHS_IPAddressCompare(&a, &b) != 0);

    IHS_IPAddressFromString(&a, "fe80::ffff:ffff:ffff:ffff");
    IHS_IPAddressFromString(&b, "127.0.0.1");
    assert(IHS_IPAddressCompare(&a, &b) != 0);

    return 0;
}