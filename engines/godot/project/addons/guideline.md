# engines/godot/project/addons/

Google-Storage-backed Godot addon and plugin payloads used by the first-party client shell.

## Usage
- Read this file before scanning the folder in detail.
- The payload in this folder is restored by `tools/sync_gcs_content.ps1` from the snapshot pinned in `tools/gcs_content.json`.
- Keep `guideline.md` in Git, but treat the actual addon/plugin files under this folder as external content that belongs in Google Cloud Storage rather than Git.

## Contents
- `terrain_3d/`: The current Terrain3D addon tree referenced by `engines/godot/project/project.godot`.
