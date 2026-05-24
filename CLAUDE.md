# ihslib working notes for Claude

## Backlog lives in GitHub Projects

All triage / follow-up work is tracked at https://github.com/users/mariotaku/projects/4 ("ihslib backlog"). Single-select `Importance` field with `High` / `Medium` / `Low` options.

Field IDs for scripted updates:
- Project: `PVT_kwHOAAyrls4BYqD6`
- Importance field: `PVTSSF_lAHOAAyrls4BYqD6zhTuXQo`
- Option IDs: `High=b557e571`, `Medium=9e0f8009`, `Low=a0505a88`

## How to work a backlog item

**Reverse-engineer first, document in the project body, then implement. Never guess the implementation from a task title.**

For every task picked off the backlog:

1. **RE the Steam binary** for the relevant function(s) via the ghidra MCP server. Confirm the actual behavior — addresses, calling convention, magic numbers, retry counts, gating predicates, field layouts. Don't assume the title or prior summary is complete.
2. **Update the project draft item body** with the new findings: addresses, decompiled snippets in fenced blocks, suggested change refined to match what Steam actually does. Use the `updateProjectV2DraftIssue` mutation (`gh api graphql`).
3. **Implement** against the now-documented behavior.
4. **Commit** with a message that references what Steam does and why this change matches it.
5. **Mark the project item Done** (or delete it if the project has no Status field set up) once committed.

The cost of skipping step 1-2 is implementing a plausible-sounding-but-wrong version of Steam's behavior. The user's reference binary is the source of truth; the task title is not.

## Useful Ghidra entry points

The reference program in Ghidra is named `streaming_client`. A few high-value classes / address ranges seen so far:

- Video decoder: `CStreamDecoderVideo` ctor `0x20528c`, `DecodeFrame` `0x2055dc`, `OnThink` `0x205ba8`, `CheckOverflow` `0x205fc4`, `FlushPendingData` `0x206134`
- HID input: `CStreamPlayer::OnRemoteHIDMessage` `0x228a64`, `CHIDDeviceReportThread::Run` `0x20c8d0`, `RunHIDDeviceReportThread` `0x21d4c8`, `CHIDDeviceReportGenerator::BCollectReports` `0x240784`, `SendBuffer` `0x2454ec`
- Input channels: `CStreamClient::SendKeyDown` `0x1f99cc`, `SendMouseMotion` `0x1f910c`, `SendMouseDown` `0x1f9654`, `SendMouseWheel` `0x1f94ac`, `SendText` `0x1f9d4c`, `SendLatencyTest` `0x1f8a64`, `CreateInputMark` `0x1fb264`, `GetInputMarkIndex` `0x1f4e30`
- HID send: `CStreamClient::SendRemoteHIDMessage` `0x1fa1c8`
- Audio in: `BInitializeMicrophone` `0x21e490`, `SendMicrophoneData` `0x1f805c`, `CMicrophoneAudioEncoder` `0x19b2d8`
- Common gates: `IsStreaming`, `BStreamingInput`, `BStreamingMicrophone`

## Project-specific conventions

- Timestamps on the wire use `IHS_SessionPacketTimestamp()` units = `1/65536` second (see `src/session/packet.c:130`). Convert with `(value * 1000 / 65536)` for ms.
- `IHS_VideoPartialFrame.timestamp` carries the per-fragment sender timestamp (added for the 150 ms stall-detect work). Maintain when adding new code paths that build partial-frame nodes.
- Video frame header fields renamed away from Steam's misnomer: `reserved1` → `subFrameStart`, `reserved2` → `subFrameEnd`, `VideoFrameFlagReserved1Increment` → `VideoFrameFlagSubFrameAdvance`. The wire format and semantics are unchanged.

## Build / verify

```sh
cmake --build build/linux-debug
build/linux-debug/tests/session/video/ihstest_partial_frames
```

The clangd diagnostics surfaced by the editor are misconfigured include paths — pre-existing, ignore.
