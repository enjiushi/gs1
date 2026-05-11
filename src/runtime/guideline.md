# src/runtime/

Core runtime services that drive fixed-step execution, queue dispatch, clocks, and host integration.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `engine_message_queue.h`: Queue helpers for engine-originated messages entering gameplay.
- `fixed_step_runner.h`: Fixed-timestep runner used by the runtime update loop.
- `game_runtime.h`: Main runtime type declarations and update entry points, now owning the temporary `GameRuntimeTempBridge` template helpers so existing systems can satisfy the new common interface while still pulling their legacy typed contexts on demand.
- `game_runtime.cpp`: Runtime loop implementation and routing core, including direct runtime-owned system creation, host-message subscriber dispatch, internal game-message dispatch, profiling, and fixed-step scheduling, with the old large enum-switch routing replaced by interface-pointer iteration.
- `system_interface.h`: Shared runtime system interface plus small implementation macros used by individual campaign/site systems so they can expose direct message/host subscription lists and use `GameRuntimeTempBridge` locally without a separate central runtime-system source file.
- `id_allocator.h`: Lightweight runtime ID allocation helpers.
- `message_queue.h`: Internal gameplay message queue declarations.
- `random_service.h`: Runtime random-number service/state wrapper.
- `runtime_clock.h`: Canonical runtime day-length constants plus real-seconds-to-in-game-minutes conversion helpers.
