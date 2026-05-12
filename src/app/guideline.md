# src/app/

Application-facing glue that assembles campaign/site state and exposes the gameplay DLL API.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `app_state_machine.h`: High-level scene/app state transitions across menus, campaign, and site play.
- `campaign_presentation_ops.h`: Campaign/regional-map presentation follow-up declarations extracted from the old monolithic presentation coordinator.
- `campaign_presentation_ops.cpp`: Campaign presentation follow-up implementation that coordinates campaign resources, unlock cues, and site-result reactions while delegating the regional-map publisher work into its own module.
- `campaign_factory.h`: Factory declarations for creating and wiring campaign state.
- `campaign_factory.cpp`: Campaign factory implementation and initialization wiring, including derived 8-neighbor regional-map adjacency from authored site tile coordinates.
- `game_presentation_coordinator.h`: App-layer presentation facade declaration that now delegates campaign/regional and site-specific presentation follow-up into focused helper modules while still owning the low-level runtime-message emitters.
- `game_presentation_coordinator.cpp`: App-layer facade implementation that keeps shared runtime-message emit helpers plus site projection publishers while delegating higher-level post-message policy, site dirty-flush orchestration, and extracted regional-map publishers into smaller modules.
- `game_api.cpp`: Exported C API implementation that bridges hosts into the runtime/app layer.
- `game_app.h`: Top-level gameplay app container that owns runtime-facing state.
- `game_bootstrap.h`: Startup/bootstrap helpers for constructing the app and initial scene state.
- `game_loop.h`: Per-frame loop coordination between host input, runtime update, and projection output.
- `regional_map_presentation_publisher.cpp`: Dedicated regional-map publisher implementation extracted from the old coordinator for map menu, selection, tech-tree, and snapshot projection emission.
- `scene_coordinator.h`: Scene switching/orchestration helpers between campaign and site flows.
- `site_presentation_ops.h`: Site-presentation follow-up and dirty-flush orchestration declarations extracted from the old monolithic presentation coordinator.
- `site_presentation_ops.cpp`: Site-presentation follow-up implementation for protection/UI reactions, site-action and cue follow-up, and site dirty-flush orchestration around the existing low-level emitters on `GamePresentationCoordinator`.
- `site_run_factory.h`: Factory declarations for creating active site runs.
- `site_run_factory.cpp`: Site-run factory implementation, initialization wiring, deterministic coherent site-fertility seeding for runtime and presentation alignment, cash-target survival objective progress seeding from starting site cash, single-crate near-camp delivery bootstrap placement with the special fixed-integrity delivery crate flag, initialization of tile-projection cache state, and site-one onboarding state seeding such as the starter workbench plus authored near-camp starter plant patches, while deriving worker-pack slot growth from the current Village tier layout.
