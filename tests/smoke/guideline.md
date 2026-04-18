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
- `smoke_engine_host.cpp`: Smoke host implementation.
- `smoke_host_main.cpp`: Standard smoke host executable entry point.
- `smoke_live_http_server.h`: HTTP server interface for exposing live smoke state.
- `smoke_live_http_server.cpp`: HTTP server implementation for the visual smoke viewer.
- `smoke_live_state_json.cpp`: JSON serialization of live smoke/runtime state.
- `smoke_log.h`: Smoke logging helper declarations.
- `smoke_log.cpp`: Smoke logging helper implementation.
- `smoke_script_directive.h`: Parsed smoke-script directive/value definitions.
- `smoke_script_runner.h`: Smoke script runner interface.
- `smoke_script_runner.cpp`: Smoke script runner implementation.
- `visual_smoke_host_main.cpp`: Visual smoke host executable entry point.
- `visual_smoke_live.html`: Browser UI for the live visual smoke viewer.
- `visual_smoke_live_viewer.js`: Client-side logic for the live visual smoke viewer.
