# tests/runtime/

Runtime-level tests and probes that validate queue flow, projections, and performance characteristics.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `game_runtime_message_chain_test.cpp`: Verifies chained gameplay message dispatch behavior and projected HUD payloads in the runtime.
- `game_runtime_performance_test.cpp`: Runtime performance-oriented test coverage.
- `game_runtime_projection_test.cpp`: Verifies projected runtime/UI state output, including regional-map support projection, site bootstrap, inventory presentation, authoritative phone panel state emission, and phone listing add/remove projection behavior.
- `local_weather_perf_probe.cpp`: Focused performance probe for local weather processing.
- `site_system_message_flow_test.cpp`: Verifies message flow between site systems through the runtime path.
