/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2026 Mariotaku <https://github.com/mariotaku>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

/**
 * What the buffer does when it cannot grow. Growth used to assert: release builds carried on with a
 * null data pointer, debug builds killed the process. Both are now a plain refusal that leaves the
 * buffer exactly as it was.
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "ihs_buffer.h"
#include "ihs_buffer_ext.h"

static void TestGrowthWithinBounds(void) {
    IHS_Buffer buffer;
    IHS_BufferInit(&buffer, 16, 1024);

    assert(IHS_BufferEnsureCapacity(&buffer, 16));
    assert(buffer.capacity >= 16);
    assert(IHS_BufferAppendMem(&buffer, (const uint8_t *) "hello", 5) == 5);
    assert(buffer.size == 5);

    // Doubling from 16 would land on 1024 exactly; anything up to the ceiling is fine.
    assert(IHS_BufferEnsureCapacity(&buffer, 1024));
    assert(buffer.capacity == 1024);
    assert(memcmp(IHS_BufferPointer(&buffer), "hello", 5) == 0);

    IHS_BufferClear(&buffer, true);
}

static void TestRefusesPastMaxCapacity(void) {
    IHS_Buffer buffer;
    IHS_BufferInit(&buffer, 16, 64);
    assert(IHS_BufferAppendMem(&buffer, (const uint8_t *) "hello", 5) == 5);

    size_t capacityBefore = buffer.capacity;
    const uint8_t *dataBefore = buffer.data;

    // The ceiling bounds what a remote peer can make this side allocate, so passing it is refused
    // rather than asserted — a corrupt or hostile frame length must not be able to abort a build
    // with assertions on, nor over-allocate in one without.
    assert(!IHS_BufferEnsureCapacity(&buffer, 65));
    assert(!IHS_BufferEnsureCapacityExact(&buffer, 65));
    assert(!IHS_BufferEnsureMaxSize(&buffer, 65));

    // Nothing moved.
    assert(buffer.capacity == capacityBefore);
    assert(buffer.data == dataBefore);
    assert(buffer.size == 5);
    assert(memcmp(IHS_BufferPointer(&buffer), "hello", 5) == 0);

    IHS_BufferClear(&buffer, true);
}

static void TestWritesDegradeToNoOp(void) {
    IHS_Buffer buffer;
    IHS_BufferInit(&buffer, 8, 8);
    assert(IHS_BufferAppendMem(&buffer, (const uint8_t *) "1234", 4) == 4);

    // A caller that ignores the result writes nothing instead of dereferencing null.
    assert(IHS_BufferPointerForAppend(&buffer, 16) == NULL);
    assert(IHS_BufferAppendMem(&buffer, (const uint8_t *) "567890abcdef", 12) == 0);
    assert(IHS_BufferWriteMem(&buffer, 4, (const uint8_t *) "567890abcdef", 12) == 0);
    assert(IHS_BufferFillMem(&buffer, 4, 0xAA, 12) == 0);
    assert(!IHS_BufferSetSuffixLength(&buffer, 16));
    assert(buffer.suffix == 0);

    // The four bytes that did fit are still intact and still the whole content.
    assert(buffer.size == 4);
    assert(memcmp(IHS_BufferPointer(&buffer), "1234", 4) == 0);

    // The fixed-width helpers refuse in the same way rather than writing through a null pointer.
    assert(IHS_BufferAppendUInt32LE(&buffer, 0x11223344) == 4);
    assert(buffer.size == 8);
    assert(IHS_BufferAppendUInt8(&buffer, 0xFF) == 0);
    assert(IHS_BufferAppendUInt16LE(&buffer, 0xBEEF) == 0);
    assert(IHS_BufferAppendSInt16LE(&buffer, -1) == 0);
    assert(IHS_BufferAppendUInt32LE(&buffer, 0) == 0);
    assert(buffer.size == 8);

    IHS_BufferClear(&buffer, true);
}

static void TestAllocatorRefusal(void) {
    IHS_Buffer buffer;
    // No ceiling, so the only thing that can say no is the allocator.
    IHS_BufferInit(&buffer, 16, 0);
    assert(IHS_BufferAppendMem(&buffer, (const uint8_t *) "hello", 5) == 5);

    const uint8_t *dataBefore = buffer.data;
    size_t capacityBefore = buffer.capacity;

    // A quarter of the address space: no allocator satisfies this, and the doubling loop has to
    // walk all the way up without spinning to find that out. (Valgrind reports anything with the
    // sign bit set as a "fishy" allocation size, hence /4 rather than a rounder SIZE_MAX.)
    assert(!IHS_BufferEnsureCapacity(&buffer, SIZE_MAX / 4));
    assert(!IHS_BufferEnsureCapacityExact(&buffer, SIZE_MAX / 4));

    // realloc left the old block alone, so the buffer is still whole and still usable.
    assert(buffer.data == dataBefore);
    assert(buffer.capacity == capacityBefore);
    assert(memcmp(IHS_BufferPointer(&buffer), "hello", 5) == 0);
    assert(IHS_BufferAppendMem(&buffer, (const uint8_t *) "!", 1) == 1);
    assert(buffer.size == 6);

    IHS_BufferClear(&buffer, true);
}

int main() {
    TestGrowthWithinBounds();
    TestRefusesPastMaxCapacity();
    TestWritesDegradeToNoOp();
    TestAllocatorRefusal();
    return 0;
}
