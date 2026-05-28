# tests/runtime/

Runtime-level tests and probes that validate split runtime flow, exported state-view/projection behavior, adapter policy helpers, and performance characteristics.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `game_runtime_projection_test.cpp`: Current host-contract regression for `Gs1GameStateView` and `query_site_tile_view(...)`, covering campaign/site state-view materialization plus progression-entry and tile-query visibility on the post-projection runtime path.
- `game_runtime_message_chain_test.cpp`: Runtime message-chain regression covering multi-system gameplay message dispatch, task-claim ordering, cue emission, crafted-output follow-up, host-facing runtime message ordering, and debug-only semantic nested gameplay-message stack capture for inline dispatch chains as the runtime moves away from deferred internal drains.
- `game_runtime_semantic_message_stack_test.cpp`: Focused debug-runtime regression that directly exercises `RuntimeSemanticGameMessageScope`, locks the semantic nested gameplay-message stack capture, and confirms the stack unwinds cleanly after nested inline scopes exit.
- `runtime_system_trait_metadata_test.cpp`: Focused runtime-trait regression that locks compile-time `profile_id` / `fixed_step_order_value` / `emitted_runtime_messages` readers, asserts that the active `GameSystems` pack now declares runtime metadata across the board, validates the first compile-time runtime-to-host emitted-message manifest for the live system pack, confirms the runtime still falls back to virtual metadata for non-adopting systems during the migration, and now also verifies that already-migrated typed gameplay-message families no longer populate the legacy enum-keyed subscriber table while legacy envelopes for those families still dispatch successfully.
- `state_manager_test.cpp`: Focused resolver-registration coverage for `StateManager`, including default/active owner assignment, idempotent re-registration, duplicate-owner rejection, untouched never-registered state sets, and the mutating-system stack behavior used to preserve correct owner tracking across nested inline gameplay-message dispatch.
- `timed_modifier_projection_test.cpp`: Focused regression covering active site-modifier projection and remaining-game-hour bucketing behavior.
- `site_system_message_flow_test.cpp`: Verifies cross-system site gameplay coordination through subscribed `GameMessage` flow on the real runtime path, including split-state fixture refreshes that preserve the live `SiteWorld` contract for active site runs without rebuilding temporary aggregates.
- `game_runtime_performance_test.cpp`: Runtime performance-oriented coverage for deterministic mixed-site seeding and update cost.
- `local_weather_perf_probe.cpp`: Focused local-weather performance probe with per-stage timing breakdowns for dense-cover investigation, now using in-place split-state fixture refreshes after each run.
- `smoke_engine_host_threading_test.cpp`: Verifies smoke-host frame-thread command draining, publication behavior, timing telemetry, and key viewer asset contracts.
- `godot_debug_http_protocol_test.cpp`: Pure C++ regression for the Godot adapter's optional loopback debug HTTP command parser and semantic payload translation.
- `godot_director_scene_policy_test.cpp`: Pure C++ regression for Godot director scene-reuse/switch policy.
- `godot_regional_map_click_policy_test.cpp`: Pure C++ regression for regional-map click-consumption and selection-clearing policy.
- `godot_regional_site_selection_policy_test.cpp`: Pure C++ regression for explicit-site-only regional selection policy.
