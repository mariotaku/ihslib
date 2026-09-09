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
 * A holder accumulates many reports before the poll tick flushes them, and all three of its
 * containers — dataBuffer, reportItems, reportPointers — reallocate as it grows. Anything captured
 * as a raw pointer at Add time dangles once that happens, so GetMessage has to bind them.
 *
 * These cases push well past the first growth of each container (dataBuffer's lands at the 6th
 * 48-byte report, the two lists at the 15th) and then check that every pointer the message hands
 * out is live and carries the bytes that were added under it.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hid/report.h"
#include "ihs_buffer.h"

#define REPORT_LEN 48
#define REPORT_COUNT 40

/** Every pointer in the message must land inside dataBuffer and hold what was added. */
static void CheckMessage(IHS_HIDReportHolder *holder, size_t expectedCount,
                         const uint8_t expected[][REPORT_LEN], const bool *isFull) {
    IHS_HIDDeviceReportMessage *message = IHS_HIDReportHolderGetMessage(holder);
    assert(message != NULL);
    assert(message->n_reports == expectedCount);
    assert(message->reports != NULL);

    const uint8_t *bufStart = IHS_BufferPointer(&holder->dataBuffer);
    const uint8_t *bufEnd = bufStart + holder->dataBuffer.size;

    for (size_t i = 0; i < expectedCount; i++) {
        CHIDDeviceInputReport *item = message->reports[i];
        assert(item != NULL);
        // reportPointers must point at the live reportItems storage, not at a stale copy.
        assert(item == IHS_ArrayListGet(&holder->reportItems, i));
        assert(item->has_full_report == isFull[i]);

        const uint8_t *data = isFull[i] ? item->full_report.data : item->delta_report.data;
        size_t len = isFull[i] ? item->full_report.len : item->delta_report.len;
        assert(data != NULL);
        assert(data >= bufStart && data + len <= bufEnd && "report data must live inside dataBuffer");

        if (isFull[i]) {
            assert(len == REPORT_LEN);
            assert(memcmp(data, expected[i], REPORT_LEN) == 0 && "full report contents moved");
        } else {
            // A delta is opaque here, but its declared original size must survive.
            assert(item->has_delta_report_size && item->delta_report_size == REPORT_LEN);
        }
    }
}

/**
 * Full reports only. Each differs from the last, so the dedup keeps all of them, and each is
 * appended verbatim — the strongest check that nothing moved out from under the pointers.
 */
static void test_many_full_reports(void) {
    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 1);
    IHS_HIDReportHolderSetReportLength(&holder, REPORT_LEN);

    static uint8_t expected[REPORT_COUNT][REPORT_LEN];
    static bool isFull[REPORT_COUNT];
    for (size_t i = 0; i < REPORT_COUNT; i++) {
        memset(expected[i], (int) (i & 0xFF), REPORT_LEN);
        // Make byte 0 unique too, so no two reports are byte-identical.
        expected[i][0] = (uint8_t) i;
        isFull[i] = true;
        IHS_HIDReportHolderAddFull(&holder, expected[i], REPORT_LEN);
    }

    CheckMessage(&holder, REPORT_COUNT, expected, isFull);
    IHS_HIDReportHolderResetMessage(&holder);
    assert(IHS_HIDReportHolderGetMessage(&holder) == NULL);
    IHS_HIDReportHolderDeinit(&holder);
}

/**
 * Deltas, which take a different append path and reserve a different length. One byte changes per
 * step so the delta stays smaller than a full report and AddDelta does not fall back.
 */
static void test_many_delta_reports(void) {
    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 1);
    IHS_HIDReportHolderSetReportLength(&holder, REPORT_LEN);

    static uint8_t expected[REPORT_COUNT][REPORT_LEN];
    static bool isFull[REPORT_COUNT];
    uint8_t previous[REPORT_LEN];
    uint8_t current[REPORT_LEN];
    memset(previous, 0, sizeof(previous));
    memcpy(current, previous, sizeof(current));

    for (size_t i = 0; i < REPORT_COUNT; i++) {
        // One changed byte: the delta is a 6-byte mask plus one byte, well under the fallback bar.
        current[i % REPORT_LEN] = (uint8_t) (i + 1);
        memcpy(expected[i], current, REPORT_LEN);
        // The very first Add has no flushed state behind it, so it goes out as a full report.
        isFull[i] = (i == 0);
        if (i == 0) {
            IHS_HIDReportHolderAddFull(&holder, current, REPORT_LEN);
        } else {
            IHS_HIDReportHolderAddDelta(&holder, previous, current, REPORT_LEN);
        }
        memcpy(previous, current, sizeof(previous));
    }

    CheckMessage(&holder, REPORT_COUNT, expected, isFull);
    IHS_HIDReportHolderDeinit(&holder);
}

/** Reports keep accumulating correctly across a flush, since Reset clears the offsets too. */
static void test_reuse_after_reset(void) {
    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 1);
    IHS_HIDReportHolderSetReportLength(&holder, REPORT_LEN);

    static uint8_t expected[REPORT_COUNT][REPORT_LEN];
    static bool isFull[REPORT_COUNT];
    for (int round = 0; round < 3; round++) {
        for (size_t i = 0; i < REPORT_COUNT; i++) {
            memset(expected[i], (int) ((i + round) & 0xFF), REPORT_LEN);
            expected[i][0] = (uint8_t) i;
            expected[i][1] = (uint8_t) round;
            isFull[i] = true;
            IHS_HIDReportHolderAddFull(&holder, expected[i], REPORT_LEN);
        }
        CheckMessage(&holder, REPORT_COUNT, expected, isFull);
        IHS_HIDReportHolderResetMessage(&holder);
    }
    IHS_HIDReportHolderDeinit(&holder);
}

int main(void) {
    test_many_full_reports();
    test_many_delta_reports();
    test_reuse_after_reset();
    printf("hid report batch tests OK\n");
    return 0;
}
