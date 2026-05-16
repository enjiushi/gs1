# src/runtime/

Core runtime services that drive fixed-step execution, queue dispatch, clocks, and host integration.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `engine_message_queue.h`: Queue helpers for engine-originated messages entering gameplay.
- `fixed_step_runner.h`: Fixed-timestep runner used by the runtime update loop.
- `game_runtime.h`: Main runtime type declarations and split update entry points, with `GameRuntime` owning the `StateManager`, state-view cache, host-message queue, runtime-message queue access, and profiling toggles/snapshots while exposing the manager-owned game state through its compatibility alias.
- `game_state.h`: Compatibility state access alias for the runtime-owned authoritative state-set storage now held by `StateManager`, with gameplay messages and runtime messages remaining runtime-owned queues.
- `game_runtime.cpp`: Runtime loop implementation and routing core, including direct system creation, resolver registration, split host-message phase dispatch, internal gameplay message dispatch, profiling, fixed-step scheduling, and rebuilding/querying the public gameplay-state view from authoritative split state storage.
- `game_state_view.h` / `game_state_view.cpp`: Runtime-owned helpers that materialize the exported ABI-safe `Gs1GameStateView`, rebuild host-facing cache slices directly from flat authoritative state sets, flatten active-site craft-context and placement-preview data for host reads, and answer targeted tile queries without exposing internal runtime storage directly.
- `runtime_state_access.h`: Tagged runtime-state access declarations plus `RuntimeInvocation` plumbing for authoritative app/campaign/site state-set access, fixed-step timing, message queues, and move-direction snapshots, with the broad campaign compatibility caches removed from the production runtime path.
- `runtime_split_state_compat.h`: Shared split-state helper mappings for fine-grained state slices while the test-only aggregate round-trip bridge now lives under `tests/split_state_test_helpers.h`.
- `state_manager.h` / `state_manager.cpp`: Split-state resolver registration plus typed authoritative access for the authoritative app, campaign, and site state sets, with non-container wrappers cache-line aligned at 64 bytes and the manager now owning the aggregated `GameState`.
- `state_set.h`: State-set id inventory, cache-line-aligned wrapper templates, and compile-time 64-byte alignment enforcement for non-container state sets used by the state-resolver refactor, plus the separate non-aligned container wrapper for container-backed state sets and the pointer-free payload rule for both non-container states and container elements.
- `system_interface.h`: Shared runtime system interface and subscriber array declarations, now also carrying state-set ownership helpers that map explicit runtime claims and site-component ownership to `StateSetId` registration.
- `id_allocator.h`: Lightweight runtime ID allocation helpers.
- `message_queue.h`: Internal gameplay message queue declarations.
- `random_service.h`: Runtime random-number service/state wrapper.
- `runtime_clock.h`: Canonical runtime day-length constants plus real-seconds-to-in-game-minutes conversion helpers.
