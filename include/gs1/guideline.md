# include/gs1/

Public C ABI and shared runtime type declarations exposed outside the gameplay DLL.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `export.h`: DLL import/export macro definitions used by the public API headers.
- `game_api.h`: Main exported gameplay entry points and host-facing function declarations, now including direct read-only gameplay-state access through `gs1_get_game_state_view(...)` plus targeted tile lookup through `gs1_query_site_tile_view(...)`.
- `state_view.h`: ABI-safe read-only gameplay state surface built from trivial pointer/count/query-facing structs so hosts can inspect authoritative campaign/site state without reading internal C++ runtime containers directly, now also exposing active-site craft-context options plus placement-mode preview state for host-owned craft/placement panels.
- `status.h`: Public result/status codes returned across the DLL boundary.
- `system_test_api.h`: Public hooks for the standalone system-test host and test asset runners.
- `types.h`: Shared API structs, payloads, engine-message transport layouts, handles, and enums used by runtime callers, including the host-supplied shared project-config root pointer in `Gs1RuntimeCreateDesc` plus optional adapter-config JSON, host-side phone/inventory/protection/session input enums and payloads, typed UI-surface visibility transport for engine-owned panel shells such as the regional selection shell, tech overlay, inventory panel, phone panel, and protection overlay, distinct regional site-list and read-only regional loadout list ids, the newer typed progression-view transport for regional-map research/unlock data with compact per-entry ids, reputation requirements, factions, kinds, and action fields, tracked inventory-view events for worker-pack or device-storage open/close intents plus a neutral host inventory-slot-tap event that leaves use-versus-transfer resolution inside gameplay, the explicit `GS1_HOST_EVENT_SITE_SCENE_READY` adapter-to-runtime handshake that confirms a newly requested site scene has finished loading before gameplay emits bootstrap projections, compatibility aliases that keep older `Gs1HostEvent` and `Gs1EngineMessage` naming mapped onto the current host/runtime transport types, the reduced semantic runtime-message families used by the active Godot path, and the older projection-era transport declarations still retained as compatibility surface for smoke/viewer/test code while they await separate cleanup.
