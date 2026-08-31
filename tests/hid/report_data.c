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
#include "hid/report.h"

static void test_full_then_delta(void) {
    // Nothing is pressed
    uint8_t a[48] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    // Menu is pressed
    uint8_t b[48] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 3);

    IHS_HIDReportHolderSetReportLength(&holder, 65);

    assert(IHS_HIDReportHolderGetMessage(&holder) == NULL);

    IHS_HIDReportHolderAddFull(&holder, a, 48);
    IHS_HIDReportHolderAddDelta(&holder, a, b, 48);

    IHS_HIDDeviceReportMessage *report = IHS_HIDReportHolderGetMessage(&holder);
    assert(report->has_device);
    assert(report->device == 3);
    assert(report->n_reports == 2);
    assert(report->reports[0]->full_report.len == 48);
    assert(memcmp(report->reports[0]->full_report.data, a, 48) == 0);

    assert(report->reports[1]->delta_report_size == 48);
    assert(report->reports[1]->delta_report.len == 10);
    uint8_t deltaExpected[] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40};
    assert(memcmp(report->reports[1]->delta_report.data, deltaExpected, 10) == 0);
    assert(report->reports[1]->delta_report_crc == 406293423);

    IHS_HIDReportHolderResetMessage(&holder);

    IHS_HIDReportHolderDeinit(&holder);
}

// A state that has already been flushed is dropped when it is offered again unchanged — unless the
// host has asked for a full report, in which case the latch has to beat the dedup.
static void test_full_report_latch_beats_dedup(void) {
    uint8_t a[48] = {0};
    a[27] = 0x01;

    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 3);
    IHS_HIDReportHolderSetReportLength(&holder, 65);

    IHS_HIDReportHolderAddFull(&holder, a, 48);
    IHS_HIDReportHolderResetMessage(&holder);

    // Same bytes again: nothing changed, so nothing to say.
    IHS_HIDReportHolderAddFull(&holder, a, 48);
    assert(IHS_HIDReportHolderGetMessage(&holder) == NULL);

    // Host asks for a full report. Silence is not an acceptable answer.
    IHS_HIDReportHolderRequestFullReport(&holder);
    IHS_HIDReportHolderAddFull(&holder, a, 48);
    IHS_HIDDeviceReportMessage *report = IHS_HIDReportHolderGetMessage(&holder);
    assert(report != NULL);
    assert(report->n_reports == 1);
    assert(report->reports[0]->has_full_report);
    assert(report->reports[0]->full_report.len == 48);

    // The latch is one-shot: once served, the dedup is back in charge.
    IHS_HIDReportHolderResetMessage(&holder);
    IHS_HIDReportHolderAddFull(&holder, a, 48);
    assert(IHS_HIDReportHolderGetMessage(&holder) == NULL);

    IHS_HIDReportHolderDeinit(&holder);
}

// A latched request cannot be answered with a delta, however small the delta would be.
static void test_full_report_latch_downgrades_delta(void) {
    uint8_t a[48] = {0};
    uint8_t b[48] = {0};
    a[27] = 0x01;
    b[27] = 0x01;
    b[16] = 0x40;

    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 3);
    IHS_HIDReportHolderSetReportLength(&holder, 65);

    IHS_HIDReportHolderRequestFullReport(&holder);
    IHS_HIDReportHolderAddDelta(&holder, a, b, 48);

    IHS_HIDDeviceReportMessage *report = IHS_HIDReportHolderGetMessage(&holder);
    assert(report->n_reports == 1);
    assert(report->reports[0]->has_full_report);
    assert(!report->reports[0]->has_delta_report);
    assert(memcmp(report->reports[0]->full_report.data, b, 48) == 0);

    IHS_HIDReportHolderDeinit(&holder);
}

// Steam only takes the delta when deltaLen + 8 < fullLen. With a 48 byte report the mask is 6
// bytes, so the delta wins below 34 changed bytes and loses at or above it.
static void test_delta_only_when_smaller(void) {
    uint8_t a[48] = {0};
    uint8_t b[48] = {0};

    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 3);
    IHS_HIDReportHolderSetReportLength(&holder, 48);

    // 10 changed bytes -> delta is 6 + 10 = 16, and 16 + 8 < 48.
    memset(b, 0xAA, 10);
    IHS_HIDReportHolderAddDelta(&holder, a, b, 48);
    IHS_HIDDeviceReportMessage *report = IHS_HIDReportHolderGetMessage(&holder);
    assert(report->n_reports == 1);
    assert(report->reports[0]->has_delta_report);
    assert(report->reports[0]->delta_report.len == 16);
    IHS_HIDReportHolderResetMessage(&holder);

    // 40 changed bytes -> delta would be 6 + 40 = 46, and 46 + 8 >= 48, so send the full state.
    uint8_t c[48] = {0};
    memset(c, 0xBB, 40);
    IHS_HIDReportHolderAddDelta(&holder, b, c, 48);
    report = IHS_HIDReportHolderGetMessage(&holder);
    assert(report->n_reports == 1);
    assert(report->reports[0]->has_full_report);
    assert(!report->reports[0]->has_delta_report);
    assert(report->reports[0]->full_report.len == 48);
    assert(memcmp(report->reports[0]->full_report.data, c, 48) == 0);

    IHS_HIDReportHolderDeinit(&holder);
}

// The delta bitmask is indexed by byte position, so it is meaningless across a length change.
static void test_length_change_forces_full(void) {
    uint8_t a[48] = {0};
    uint8_t b[48] = {0};
    a[27] = 0x01;
    memcpy(b, a, 48);
    b[16] = 0x40;

    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 3);
    IHS_HIDReportHolderSetReportLength(&holder, 65);

    // Flush a 48 byte state so lastSentLen becomes 48.
    IHS_HIDReportHolderAddFull(&holder, a, 48);
    IHS_HIDReportHolderResetMessage(&holder);

    // Now a 32 byte state arrives. A delta against a 48 byte baseline would be garbage.
    IHS_HIDReportHolderAddDelta(&holder, a, b, 32);
    IHS_HIDDeviceReportMessage *report = IHS_HIDReportHolderGetMessage(&holder);
    assert(report->n_reports == 1);
    assert(report->reports[0]->has_full_report);
    assert(report->reports[0]->full_report.len == 32);

    IHS_HIDReportHolderDeinit(&holder);
}

int main() {
    test_full_then_delta();
    test_full_report_latch_beats_dedup();
    test_full_report_latch_downgrades_delta();
    test_delta_only_when_smaller();
    test_length_change_forces_full();
    return 0;
}