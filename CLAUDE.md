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

1. **RE the Steam binary via a subagent.** Dispatch a `general-purpose` (or `Explore`) subagent over the ghidra MCP tools to trace the relevant function(s). Brief it on what you need: function names, expected behavior, magic numbers, retry counts, gating predicates, packet/field layouts, calling convention. Have the subagent **write its findings directly into the project draft item body** via `gh api graphql` `updateProjectV2DraftIssue` — verdicts as prose, decompiled snippets in fenced code blocks, address citations. The subagent must update the project, not just summarize back to you (that protects the main conversation context from huge decompiler dumps).
2. **Read the updated project body** to absorb the findings the subagent wrote.
3. **Implement** against the now-documented behavior. Run small RE follow-ups inline if a detail is missing; if you need a wider trace, dispatch another subagent.
4. **Commit** with a message that references what Steam does and why this change matches it.
5. **Mark the project item Done** (or delete it if the project has no Status field set up) once committed.

The cost of skipping step 1-2 is implementing a plausible-sounding-but-wrong version of Steam's behavior. The user's reference binary is the source of truth; the task title is not.

The subagent rule exists because decompiled function dumps are large — 1000+ lines per call. Running them through a subagent that writes the distilled findings to the project keeps your main context clean and gives the next session (or another agent) a permanent, structured record to read.

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

### Sanitizers (preferred for CI and non-WSL local dev)

CMake options:
- `-DIHSLIB_SANITIZE_ADDRESS=ON` (`-fsanitize=address` + `-static-libasan`)
- `-DIHSLIB_SANITIZE_UNDEFINED=ON` (`-fsanitize=undefined`)
- `-DIHSLIB_SANITIZE_THREAD=ON` (`-fsanitize=thread`, mutually exclusive with ASan)

The GitHub workflow runs two matrix jobs (ASan+UBSan, TSan) on every push.

### Valgrind (WSL fallback)

On WSL, ASan's leak detector hits `PTRACE_ATTACH` restrictions and segfaults at exit ~30% of runs, and TSan fails to start at all ("unexpected memory mapping"). Use Valgrind instead — it's a userspace emulator so neither limitation applies:

```sh
cmake -S . -B build/linux-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux-debug
cd build/linux-debug
for t in $(find . -name 'ihstest_*' -type f -executable); do
  valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=definite \
    --track-origins=yes --errors-for-leak-kinds=definite "$t"
done
```

Valgrind catches the same UAF/leak/uninitialised-read class as ASan; it doesn't catch threading races (use TSan in CI for those).
