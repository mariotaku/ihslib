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
#include <stdlib.h>
#include <string.h>

#include "test_session.h"
#include "ihslib.h"
#include "ihs_buffer.h"
#include "ihs_buffer_ext.h"
#include "session/channels/ch_control.h"
#include "session/channels/channel.h"
#include "protobuf/remoteplay.pb-c.h"

typedef struct callback_log_t {
    int set_caps_lock_calls;
    bool last_caps_lock;
    int set_keymap_calls;
    size_t last_keymap_count;
    IHS_KeymapEntry last_keymap_first;
} callback_log_t;

static void on_set_caps_lock(IHS_Session *session, bool pressed, void *context) {
    (void) session;
    callback_log_t *log = context;
    log->set_caps_lock_calls++;
    log->last_caps_lock = pressed;
}

static void on_set_keymap(IHS_Session *session, const IHS_KeymapEntry *entries, size_t count, void *context) {
    (void) session;
    callback_log_t *log = context;
    log->set_keymap_calls++;
    log->last_keymap_count = count;
    if (count > 0) {
        log->last_keymap_first = entries[0];
    }
}

static const IHS_StreamInputCallbacks callbacks = {
        .setCapsLock = on_set_caps_lock,
        .setKeymap = on_set_keymap,
};

/* Pack a protobuf into an owned IHS_Buffer and dispatch through the control
 * channel's main switch. We construct a synthetic stack channel — only
 * channel->session is touched by the keyboard handlers. */
static void dispatch_ctrl(IHS_SessionChannel *channel, EStreamControlMessage type,
                          const ProtobufCMessage *message) {
    size_t size = protobuf_c_message_get_packed_size(message);
    IHS_Buffer buf;
    if (size > 0) {
        IHS_BufferInit(&buf, size, size);
        IHS_BufferAppendMessage(&buf, message);
    } else {
        /* An empty-payload message still needs a valid pointer for the
         * dispatcher's IHS_UNPACK_BUFFER macro. Allocate one byte. */
        IHS_BufferInit(&buf, 1, 1);
        IHS_BufferEnsureCapacityExact(&buf, 1);
    }
    IHS_SessionChannelControlOnMessageReceived(channel, type, &buf, NULL);
    IHS_BufferClear(&buf, true);
}

int main(void) {
    IHS_Init();
    IHS_Session *session = IHS_TestSessionCreate();
    assert(session != NULL);
    callback_log_t log;
    memset(&log, 0, sizeof(log));
    IHS_SessionSetInputCallbacks(session, &callbacks, &log);

    IHS_SessionChannel channel = {.session = session};

    /* SetCapslock = true */
    {
        CSetCapslockMsg msg = CSET_CAPSLOCK_MSG__INIT;
        msg.has_pressed = 1;
        msg.pressed = 1;
        dispatch_ctrl(&channel, k_EStreamControlSetCapslock, (const ProtobufCMessage *) &msg);
        assert(log.set_caps_lock_calls == 1);
        assert(log.last_caps_lock == true);
    }

    /* SetCapslock = false */
    {
        CSetCapslockMsg msg = CSET_CAPSLOCK_MSG__INIT;
        msg.has_pressed = 1;
        msg.pressed = 0;
        dispatch_ctrl(&channel, k_EStreamControlSetCapslock, (const ProtobufCMessage *) &msg);
        assert(log.set_caps_lock_calls == 2);
        assert(log.last_caps_lock == false);
    }

    /* SetCapslock with no `pressed` field — has_pressed = 0. Default to false
     * and fire the callback (some clients may emit the bare message). */
    {
        CSetCapslockMsg msg = CSET_CAPSLOCK_MSG__INIT;
        dispatch_ctrl(&channel, k_EStreamControlSetCapslock, (const ProtobufCMessage *) &msg);
        assert(log.set_caps_lock_calls == 3);
        assert(log.last_caps_lock == false);
    }

    /* SetKeymap with two entries. Verify the parsed array reaches the callback
     * with codepoints unchanged. */
    {
        CStreamingKeymapEntry e0 = CSTREAMING_KEYMAP_ENTRY__INIT;
        e0.has_scancode = 1;
        e0.scancode = 0x04;        /* USB HID 'a' */
        e0.has_normal_keycode = 1;
        e0.normal_keycode = 'a';
        e0.has_shift_keycode = 1;
        e0.shift_keycode = 'A';

        CStreamingKeymapEntry e1 = CSTREAMING_KEYMAP_ENTRY__INIT;
        e1.has_scancode = 1;
        e1.scancode = 0x35;        /* USB HID '`' */
        e1.has_normal_keycode = 1;
        e1.normal_keycode = '`';
        e1.has_shift_keycode = 1;
        e1.shift_keycode = '~';

        CStreamingKeymapEntry *entries[2] = {&e0, &e1};
        CStreamingKeymap keymap = CSTREAMING_KEYMAP__INIT;
        keymap.n_entries = 2;
        keymap.entries = entries;

        CSetKeymapMsg msg = CSET_KEYMAP_MSG__INIT;
        msg.keymap = &keymap;
        dispatch_ctrl(&channel, k_EStreamControlSetKeymap, (const ProtobufCMessage *) &msg);
        assert(log.set_keymap_calls == 1);
        assert(log.last_keymap_count == 2);
        assert(log.last_keymap_first.scancode == 0x04);
        assert(log.last_keymap_first.normal_keycode == 'a');
        assert(log.last_keymap_first.shift_keycode == 'A');
        /* Fields not set on the wire must be 0 in the parsed entry. */
        assert(log.last_keymap_first.capslock_keycode == 0);
        assert(log.last_keymap_first.altgr_shift_capslock_keycode == 0);
    }

    /* SetKeymap with no nested keymap (optional field omitted). Callback
     * must NOT fire, but the dispatcher must not crash either. */
    {
        CSetKeymapMsg msg = CSET_KEYMAP_MSG__INIT;
        dispatch_ctrl(&channel, k_EStreamControlSetKeymap, (const ProtobufCMessage *) &msg);
        assert(log.set_keymap_calls == 1);
    }

    /* SetKeymap with an empty entries list. Callback fires with count=0. */
    {
        CStreamingKeymap keymap = CSTREAMING_KEYMAP__INIT;
        CSetKeymapMsg msg = CSET_KEYMAP_MSG__INIT;
        msg.keymap = &keymap;
        dispatch_ctrl(&channel, k_EStreamControlSetKeymap, (const ProtobufCMessage *) &msg);
        assert(log.set_keymap_calls == 2);
        assert(log.last_keymap_count == 0);
    }

    IHS_SessionDestroy(session);
    IHS_Quit();
    return 0;
}
