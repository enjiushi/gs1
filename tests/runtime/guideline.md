# tests/runtime/

Runtime-level tests and probes that validate queue flow, projections, and performance characteristics.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `game_runtime_message_chain_test.cpp`: Verifies chained gameplay message dispatch behavior and projected HUD/task payloads in the runtime.
- `game_runtime_performance_test.cpp`: Runtime performance-oriented test coverage.
- `game_runtime_projection_test.cpp`: Verifies projected runtime/UI state output, including regional-map support projection, prototype tech-tree UI projection from both the regional map and active site sessions, single-crate site bootstrap and loadout projection, tile-upsert delta coalescing for quantized visible density, local-wind, and ecology-driven terrain-appearance changes, weather-event timeline projection, tracked worker-pack/device-storage inventory-view presentation, authoritative phone home/app-panel state emission with task counts plus open-close visibility, inventory-phone-tech panel exclusivity behavior, phone listing add/remove projection behavior, phone section-change updates, and runtime delivery pacing under the dedicated time-system architecture.
- `local_weather_perf_probe.cpp`: Focused performance probe for local weather processing.
- `site_system_message_flow_test.cpp`: Verifies message flow between site systems through the runtime path using prototype campaign state plus dedicated campaign/site time systems.
