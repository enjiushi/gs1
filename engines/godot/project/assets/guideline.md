# engines/godot/project/assets/

Google-Storage-backed Godot project art and presentation resources for the first-party client shell, with the local repo copy intentionally reduced to the currently referenced subset while the full payload can live in external storage.

## Usage
- Read this file before scanning the folder in detail.
- The payload in this folder is restored by `tools/sync_gcs_content.ps1` from the snapshot pinned in `tools/gcs_content.json`.
- Keep `guideline.md` in Git, but treat the actual asset files under this folder as external content that belongs in Google Cloud Storage rather than Git.
- When pruning or restoring this folder, keep the local copy aligned with the Godot project's currently referenced resources and preserve any fuller archive outside the repo or in object storage.

## Contents
- `field_command/`: Curated UI textures and font resources used by the shared shell themes.
- `menu_backdrop/`: Desert backdrop meshes, textures, HDRI resources, and extracted `.res` support files used by the menu and regional-map shell scenes.
- `characters/`: Minimal set of derived animation resources still used by the current menu backdrop, with larger source packs expected to live in external storage when not actively referenced.
- `terrain/`: Terrain textures and related supporting Godot resources used by the shell-scene terrain presentation.
- `shaders/`: Presentation-only Godot shaders for shell scenes.
- `trees/`: The currently referenced tree pack variant used by the tracked tree support scene.
- Theme `.tres` files and other authored Godot resources: Shared shell styling and other presentation resources referenced directly by the tracked project scenes.
