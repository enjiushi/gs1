# project/

Shared project-owned configuration and data roots that sit above any one engine adapter.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `adapter_metadata/`: Shared adapter-owned presentation metadata tables, including modifier plus progression display text and descriptions that hosts and engine adapters may load directly without making the gameplay DLL own that non-simulation copy, with the Godot build staging this folder into `engines/godot/project/gs1/runtime/adapter_metadata/` for packaged runs, and now also carrying the Godot prewarm design plan plus the authored transition warmup catalog that currently covers the main-menu, regional-map, and site-session target scenes alongside the shared terrain/backdrop resource preloads for the native adapter.
- `content/`: Shared gameplay-authored TOML configuration loaded by the gameplay DLL from the project root path supplied by the host or adapter, including campaign, site, unlock, tuning, task, reward, and technology tables that stay outside any engine-specific project shell, with the Godot build staging this folder into `engines/godot/project/gs1/runtime/content/` for packaged runs.
- `shared_framework_process.md`: Standard workflow for extracting the reusable engine-agnostic runtime framework plus reusable Godot-side adapter/bootstrap helpers into the shared upstream repo, vendoring snapshots into game repos such as `gs1` and `gs2`, and keeping `godot-cpp` as a game-owned dependency instead of making the shared framework repo own duplicated Godot headers.

## Cross-Table Id Rule
- For any authored type family such as items, unlockables, modifiers, or technology nodes, the per-type id must be globally unique for that type and reused unchanged across every related table for that type.
- Reference tables, resource tables, description tables, and adapter-metadata tables must all point at the same shared per-type id instead of introducing table-local keys or recomputing composite identifiers.
