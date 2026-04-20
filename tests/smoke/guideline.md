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
- `smoke_engine_host.cpp`: Smoke host implementation, including authoritative site phone panel state with completed/claimed task counts, weather-event timeline projection application, and logging names for regional-map tech-tree UI actions/setups.
- `smoke_host_main.cpp`: Standard smoke host executable entry point, including the optional `--verbose` switch for full engine logging.
- `smoke_live_http_server.h`: HTTP server interface for exposing live smoke state.
- `smoke_live_http_server.cpp`: HTTP server implementation for the visual smoke viewer.
- `smoke_live_state_json.cpp`: JSON serialization of live smoke/runtime state, including phone panel section/count snapshot fields, claimed-task list-kind names, weather-event timeline fields, and regional-map tech-tree tab/action/setup names.
- `smoke_log.h`: Smoke logging helper declarations.
- `smoke_log.cpp`: Smoke logging helper implementation.
- `smoke_script_directive.h`: Parsed smoke-script directive/value definitions.
- `smoke_script_runner.h`: Smoke script runner interface.
- `smoke_script_runner.cpp`: Smoke script runner implementation, including tech-tree UI action parsing for scripts.
- `visual_smoke_host_main.cpp`: Visual smoke host executable entry point, including tech-tree UI action parsing for browser-driven clicks.
- `visual_smoke_host_main.cpp`: Visual smoke host executable entry point, including `--port`, optional `--verbose` handling, and tech-tree UI action parsing for browser-driven clicks.
- `visual_smoke_live.html`: Browser UI shell and HUD/panel styling for the live visual smoke viewer, including the phone panel layout, section scrolling styles, and centered site-result modal treatment.
- `visual_smoke_live_viewer.js`: Client-side scene, HUD, inventory, storage, phone panel, centered site-result overlay, and interaction logic for the live visual smoke viewer, including DLL-driven phone panel section switching, faction-tab tech-tree overlay rendering with bottom-to-top tier stacking plus 3-across tech rows and left/right amplification icons, plus simplified channel-separated weather/VFX presentation with the post-process output converted through the renderer color-space path.
