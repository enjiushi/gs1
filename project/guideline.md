# project/

Shared project-owned configuration and data roots that sit above any one engine adapter.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `adapter_metadata/`: Shared adapter-owned presentation metadata tables, including modifier plus progression display text and descriptions that hosts and engine adapters may load directly without making the gameplay DLL own that non-simulation copy, with the Godot build staging this folder into `engines/godot/project/gs1/runtime/adapter_metadata/` for packaged runs.
- `content/`: Shared gameplay-authored TOML configuration loaded by the gameplay DLL from the project root path supplied by the host or adapter, including campaign, site, unlock, tuning, task, reward, and technology tables that stay outside any engine-specific project shell, with the Godot build staging this folder into `engines/godot/project/gs1/runtime/content/` for packaged runs.

## Cross-Table Id Rule
- For any authored type family such as items, unlockables, modifiers, or technology nodes, the per-type id must be globally unique for that type and reused unchanged across every related table for that type.
- Reference tables, resource tables, description tables, and adapter-metadata tables must all point at the same shared per-type id instead of introducing table-local keys or recomputing composite identifiers.
