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
 * Deltas are a chain: the host applies each one to the state the previous one left it in, so a
 * delta may never be dropped — not even one whose bytes land back on the last flushed state.
 * A full report is self-contained and a repeat of it may be.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hid/report.h"

#define REPORT_LEN 48

/**
 * Press and release inside one poll tick. The two deltas net to zero, so the release used to be
 * dropped for matching the last flushed state — leaving the host holding the button, and every
 * later delta anchored to a state it was never in.
 */
static void test_press_release_in_one_tick(void) {
    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 1);
    IHS_HIDReportHolderSetReportLength(&holder, REPORT_LEN);

    uint8_t idle[REPORT_LEN], pressed[REPORT_LEN];
    memset(idle, 0, sizeof(idle));
    memcpy(pressed, idle, sizeof(pressed));
    pressed[0] = 0x01;

    // Flush an idle report, so lastSent == idle.
    IHS_HIDReportHolderAddFull(&holder, idle, REPORT_LEN);
    assert(IHS_HIDReportHolderGetMessage(&holder) != NULL);
    IHS_HIDReportHolderResetMessage(&holder);

    IHS_HIDReportHolderAddDelta(&holder, idle, pressed, REPORT_LEN);
    IHS_HIDReportHolderAddDelta(&holder, pressed, idle, REPORT_LEN);

    IHS_HIDDeviceReportMessage *message = IHS_HIDReportHolderGetMessage(&holder);
    assert(message != NULL);
    assert(message->n_reports == 2 && "the release must survive, or the host holds the button");
    assert(message->reports[0]->has_delta_report);
    assert(message->reports[1]->has_delta_report);
    assert(message->reports[0]->delta_report.data != message->reports[1]->delta_report.data);

    IHS_HIDReportHolderDeinit(&holder);
}

/** A repeated full report carries nothing new and breaks no chain, so it is still dropped. */
static void test_repeated_full_report_is_dropped(void) {
    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 1);
    IHS_HIDReportHolderSetReportLength(&holder, REPORT_LEN);

    uint8_t idle[REPORT_LEN];
    memset(idle, 0, sizeof(idle));

    IHS_HIDReportHolderAddFull(&holder, idle, REPORT_LEN);
    assert(IHS_HIDReportHolderGetMessage(&holder) != NULL);
    IHS_HIDReportHolderResetMessage(&holder);

    IHS_HIDReportHolderAddFull(&holder, idle, REPORT_LEN);
    assert(IHS_HIDReportHolderGetMessage(&holder) == NULL);

    IHS_HIDReportHolderDeinit(&holder);
}

/**
 * The host asking for a full report outranks the dedup: it wants the current state restated, so
 * an unchanged one is still an answer it must get.
 */
static void test_requested_full_report_beats_dedup(void) {
    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 1);
    IHS_HIDReportHolderSetReportLength(&holder, REPORT_LEN);

    uint8_t idle[REPORT_LEN];
    memset(idle, 0, sizeof(idle));

    IHS_HIDReportHolderAddFull(&holder, idle, REPORT_LEN);
    assert(IHS_HIDReportHolderGetMessage(&holder) != NULL);
    IHS_HIDReportHolderResetMessage(&holder);

    IHS_HIDReportHolderRequestFullReport(&holder);
    IHS_HIDReportHolderAddFull(&holder, idle, REPORT_LEN);
    IHS_HIDDeviceReportMessage *message = IHS_HIDReportHolderGetMessage(&holder);
    assert(message != NULL && "a requested full report must go out even when unchanged");
    assert(message->n_reports == 1);
    assert(message->reports[0]->has_full_report);

    // The request is served once, so the next unchanged report is dropped again.
    IHS_HIDReportHolderResetMessage(&holder);
    IHS_HIDReportHolderAddFull(&holder, idle, REPORT_LEN);
    assert(IHS_HIDReportHolderGetMessage(&holder) == NULL);

    IHS_HIDReportHolderDeinit(&holder);
}

int main(void) {
    test_press_release_in_one_tick();
    test_repeated_full_report_is_dropped();
    test_requested_full_report_beats_dedup();
    printf("hid report chain tests OK\n");
    return 0;
}
