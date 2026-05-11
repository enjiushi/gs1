# src/runtime/

Core runtime services that drive fixed-step execution, queue dispatch, clocks, and host integration.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `engine_feedback_buffer.h`: Buffer/storage for translated engine feedback events.
- `engine_message_queue.h`: Queue helpers for engine-originated messages entering gameplay.
- `fixed_step_runner.h`: Fixed-timestep runner used by the runtime update loop.
- `game_runtime.h`: Main runtime type declarations and update entry points, now trimmed so `GameRuntime` stays focused on host-event submission, message routing, fixed-step orchestration, profiling, and the `SITE_SCENE_READY` handshake while delegating game-specific bootstrap/projection follow-up to the app-layer presentation coordinator.
- `game_runtime.cpp`: Runtime loop implementation and routing core, including startup resolution of the shared project-config root, host-event-to-message translation, subscribed message dispatch, profiling, and fixed-step system scheduling, with the heavy regional/site projection, bootstrap, HUD/cue, and panel-policy code extracted into `src/app/game_presentation_coordinator.cpp`.
- `id_allocator.h`: Lightweight runtime ID allocation helpers.
- `message_queue.h`: Internal gameplay message queue declarations.
- `random_service.h`: Runtime random-number service/state wrapper.
- `runtime_clock.h`: Canonical runtime day-length constants plus real-seconds-to-in-game-minutes conversion helpers.
