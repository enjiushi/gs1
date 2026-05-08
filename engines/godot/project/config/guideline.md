# engines/godot/project/config/

Engine-specific Godot presentation config authored in-source and staged into the Godot package for the native adapter to load at runtime.

## Usage
- Read this file before scanning the folder in detail.
- Keep gameplay-authored names, descriptions, and progression meaning in the shared repo-level `project/content/` tables.
- Use this folder only for Godot-specific resource bindings such as icon texture paths.
- The Godot build stages this folder into the ignored `engines/godot/project/gs1/godot_config/` package root; treat this source-controlled folder as the authored source of truth.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `progression_resources.toml`: Godot-only progression resource map keyed by shared `tech_node_id` plus unlockable content kind/id pairs, currently pointing every tech and unlockable entry at the shared placeholder icon while the adapter-side loading path is established.
- `content_resources.toml`: Godot-only general content resource map keyed by shared `Item`, `Plant`, `Structure`, and `Recipe` ids so inventory, craft, placement-preview, and other non-progression UI surfaces can bind engine-local icons without moving names or descriptions out of the shared gameplay content tables, with plant and structure rows also pointing at script-free site-session placeholder scene paths until final art assets replace them.
- `render_resources.toml`: Godot-only domain/id render resource map for adapter-owned presentation bindings that are not just generic content icons, currently covering shared `Modifier`, `TaskTemplate`, `RewardCandidate`, and `Site` ids plus optional site thumbnails and site marker scene paths so the native adapter can resolve UI badges and world marker scenes through config instead of hardcoded asset choices.
