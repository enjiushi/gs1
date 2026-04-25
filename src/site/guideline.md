# src/site/

Site-owned state, ECS world support, and helper logic for active site gameplay.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `action_state.h`: Site action queue/state owned by action execution flows, including deferred-placement mode tracking, harvest action state, plus follow-up plant-mode reactivation after successful item-based planting when inventory remains.
- `contractor_state.h`: Contractor/worker assignment state for the site run.
- `craft_logic.h`: Shared crafting rules/helpers used by craft-related systems.
- `craft_state.h`: Crafting queue/progress state for the active site.
- `defs/`: Site-specific authored definition helpers, including harvest-capable site action metadata access.
- `device_interaction_logic.h`: Helper rules for device interaction behavior.
- `economy_state.h`: Site-local economy/shop/finance state, including content-authored phone delivery settings plus revealed/direct-purchase unlockable tracking.
- `event_state.h`: Site event state for active/local event tracking, including absolute event timeline markers and active pressure channels for weather interpolation.
- `inventory_state.h`: Inventory ownership/state for items in the site run, including harvested-output insertion into the worker pack, the pending overflow queue for delivery-crate inserts that could not fit yet, and the tracked worker-pack/device-storage panel visibility state projected to hosts.
- `inventory_storage.h`: Inventory container/storage primitives and stack helpers.
- `local_weather_resolve_state.h`: Local weather resolution runtime state, including the last aggregated per-tile contribution snapshot plus deferred full-snapshot emission on site start.
- `modifier_state.h`: Modifier application state for site effects, including resolved generic meter channels plus resolved terrain-factor weight/bias controls.
- `phone_panel_state.h`: Authoritative phone home/app-panel section state plus tracked open/closed visibility and projected listing/task snapshot state for the active site, including live/completed/claimed task counts.
- `placement_preview.h`: Placement-preview state for build/placement UI flows.
- `site_projection_update_flags.h`: Dirty/update flags for projection refresh decisions.
- `site_objective_state.h`: Site objective mode/config state, including highway target-band metadata plus green-wall connection masks, hold countdown state, and paused main-timer tracking for objective evaluation.
- `site_run_state.h`: Aggregate active site-run state that owns the site slice, including pending projection dirtiness, cached last-reported tile-density values for cumulative ecology message gating, cached last-emitted tile projection state for delta coalescing across visible tile data, local-wind-driven plant visuals, and ecology-owned counters such as living-plant stability status.
- `site_world.h`: ECS/world wrapper declarations for the active site, including split plant-versus-device weather contribution state alongside ecology, local weather, and device data.
- `site_world.cpp`: ECS/world wrapper implementation and setup, including owner-specific plant/device weather contribution component storage on tile entities.
- `site_world_access.h`: Owner-scoped ECS access helpers for reading/writing site components.
- `site_world_components.h`: ECS component structs attached to site entities.
- `systems/`: Site system declarations and implementations, including harvest action execution, ecology harvest resolution, and worker-pack harvest insertion.
- `task_board_state.h`: Task-board state for current site objectives and listings, including claimed-history tracking plus per-task resolved generator outputs, stored difficulty/reward cash-point valuation fields, runtime accumulators, masks, and the task-owned mirror caches updated from owner-emitted worker/tile/device progress messages.
- `tile_footprint.h`: Tile-footprint geometry helpers for structures and placement.
- `weather_contribution_logic.h`: Shared local-weather helper logic for contribution falloff, directional wind-shadow sampling, and plant/device contribution accumulation helpers.
- `weather_state.h`: Weather state tracked during the active site run.
