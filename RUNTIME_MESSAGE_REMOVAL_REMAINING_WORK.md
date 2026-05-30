# Runtime Message Removal Remaining Work

This document records the final cleanup results from the runtime-message transport removal work.

## Done

- Removed the public `gs1_pop_runtime_message(...)` ABI entry point.
- Removed host DLL-loader and runtime-session support for runtime-message popping.
- Removed runtime-owned message queue storage and queue accessors from the core runtime path.
- Removed gameplay-side runtime-message emission helpers and their main call sites from active systems.
- Stopped the Godot adapter from draining gameplay runtime messages; it now runs from state/query reads plus a tiny adapter-local notification bus.
- Removed smoke / visual-smoke / runtime-message-chain style targets from the default build graph.
- Updated process-host and system-test fixture code so supported targets no longer require the removed transport.
- Removed the dead public runtime-message transport declarations from `include/gs1/types.h`; the remaining tiny Godot-side message schema now lives in `engines/godot/native/gs1_godot_notification_types.h`.
- Deleted the dormant legacy smoke / visual-smoke source tree and the dead PowerShell entry points that still targeted the removed hosts.
- Deleted the removed-target runtime tests that still depended on `Gs1RuntimeMessage`.
- Deleted the dead `src/runtime/engine_message_queue.h` header.
- Replaced the empty game-process registry with a supported `site1_onboarding` scenario that verifies Site 1 onboarding bootstrap through the public split-phase + state-view API.

## Remaining Notes

- No remaining legacy runtime-message transport naming is left in supported code paths; the surviving Godot-side notification schema is now fully adapter-local and uses Godot-specific notification/projection names.

## Current Supported Verified Targets

These were built or run successfully after the final cleanup:

- `gs1_game`
- `gs1_game_process_test_host`
- `site1_onboarding` process scenario via `gs1_game_process_test_host`
