# engines/godot/project/scenes/site_placeholders/

Placeholder site-session world visuals keyed from gameplay content IDs through `engines/godot/project/config/content_resources.toml`.

## Usage
- Read this file before editing placeholder site visuals.
- Keep placeholder scenes script-free and rooted at `Node3D` so the native `Gs1GodotSiteSceneController` can instance them directly.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `plants/`: Plant placeholder scenes, one scene per authored plant type.
- `devices/`: Device/structure placeholder scenes, one scene per authored buildable structure type.
