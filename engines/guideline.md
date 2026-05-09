# engines/

First-party engine client shells and engine-specific runtime adapter scaffolding.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `godot/`: First-party Godot client scaffolding, including the `godot-cpp`-first native runtime extension target whose bootstrap scene now keeps the shared runtime alive while routing between dedicated main-menu, regional-map, and site-session scenes, still preserving stable gameplay-key reconciliation for projected controls and regional/site visuals while relying on authored regional/menu backdrop and camp-marker scenes instead of native backdrop geometry generation, plus the Godot project shell with the scrollable tabbed research overlay.
