# include/gs1/

Public C ABI and shared runtime type declarations exposed outside the gameplay DLL.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `export.h`: DLL import/export macro definitions used by the public API headers.
- `game_api.h`: Main exported gameplay entry points and host-facing function declarations.
- `status.h`: Public result/status codes returned across the DLL boundary.
- `system_test_api.h`: Public hooks for the standalone system-test host and test asset runners.
- `types.h`: Shared API structs, payloads, engine-message transport layouts, handles, and enums used by runtime callers, including the host-supplied shared project-config root pointer in `Gs1RuntimeCreateDesc` plus optional adapter-config JSON, authoritative phone home/app-panel section transport plus tracked open-state and unread-badge flags, the newer panel-slot/list UI projection transport now used by the authored Godot main-menu and regional-map shells alongside the older semantic UI-setup batches still used by unmigrated panels, including distinct regional site-list and read-only regional loadout list ids, the new typed progression-view transport for regional-map research/unlock data with compact per-entry ids, reputation requirements, factions, kinds, and action fields, tracked inventory-view events for both the worker pack and device storage panels, pending-claim plus claimed task-list projection states, active site-modifier list projection with timed-modifier remaining-game-hour buckets plus end-modifier UI action transport, site-tile local-wind plus ecology-meter, device-integrity, excavation-depth, and protection-channel projection for viewer-reactive plant motion, terrain shading, excavation marks, and protection heatmaps including the occupied-only plant-density/device-integrity overlay mode, sparse site plant/device visual upsert-remove transport keyed by gameplay-owned visual IDs for engine-native instancing with explicit footprint, height-scale, and authored rendering-hint flags, HUD player-meter transport for health, hydration, nourishment, energy, and morale, campaign cash plus total/Village/Forestry/University reputation projection with player-facing cash floats over internal cash-point backing, weather-event timeline payloads, placement-preview impact-tile overlay transport for hypothetical build/plant shelter-and-number previews, a dedicated task-reward-claimed one-shot cue for host-side celebration panels/audio, site-protection selector and overlay UI transport, regional-map tech-tree UI setup/action identifiers including tech refunds, and harvest-action plus harvested-output transport payloads.
