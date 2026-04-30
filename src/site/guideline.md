# src/site/

Site-owned state, ECS world support, and helper logic for active site gameplay.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `action_state.h`: Site action queue/state owned by action execution flows, including deferred-placement mode tracking, deferred worker-meter-cost snapshots captured at action start and applied on completion, harvest plus excavate action state, resolved multi-item harvest outputs locked at action start, and follow-up plant-mode reactivation after successful item-based planting when inventory remains.
- `contractor_state.h`: Contractor/worker assignment state for the site run.
- `craft_logic.h`: Shared crafting rules/helper declarations used by craft-related systems, now including campaign-aware recipe filtering plus hammer-gated device/tool crafting checks, with internal nearby-item caches carried as full 64-bit item entity ids.
- `craft_logic.cpp`: Shared crafting rules/helper implementation kept out of headers to reduce rebuild fan-out, including technology-gated recipe availability for Village-exclusive crafting content plus shared hammer prerequisite checks for device/tool craft outputs, with nearby-item collection now preserving full entity ids instead of truncating them through 32-bit cache paths.
- `craft_state.h`: Crafting queue/progress state for the active site, including cached nearby-item snapshots for crafting stations plus the last inventory/worker inputs used to refresh them, with both craft-station and phone inventory caches now storing full 64-bit item entity ids.
- `defs/`: Site-specific authored definition helpers, including harvest-capable site action metadata access.
- `device_weather_contribution_state.h`: Device weather-contribution runtime state, including retained dirty-target tracking plus cached wind-direction sector invalidation.
- `device_interaction_logic.h`: Helper rules for device interaction behavior.
- `economy_state.h`: Site phone-storefront state, including per-site-session transient cash, content-authored phone delivery settings, generated buy-stock refresh generation tracking, and the remaining revealed/direct-purchase unlockable containers now left empty by the rep-only progression setup unless a later task explicitly uses them.
- `event_state.h`: Site event state for active/local event tracking, including absolute event timeline markers and active pressure channels for weather interpolation.
- `inventory_state.h`: Inventory ownership/state for items in the site run, including harvested-output insertion into the worker pack, the pending overflow queue for delivery-crate inserts that could not fit yet, and the tracked worker-pack/device-storage panel visibility state projected to hosts.
- `inventory_storage.h`: Inventory container/storage primitive/helper declarations shared across runtime and tests.
- `inventory_storage.cpp`: Inventory container/storage primitive/helper implementation kept out of headers to narrow incremental rebuilds, with container item-id collection now re-reading live slot entities so stale slot references do not leak truncated or dead item ids into downstream caches.
- `local_weather_resolve_state.h`: Local weather resolution runtime state, including the last aggregated per-tile contribution snapshot plus deferred full-snapshot emission on site start.
- `modifier_state.h`: Modifier application state for site effects, including resolved generic meter channels with timed-buff-cap bias support, resolved terrain-factor weight/bias controls, resolved worker action-cost modifiers, resolved harvest-output cash-point multiplier deltas, resolved Village-tech and Bureau-tech numeric effect snapshots consumed by owner systems, plus a shared active site-modifier list keyed only by `modifier_id` where permanent effects use `0` duration and timed effects retain authored duration plus remaining world minutes internally for hour-bucket projection.
- `phone_panel_state.h`: Authoritative phone home/app-panel section state plus tracked open/closed visibility and projected listing/task snapshot state for the active site, including live/pending-claim/claimed task counts.
- `plant_weather_contribution_state.h`: Plant weather-contribution runtime state, including retained dirty-target tracking plus cached wind-direction sector invalidation.
- `placement_preview.h`: Placement-preview state for build/placement UI flows.
- `site_projection_update_flags.h`: Dirty/update flags for projection refresh decisions.
- `site_objective_state.h`: Site objective mode/config state, including cash-target survival goal cash points, highway target-band metadata, green-wall connection masks, hold countdown state, and paused main-timer tracking for objective evaluation.
- `site_run_state.h`: Aggregate active site-run state that owns the site slice, including pending projection dirtiness, retained plant/device weather-contribution dirty state, cached last-emitted tile projection state for delta coalescing across visible tile data including protection channels and excavation-depth marks, local-wind-driven plant visuals, and ecology-owned counters such as living-plant stability status, with density-report bookkeeping now living on sparse active-ecology ECS components instead of dense site-run arrays.
- `site_world.h`: ECS/world wrapper declarations for the active site, including split plant-versus-device weather contribution state alongside ecology, per-tile excavation depth, local weather, and device data, now with a per-device fixed-integrity flag for special devices such as the delivery crate.
- `site_world.cpp`: ECS/world wrapper implementation and setup, including the default low-fertility/low-moisture startup tile ecology seed, owner-specific plant/device weather contribution component storage on tile entities, per-tile excavation-depth storage, per-device fixed-integrity ECS state, plus sparse active-ecology/report-state component sync on occupancy changes.
- `site_world_access.h`: Owner-scoped ECS access helpers for reading/writing site components, including per-tile excavation depth while staying light so broad site callers do not need a direct Flecs include path.
- `site_world_components.h`: ECS component structs attached to site entities, including per-tile excavation state plus sparse `ActiveEcologyTag`, `DirtyEcologyTag`, dirty-mask, ecology report-state data for occupied-tile iteration and batched ecology output, and the per-device fixed-integrity component used by maintenance.
- `systems/`: Site system declarations and implementations, including harvest action execution, ecology harvest resolution, worker-pack harvest insertion, rep-aware shop/crafting/task filtering, and the Village-specific shovel, excavation-depth, buff, and custom-food runtime hooks.
- `task_board_state.h`: Task-board state for current site objectives and listings, including the explicit pending-claim runtime state before claimed-history, per-task resolved generator outputs, stored difficulty/reward cash-point valuation fields, runtime accumulators, periodic normal-pool refresh generation/countdown state, masks, and the task-owned mirror caches updated from owner-emitted worker/tile/device progress messages.
- `tile_footprint.h`: Tile-footprint geometry helpers for structures, placement, and footprint-scaled contribution distance helpers.
- `weather_contribution_logic.h`: Shared local-weather helper logic for contribution falloff, snapped 8-direction wind-shadow sampling with full-lane plus half-diagonal lee shelter, wind-direction sector quantization, and plant/device contribution accumulation helpers.
- `weather_state.h`: Weather state tracked during the active site run.
