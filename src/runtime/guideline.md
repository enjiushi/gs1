# src/runtime/

Core runtime services that drive fixed-step execution, queue dispatch, clocks, and host integration.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `engine_message_queue.h`: Queue helpers for engine-originated messages entering gameplay.
- `fixed_step_runner.h`: Fixed-timestep runner used by the runtime update loop.
- `game_runtime.h`: Main runtime type declarations and update entry points, with `GameRuntime` now owning the gameplay `GameState`, separate runtime-only presentation/session caches, and the exported read-only gameplay-state view cache while still exposing `RuntimeInvocation`-based helpers for tagged runtime-state access.
- `game_state.h`: Aggregate runtime-owned gameplay state container for app state, campaign/site state, gameplay message queue, runtime message queue, and fixed-step timing configuration, with presentation/session caches moved out so this struct trends toward gameplay-authoritative data only.
- `game_runtime.cpp`: Runtime loop implementation and routing core, including direct runtime-owned system creation, host-message subscriber dispatch, internal game-message dispatch now handled directly inside `GameRuntime`, profiling, fixed-step scheduling, and rebuilding/querying the public gameplay-state view.
- `game_state_view.h` / `game_state_view.cpp`: Runtime-owned helpers that materialize the exported ABI-safe `Gs1GameStateView`, track pointer/count-backed slice cache state, and answer tile-level gameplay-state queries without exposing internal runtime storage directly.
- `presentation_runtime_state.h`: Runtime-owned non-authoritative presentation caches such as last-emitted phone listing ids, coarse presentation-dirty revision tracking, and campaign unlock cue snapshots moved out of the old presentation coordinator object.
- `runtime_state_access.h`: Tagged runtime-state access declarations plus `RuntimeInvocation` plumbing used by campaign and site systems to read/write only their claimed runtime slices from either runtime-owned or test-built state views, now routing presentation-tag access through the dedicated runtime presentation state instead of `GameState`.
- `system_interface.h`: Shared runtime system interface and subscriber array declarations layered on top of the separate runtime-state access plumbing.
- `ui_presentation_state.h`: Runtime-owned UI presentation session state for active setup/panel/progression visibility bookkeeping and last-emitted app-state tracking.
- `id_allocator.h`: Lightweight runtime ID allocation helpers.
- `message_queue.h`: Internal gameplay message queue declarations.
- `random_service.h`: Runtime random-number service/state wrapper.
- `runtime_clock.h`: Canonical runtime day-length constants plus real-seconds-to-in-game-minutes conversion helpers.
