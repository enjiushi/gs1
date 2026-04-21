# tests/smoke/

DLL-loading smoke harness, scripted smoke runner, live-state server, and visual smoke viewer support.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `assets/`: Static assets used by the visual smoke experience.
- `runtime_dll_loader.h`: Runtime DLL loader interface used by smoke hosts.
- `runtime_dll_loader.cpp`: Runtime DLL loader implementation.
- `scripts/`: Scripted smoke scenario files.
- `smoke_engine_host.h`: Smoke host abstraction over the gameplay DLL runtime.
- `smoke_engine_host.cpp`: Smoke host implementation, including authoritative site phone home/app-panel state with completed/claimed task counts, site-tile local-wind projection intake for viewer-reactive plants, weather-event timeline projection application, and logging names for regional-map tech-tree UI actions/setups.
- `smoke_host_main.cpp`: Standard smoke host executable entry point, including the optional `--verbose` switch for full engine logging.
- `smoke_live_http_server.h`: HTTP server interface for exposing live smoke state.
- `smoke_live_http_server.cpp`: HTTP server implementation for the visual smoke viewer.
- `smoke_live_state_json.cpp`: JSON serialization of live smoke/runtime state, including phone home/app-panel section/count snapshot fields, claimed-task list-kind names, site-tile local-wind fields for plant animation, weather-event timeline fields, and regional-map tech-tree tab/action/setup names.
- `smoke_log.h`: Smoke logging helper declarations.
- `smoke_log.cpp`: Smoke logging helper implementation.
- `smoke_script_directive.h`: Parsed smoke-script directive/value definitions.
- `smoke_script_runner.h`: Smoke script runner interface.
- `smoke_script_runner.cpp`: Smoke script runner implementation, including tech-tree UI action parsing for scripts.
- `visual_smoke_host_main.cpp`: Visual smoke host executable entry point, including `--port`, optional `--verbose` handling, and tech-tree UI action parsing for browser-driven clicks.
- `visual_smoke_live.html`: Browser UI shell and HUD/panel styling for the live visual smoke viewer, including the simplified four-app phone home launcher, the enlarged handset, full-height app panels, inline SVG app and inventory icon surfaces, section scrolling styles, scene-specific site-vitals visibility, the tuned site-vitals palette used during site play, the centered site-result modal treatment, and the centered half-screen tech-tree overlay treatment shared by the regional map and active site sessions.
- `visual_smoke_live_viewer.js`: Client-side scene, HUD, inventory, storage, phone panel, centered site-result overlay, and interaction logic for the live visual smoke viewer, including delivery-box onboarding copy, SVG-backed inventory/context/phone/tech icons, checkerboard plant-action inventory metadata, stabilized context-menu rerender behavior during live site patches, dedicated dry-straw `2x2` checkerboard rendering with density-scaled straw height, DLL-driven phone home/app section switching with back/cart navigation, the simplified four-app root menu, phone task-card accept actions plus site-one onboarding task text/reward hints, faction-tab tech-tree overlay rendering for reputation-gated tiers with immediate unlockable rows and named technology cards, site-session tech-tree toggle support with stabilized overlay persistence across lightweight site patches and reliable primary-press DOM bindings for tech-tree controls, simplified channel-separated weather/VFX presentation with the post-process output converted through the renderer color-space path and spawned wind-stream streaks whose opacity, frequency, retained fade-out direction, and slight heading jitter scale with gust intensity, plant vertex deformation whose sway intensity follows projected local wind while gust direction follows site weather, plus the continuous desert site terrain with cached placement-only tile overlays.
