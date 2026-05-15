# src/runtime/

Core runtime services that drive fixed-step execution, queue dispatch, clocks, and host integration.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `engine_message_queue.h`: Queue helpers for engine-originated messages entering gameplay.
- `fixed_step_runner.h`: Fixed-timestep runner used by the runtime update loop.
- `game_runtime.h`: Main runtime type declarations and update entry points, with `GameRuntime` now owning the split gameplay `GameState`, the `StateManager`, and the exported read-only gameplay-state view cache while still exposing `RuntimeInvocation`-based helpers for tagged runtime-state access.
- `game_state.h`: Runtime-owned gameplay state-set storage container holding cache-line-aligned app/runtime state plus split campaign and active-site state sets, with gameplay messages and runtime messages remaining runtime-owned queues.
- `game_runtime.cpp`: Runtime loop implementation and routing core, including direct runtime-owned system creation, resolver registration, host-message subscriber dispatch, internal game-message dispatch, profiling, fixed-step scheduling, and rebuilding/querying the public gameplay-state view from split authoritative state storage.
- `game_state_view.h` / `game_state_view.cpp`: Runtime-owned helpers that materialize the exported ABI-safe `Gs1GameStateView`, track pointer/count-backed slice cache state, answer tile-level gameplay-state queries without exposing internal runtime storage directly, now rebuilding those exported read surfaces directly from split campaign/site state slices instead of reconstructing broad aggregate `CampaignState` or `SiteRunState` objects, and now also flatten active-site craft-context options plus placement-mode preview state into the exported host read surface.
- `runtime_state_access.h`: Tagged runtime-state access declarations plus `RuntimeInvocation` plumbing for split authoritative app/campaign/site state-set access, fixed-step timing, message queues, and move-direction snapshots without the old aggregate compatibility invocation cache layer.
- `runtime_split_state_compat.h`: Shared split-state-to-aggregate compatibility mapping helpers kept for test seeding/assertion helpers and the remaining narrow compatibility callers that still need full `CampaignState` or `SiteRunState` assembly outside the production runtime path.
- `state_manager.h` / `state_manager.cpp`: Split-state resolver registration plus typed authoritative access for the runtime-owned app, campaign, and site state sets, with non-container wrappers cache-line aligned at 64 bytes.
- `state_set.h`: State-set id inventory, cache-line-aligned wrapper templates, and compile-time 64-byte alignment enforcement for non-container state sets used by the state-resolver refactor.
- `state_manager.h` / `state_manager.cpp`: State-set resolver ownership registration plus direct typed access to the authoritative split runtime/campaign/site state storage.
- `system_interface.h`: Shared runtime system interface and subscriber array declarations, now also carrying state-set ownership helpers that map explicit runtime claims and site-component ownership to `StateSetId` registration.
- `id_allocator.h`: Lightweight runtime ID allocation helpers.
- `message_queue.h`: Internal gameplay message queue declarations.
- `random_service.h`: Runtime random-number service/state wrapper.
- `runtime_clock.h`: Canonical runtime day-length constants plus real-seconds-to-in-game-minutes conversion helpers.
