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

#include <stddef.h>
#include <stdbool.h>

typedef struct IHS_EnumerationClass IHS_EnumerationClass;
typedef struct IHS_Enumeration IHS_Enumeration;

typedef struct IHS_EnumerationClass {
    IHS_Enumeration *(*alloc)(const struct IHS_EnumerationClass *cls, void *arg);

    void (*free)(IHS_Enumeration *enumeration);

    size_t (*count)(const IHS_Enumeration *enumeration);

    void (*reset)(IHS_Enumeration *enumeration);

    bool (*ended)(const IHS_Enumeration *enumeration);

    void *(*get)(const IHS_Enumeration *enumeration);

    void *(*next)(IHS_Enumeration *enumeration);

} IHS_EnumerationClass;

typedef struct IHS_Enumeration {
    const IHS_EnumerationClass *cls;
} IHS_Enumeration;

typedef void *(*IHS_EnumerationLinkedListNext)(void *cur);

typedef void (*IHS_EnumerationFreeUnderlying)(void *p);

IHS_Enumeration *IHS_EnumerationCreate(const IHS_EnumerationClass *cls, void *arg);

IHS_Enumeration *IHS_EnumerationLinkedListCreate(void *ll, IHS_EnumerationLinkedListNext next,
                                                 IHS_EnumerationFreeUnderlying free);

IHS_Enumeration *IHS_EnumerationArrayCreate(void *array, size_t itemSize, size_t count,
                                            IHS_EnumerationFreeUnderlying free);

IHS_Enumeration *IHS_EnumerationEmptyCreate();

size_t IHS_EnumerationCount(const IHS_Enumeration *enumeration);