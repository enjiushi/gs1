# src/app/

Application-facing glue that assembles campaign/site state and exposes the gameplay DLL API.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `app_state_machine.h`: High-level scene/app state transitions across menus, campaign, and site play.
- `campaign_factory.h`: Factory declarations for creating and wiring campaign state.
- `campaign_factory.cpp`: Campaign factory implementation and initialization wiring, including derived 8-neighbor regional-map adjacency from authored site tile coordinates.
- `game_presentation_coordinator.h`: App-layer coordinator declaration for campaign/site bootstrap, projection publishing, panel visibility state, and post-message presentation follow-up that used to live on `GameRuntime`.
- `game_presentation_coordinator.cpp`: App-layer implementation of runtime-facing UI/projection/bootstrap publishing, including regional-map and site snapshot emission, site dirty-flush handling, placement-preview projection, HUD/resource cues, panel coordination policy extracted out of `src/runtime/game_runtime.cpp`, and the host-message-side presentation follow-up now used for direct host UI/storage handling after several systems stopped synthesizing intermediary gameplay request messages.
- `game_api.cpp`: Exported C API implementation that bridges hosts into the runtime/app layer.
- `game_app.h`: Top-level gameplay app container that owns runtime-facing state.
- `game_bootstrap.h`: Startup/bootstrap helpers for constructing the app and initial scene state.
- `game_loop.h`: Per-frame loop coordination between host input, runtime update, and projection output.
- `scene_coordinator.h`: Scene switching/orchestration helpers between campaign and site flows.
- `site_run_factory.h`: Factory declarations for creating active site runs.
- `site_run_factory.cpp`: Site-run factory implementation, initialization wiring, deterministic coherent site-fertility seeding for runtime and presentation alignment, cash-target survival objective progress seeding from starting site cash, single-crate near-camp delivery bootstrap placement with the special fixed-integrity delivery crate flag, initialization of tile-projection cache state, and site-one onboarding state seeding such as the starter workbench plus authored near-camp starter plant patches, while deriving worker-pack slot growth from the current Village tier layout.
