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
- `game_presentation_coordinator.h`: App-layer presentation facade declaration that is now trimmed down to the remaining low-level site projection emitters and dirty-flush helpers still used by runtime-owned presentation systems.
- `game_presentation_coordinator.cpp`: App-layer facade implementation that now keeps only the low-level site projection publishers plus dirty-flush helpers while campaign/regional and site follow-up behavior moves through runtime systems.
- `game_api.cpp`: Exported C API implementation that bridges hosts into the runtime/app layer.
- `game_app.h`: Top-level gameplay app container that owns runtime-facing state.
- `game_bootstrap.h`: Startup/bootstrap helpers for constructing the app and initial scene state.
- `game_loop.h`: Per-frame loop coordination between host input, runtime update, and projection output.
- `scene_coordinator.h`: Scene switching/orchestration helpers between campaign and site flows.
- `site_run_factory.h`: Factory declarations for creating active site runs.
- `site_run_factory.cpp`: Site-run factory implementation, initialization wiring, deterministic coherent site-fertility seeding for runtime and presentation alignment, cash-target survival objective progress seeding from starting site cash, single-crate near-camp delivery bootstrap placement with the special fixed-integrity delivery crate flag, initialization of tile-projection cache state, and site-one onboarding state seeding such as the starter workbench plus authored near-camp starter plant patches, while deriving worker-pack slot growth from the current Village tier layout.
