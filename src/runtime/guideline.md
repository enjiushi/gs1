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
- `game_runtime.h`: Main runtime type declarations and update entry points.
- `game_runtime.cpp`: Runtime loop implementation, integration wiring, projected engine/HUD message emission, tile-upsert coalescing against the last emitted visible tile snapshot including quantized local-wind projection for plant-reactive viewers, authoritative phone home/app-panel state plus tracked phone-open visibility and phone listing/task-count projection emission, tracked worker-pack and device-storage inventory-view projection, site-panel exclusivity coordination between inventory, phone, and tech-tree state, claimed-task list-kind projection, site-result `OK` return action projection, regional-map support previews, and prototype tech-tree UI projection available from both the regional map and active site sessions with total-reputation plant tiers, neutral base-tech tiers, faction enhancement tabs, persistent-cash summaries, and named technology nodes.
- `id_allocator.h`: Lightweight runtime ID allocation helpers.
- `message_queue.h`: Internal gameplay message queue declarations.
- `random_service.h`: Runtime random-number service/state wrapper.
- `runtime_clock.h`: Canonical runtime day-length constants plus real-seconds-to-in-game-minutes conversion helpers.
