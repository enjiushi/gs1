# Runtime Message Removal Remaining Work

This document records what is still unfinished after the current runtime-message transport removal checkpoint.

## Done

- Removed the public `gs1_pop_runtime_message(...)` ABI entry point.
- Removed host DLL-loader and runtime-session support for runtime-message popping.
- Removed runtime-owned message queue storage and queue accessors from the core runtime path.
- Removed gameplay-side runtime-message emission helpers and their main call sites from active systems.
- Stopped the Godot adapter from draining gameplay runtime messages; it now runs from state/query reads plus a tiny adapter-local notification bus.
- Removed smoke / visual-smoke / runtime-message-chain style targets from the default build graph.
- Updated process-host and system-test fixture code so supported targets no longer require the removed transport.

## Still To Remove

### Public transport declarations

These are now mostly dead declarations and should be deleted or privatized:

- `include/gs1/types.h`
  - `Gs1EngineMessageType`
  - `Gs1EngineMessage*Data` payload structs
  - `Gs1RuntimeMessage`
  - `Gs1EngineMessage` alias
  - message-payload layout asserts and size asserts

### Legacy smoke sources

These files still contain the old transport model but are no longer in the active build graph:

- `tests/smoke/smoke_engine_host.h`
- `tests/smoke/smoke_engine_host.cpp`
- `tests/smoke/smoke_live_state_json.cpp`
- `tests/smoke/smoke_script_runner.cpp`
- `tests/smoke/smoke_script_directive.h`
- `tests/smoke/smoke_host_main.cpp`
- `tests/smoke/visual_smoke_host_main.cpp`
- `tests/smoke/visual_smoke_silent_onboarding.h`
- `tests/smoke/visual_smoke_silent_onboarding.cpp`
- `tests/smoke/smoke_live_http_server.h`
- `tests/smoke/smoke_live_http_server.cpp`

### Removed-target tests that still mention transport types

These are not part of the supported target set anymore, but still contain `Gs1RuntimeMessage`-based logic:

- `tests/runtime/game_runtime_message_chain_test.cpp`
- `tests/runtime/timed_modifier_projection_test.cpp`
- `tests/runtime/game_runtime_performance_test.cpp`
- `tests/runtime/smoke_engine_host_threading_test.cpp`
- `tests/runtime/site_system_message_flow_test.cpp`
- `tests/runtime/runtime_system_trait_metadata_test.cpp`

### Internal naming / cleanup

- `src/runtime/engine_message_queue.h` is now effectively dead and can be deleted.
- `engines/godot/native/gs1_godot_projection_types.h` still carries old `Gs1EngineMessage*Data`-based helper aliases that should be renamed or simplified once the public message structs are removed.
- `tests/process/game_process_registry.cpp` currently returns no registered scenarios; add a replacement process scenario later if needed.

## Current Supported Verified Targets

These were built successfully at this checkpoint:

- `gs1_game`
- `gs1_godot_director_scene_policy_test`
- `gs1_godot_debug_http_protocol_test`
- `gs1_godot_regional_map_click_policy_test`
- `gs1_godot_regional_site_selection_policy_test`
- `gs1_game_process_test_host`
- `gs1_system_test_host`

## Recommended Next Slice

1. Delete or privatize the remaining public runtime-message declarations in `include/gs1/types.h`.
2. Remove the dead smoke transport sources from the repository or convert them into state-view/query-based hosts if they must be kept.
3. Remove or rewrite the legacy runtime transport tests instead of keeping them dormant.
