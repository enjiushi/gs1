# src/runtime/

Core runtime services that drive fixed-step execution, queue dispatch, clocks, and host integration.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `engine_message_queue.h`: Queue helpers for engine-originated messages entering gameplay.
- `fixed_step_runner.h`: Fixed-timestep runner used by the runtime update loop.
- `game_runtime.h`: Main runtime type declarations and update entry points, now exposing `RuntimeInvocation`-based helpers that let systems claim tagged runtime state access and, where still needed, build transitional site-system contexts from that invocation.
- `game_runtime.cpp`: Runtime loop implementation and routing core, including direct runtime-owned system creation, host-message subscriber dispatch, internal game-message dispatch, profiling, and fixed-step scheduling, with the old large enum-switch routing replaced by interface-pointer iteration.
- `system_interface.h`: Shared runtime system interface, tagged runtime-state access declarations, and `RuntimeInvocation` plumbing used by campaign and site systems to declare subscriptions and read/write only their claimed runtime slices.
- `id_allocator.h`: Lightweight runtime ID allocation helpers.
- `message_queue.h`: Internal gameplay message queue declarations.
- `random_service.h`: Runtime random-number service/state wrapper.
- `runtime_clock.h`: Canonical runtime day-length constants plus real-seconds-to-in-game-minutes conversion helpers.
