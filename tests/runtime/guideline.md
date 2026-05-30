# tests/runtime/

Runtime-level tests and probes that validate split runtime flow, exported state-view/projection behavior, adapter policy helpers, and performance characteristics.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `game_runtime_projection_test.cpp`: Current host-contract regression for `Gs1GameStateView` and `query_site_tile_view(...)`, covering campaign/site state-view materialization plus progression-entry and tile-query visibility on the post-projection runtime path while now entering the migrated campaign/progression families through direct typed `GameRuntime::handle_message(Message)` calls instead of packed legacy envelopes, and now also locking the translated site move/context/action/cancel host inputs that feed typed dispatch immediately.
- `game_runtime_semantic_message_stack_test.cpp`: Focused debug-runtime regression that directly exercises `RuntimeSemanticGameMessageScope`, locks the semantic nested gameplay-message stack capture, and confirms the stack unwinds cleanly after nested inline scopes exit.
- `state_manager_test.cpp`: Focused resolver-registration coverage for `StateManager`, including default/active owner assignment, idempotent re-registration, duplicate-owner rejection, untouched never-registered state sets, and the mutating-system stack behavior used to preserve correct owner tracking across nested inline gameplay-message dispatch.
- `local_weather_perf_probe.cpp`: Focused local-weather performance probe with per-stage timing breakdowns for dense-cover investigation, now using in-place split-state fixture refreshes after each run.
- `godot_debug_http_protocol_test.cpp`: Pure C++ regression for the Godot adapter's optional loopback debug HTTP command parser and semantic payload translation.
- `godot_director_scene_policy_test.cpp`: Pure C++ regression for Godot director scene-reuse/switch policy plus the current no-exclusive-snapshot engine-message policy used by the state/query-driven native adapter path.
- `godot_regional_map_click_policy_test.cpp`: Pure C++ regression for regional-map click-consumption and selection-clearing policy.
- `godot_regional_site_selection_policy_test.cpp`: Pure C++ regression for explicit-site-only regional selection policy.
