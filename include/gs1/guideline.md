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
- `types.h`: Shared API structs, payloads, engine-message transport layouts, handles, and enums used by runtime callers, including authoritative phone home/app-panel section transport plus tracked open-state flags, tracked inventory-view events for both the worker pack and device storage panels, claimed-task list-kind projection, active site-modifier list projection with timed-modifier remaining-game-hour buckets plus end-modifier UI action transport, site-tile local-wind plus ecology-meter, excavation-depth, and protection-channel projection for viewer-reactive plant motion, terrain shading, excavation marks, and protection heatmaps, campaign cash/total-reputation projection with player-facing cash floats over internal cash-point backing, weather-event timeline payloads, site-protection selector and overlay UI transport, regional-map tech-tree UI setup/action identifiers including tech refunds, and harvest-action plus harvested-output transport payloads.
