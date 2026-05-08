# engines/godot/

Godot-specific client scaffolding for the current first-party visual runtime path, with the native adapter now centered on `godot-cpp` classes that own Godot-side scene manipulation while the gameplay DLL stays behind the shared host/session bridge.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `native/`: Godot runtime-extension native source files built by the repo CMake configuration, including the persistent `godot-cpp` runtime node, the routed per-screen shell controls that now stay closer to scene-global input/runtime hookup/world-node duties while the small direct-subscriber controllers for status, site HUD, inventory, phone, craft, tasks, fixed-slot/site actions, regional selection, overlay state, and the regional tech tree self-refresh from authoritative messages and own their own panel visibility plus panel-local button wiring where applicable, the bootstrap scene router that swaps dedicated menu/regional/site scenes by projected app state, and the Godot-object presenter that maps gameplay IDs to live Godot nodes through drained engine messages with snapshot-safe keyed reconciliation, now with the regional map instancing authored camp markers and feeding the tabbed research overlay.
- `project/`: The actual Godot project shell, including the bootstrap router scene plus the dedicated main-menu, regional-map, and site-session scene assets, the reusable site HUD scene, the authored regional-map dune-basin backdrop and camp marker scenes, the runtime extension packaging files, and the engine-specific progression/content/render-resource TOML files that map gameplay tech ids plus unlockable, content, modifier, task-template, reward-candidate, and site ids to Godot icon, thumbnail, and scene asset paths.
