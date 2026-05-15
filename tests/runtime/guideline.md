# tests/runtime/

Runtime-level tests and probes that validate split runtime flow, exported state-view/projection behavior, adapter policy helpers, and performance characteristics.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `game_runtime_projection_test.cpp`: Broad runtime projection/state-view regression covering campaign, regional-map, site, inventory, phone, tech-tree, task, weather, protection-overlay, and onboarding output seen by hosts and adapters.
- `game_runtime_message_chain_test.cpp`: Runtime message-chain regression covering multi-system gameplay message dispatch, task-claim ordering, cue emission, crafted-output follow-up, and host-facing runtime message ordering.
- `state_manager_test.cpp`: Focused resolver-registration coverage for `StateManager`, including default/active owner assignment, idempotent re-registration, duplicate-owner rejection, and untouched never-registered state sets.
- `timed_modifier_projection_test.cpp`: Focused regression covering active site-modifier projection and remaining-game-hour bucketing behavior.
- `site_system_message_flow_test.cpp`: Verifies cross-system site gameplay coordination through subscribed `GameMessage` flow on the real runtime path.
- `game_runtime_performance_test.cpp`: Runtime performance-oriented coverage for deterministic mixed-site seeding and update cost.
- `local_weather_perf_probe.cpp`: Focused local-weather performance probe with per-stage timing breakdowns for dense-cover investigation.
- `smoke_engine_host_threading_test.cpp`: Verifies smoke-host frame-thread command draining, publication behavior, timing telemetry, and key viewer asset contracts.
- `godot_debug_http_protocol_test.cpp`: Pure C++ regression for the Godot adapter's optional loopback debug HTTP command parser and semantic payload translation.
- `godot_director_scene_policy_test.cpp`: Pure C++ regression for Godot director scene-reuse/switch policy.
- `godot_regional_map_click_policy_test.cpp`: Pure C++ regression for regional-map click-consumption and selection-clearing policy.
- `godot_regional_site_selection_policy_test.cpp`: Pure C++ regression for explicit-site-only regional selection policy.
