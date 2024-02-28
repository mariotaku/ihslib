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
#include "ihs_enumeration.h"

IHS_Enumeration *IHS_EnumerationCreate(const IHS_EnumerationClass *cls, void *arg) {
    IHS_Enumeration *enumeration = cls->alloc(cls, arg);
    assert(enumeration->cls == cls);
    return enumeration;
}

void IHS_EnumerationReset(IHS_Enumeration *enumeration) {
    enumeration->cls->reset(enumeration);
}

bool IHS_EnumerationEnded(IHS_Enumeration *enumeration) {
    return enumeration->cls->ended(enumeration);
}

void *IHS_EnumerationGet(const IHS_Enumeration *enumeration) {
    return enumeration->cls->get(enumeration);
}

void *IHS_EnumerationNext(IHS_Enumeration *enumeration) {
    return enumeration->cls->next(enumeration);
}

size_t IHS_EnumerationCount(const IHS_Enumeration *enumeration) {
    return enumeration->cls->count(enumeration);
}

void IHS_EnumerationFree(IHS_Enumeration *enumeration) {
    enumeration->cls->free(enumeration);
}