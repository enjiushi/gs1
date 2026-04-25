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
- `game_runtime.cpp`: Runtime loop implementation, integration wiring, projected engine/HUD message emission, tile-upsert coalescing against the last emitted visible tile snapshot including fine plant-density buckets for withering hints plus quantized local-wind projection for plant-reactive viewers, temporary Site 1 starter-tile projection probe logs for sent-versus-suppressed tile upserts now marked as debug-level so activity-only smoke logging suppresses them, authoritative phone home/app-panel state plus tracked phone-open visibility and phone listing/task-count projection emission, tracked worker-pack and device-storage inventory-view projection with stable already-open device snapshots while the worker-pack panel toggles, site-panel exclusivity coordination between inventory, phone, and tech-tree state, claimed-task list-kind projection, site-result `OK` return action projection, regional-map support previews plus authored tile-position projection, prototype tech-tree UI projection available from both the regional map and active site sessions with one-by-one total-reputation plant unlocks, faction tech branches, spend-source tabs, surcharge-aware purchase costs, and top-down refunds, while treating batched ecology task-tracking summaries as internal gameplay traffic rather than direct projection triggers.
- `id_allocator.h`: Lightweight runtime ID allocation helpers.
- `message_queue.h`: Internal gameplay message queue declarations.
- `random_service.h`: Runtime random-number service/state wrapper.
- `runtime_clock.h`: Canonical runtime day-length constants plus real-seconds-to-in-game-minutes conversion helpers.
