# engines/godot/project/

The first-party Godot game project shell that will host the GS1 runtime extension and Godot-authored content.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `addons/`: Godot addon/plugin payloads restored from the pinned Google Cloud Storage snapshot so `project.godot` can enable them without storing the heavy plugin tree in Git, with the local working copy trimmed to the plugin subset currently enabled by the tracked project.
- `assets/`: Shared Godot-authored presentation resources restored from the pinned Google Cloud Storage snapshot so the project scenes can keep referencing `res://assets/...` paths while Git only carries the manifest and sync workflow, with the local working copy trimmed to the asset subset currently referenced by the tracked project scenes and resources.
- `config/`: Source-authored Godot-only TOML presentation config that is staged into the ignored `gs1/godot_config/` package root for runtime loading, currently mapping shared tech ids, unlockable content ids, general shared content ids such as items/plants/structures/recipes, and broader render-resource domains such as modifiers, task templates, reward candidates, and sites to Godot icon, thumbnail, and scene resources while leaving descriptions in the shared repo-level gameplay content tables.
- `gs1/`: Ignored staged runtime package root produced by the Godot adapter build, containing `runtime/content/`, `runtime/adapter_metadata/`, and `godot_config/` copies so the Godot project can boot from a self-contained package instead of the repo-level authored roots.
- `project.godot`: Root Godot project configuration for the current GS1 visual client shell, now launching the routed bootstrap scene that keeps the gameplay runtime alive while swapping state-specific UI scenes.
- `extensions/`: Runtime extension packaging files consumed by the Godot project.
- `scripts/`: Reserved for any unavoidable Godot project scripts, currently reduced to guidelines because the main client shell controller now lives in the native plugin layer.
- `scenes/`: Godot-authored scenes for the client shell, now split between a bootstrap router scene, dedicated main-menu/regional-map/site-session shells, the reusable site HUD, the authored player-meter, phone, worker-pack, and opened-storage site-session panel sub-scenes, the regional-map HUD panel scene, and additional authored regional-map support scenes for the dune-basin backdrop and camp-style site marker, all following gameplay-projected app-state/UI setup messages while sharing the project-authored desert shell theme resource, with the menu now presenting its authored 3D vista through a dedicated `SubViewport` layer plus the shared heat-haze language.
