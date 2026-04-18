# src/site/

Site-owned state, ECS world support, and helper logic for active site gameplay.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `action_state.h`: Site action queue/state owned by action execution flows.
- `contractor_state.h`: Contractor/worker assignment state for the site run.
- `craft_logic.h`: Shared crafting rules/helpers used by craft-related systems.
- `craft_state.h`: Crafting queue/progress state for the active site.
- `defs/`: Site-specific authored definition helpers.
- `device_interaction_logic.h`: Helper rules for device interaction behavior.
- `economy_state.h`: Site-local economy/shop/finance state.
- `event_state.h`: Site event state for active/local event tracking.
- `inventory_state.h`: Inventory ownership/state for items in the site run.
- `inventory_storage.h`: Inventory container/storage primitives and stack helpers.
- `local_weather_resolve_state.h`: Local weather resolution scratch/state.
- `modifier_state.h`: Modifier application state for site effects.
- `placement_preview.h`: Placement-preview state for build/placement UI flows.
- `site_projection_update_flags.h`: Dirty/update flags for projection refresh decisions.
- `site_run_state.h`: Aggregate active site-run state that owns the site slice.
- `site_world.h`: ECS/world wrapper declarations for the active site.
- `site_world.cpp`: ECS/world wrapper implementation and setup.
- `site_world_access.h`: Owner-scoped ECS access helpers for reading/writing site components.
- `site_world_components.h`: ECS component structs attached to site entities.
- `systems/`: Site system declarations and implementations.
- `task_board_state.h`: Task-board state for current site objectives and listings.
- `tile_footprint.h`: Tile-footprint geometry helpers for structures and placement.
- `weather_state.h`: Weather state tracked during the active site run.
