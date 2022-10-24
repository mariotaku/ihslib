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

#include <assert.h>
#include "ihs_arraylist.h"

static int IntFindFn(const void *value, const void *item);

int main(int argc, char *argv[]) {
    IHS_ArrayList list;
    IHS_ArrayListInit(&list, sizeof(int));

    for (int item = 1; item < 50; item += 2) {
        IHS_ArrayListAppend(&list, &item);
    }

    assert(list.size == 25);

    int value = 0;
    int index = IHS_ArrayListBinarySearch(&list, &value, IntFindFn);
    assert(index < 0);

    value = 7;
    index = IHS_ArrayListBinarySearch(&list, &value, IntFindFn);
    assert(index == 3);

    value = 0;
    // Nothing should be removed
    assert(!IHS_ArrayListRemoveFirst(&list, &value));

    value = 1;
    // The reset item should be removed, causes the size to become 24 and the reset item to become 3
    assert(IHS_ArrayListRemoveFirst(&list, &value));

    assert(list.size == 24);
    assert(*((int *) IHS_ArrayListGet(&list, 0)) == 3);

    IHS_ArrayListDeinit(&list);
    return 0;
}

static int IntFindFn(const void *value, const void *item) {
    return *((const int *) value) - *((const int *) item);
}