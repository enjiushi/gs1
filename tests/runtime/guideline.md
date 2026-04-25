# tests/runtime/

Runtime-level tests and probes that validate queue flow, projections, and performance characteristics.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `game_runtime_message_chain_test.cpp`: Verifies chained gameplay message dispatch behavior and projected task-completion flow in the runtime, including explicit test-side task seeding now that production Site 1 startup no longer auto-surfaces placeholder onboarding tasks.
- `game_runtime_performance_test.cpp`: Runtime performance-oriented test coverage.
- `game_runtime_projection_test.cpp`: Verifies projected runtime/UI state output, including regional-map support projection, authored regional-map tile positions and derived links, prototype tech-tree UI projection from both the regional map and active site sessions with one-by-one plant unlocks, faction tech branches, surcharge-aware spend-source tabs, and refund actions, single-crate site bootstrap and loadout projection, tile-upsert delta coalescing for fine visible plant-density buckets, local-wind, and ecology-driven terrain-appearance changes, weather-event timeline projection, tracked worker-pack/device-storage inventory-view presentation including the crate-open then worker-pack-open regression, authoritative phone home/app-panel state emission with task counts plus open-close visibility, explicit runtime-side task seeding after Site 1 bootstrap, inventory-phone-tech panel exclusivity behavior, phone listing add/remove projection behavior, phone section-change updates, and runtime delivery pacing under the dedicated time-system architecture.
- `local_weather_perf_probe.cpp`: Focused performance probe for local weather processing, including per-stage plant/device/resolve timing breakdowns for dense-cover debug investigation.
- `smoke_engine_host_threading_test.cpp`: Verifies the smoke host's queue-versus-published-snapshot split, including frame-thread draining of queued commands and coalesced site-move input submission.
- `site_system_message_flow_test.cpp`: Verifies message flow between site systems through the runtime path using prototype campaign state plus dedicated campaign/site time systems.
