# engines/

First-party engine client shells and engine-specific runtime adapter scaffolding.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `godot/`: First-party Godot client scaffolding, including the `godot-cpp`-first native runtime extension target whose bootstrap scene now keeps the shared runtime alive while routing between dedicated main-menu, regional-map, and site-session scenes, preserves stable gameplay-key reconciliation for projected controls and regional/site visuals, can optionally expose a loopback-only semantic debug HTTP control port for repeatable automation without widget-level click synthesis, now lets the director explicitly preload the current regional tech-tree icon set and buffer/replay all engine messages during the loading-scene handoff before the regional map is activated while the regional tech-tree controller also prewarms its icon and tooltip caches to avoid the first overlay-open hitch, keeps regional blank-map clicks consumed through a shared pure-C++ click-policy helper so they do not leak to unrelated UI, relies on authored regional/menu backdrop and camp-marker scenes instead of native backdrop geometry generation, and now packages the site-session player-meter, phone, worker-pack, and opened-storage surfaces as authored sub-scenes instead of leaving those panel layouts inline in `site_session.tscn`, while still shipping the Godot project shell with the scrollable tabbed research overlay.
