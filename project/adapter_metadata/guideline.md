# project/adapter_metadata/

Adapter-owned catalog tables for presentation-only metadata that should not be loaded by the gameplay DLL.

## Usage
- Read this file before scanning the folder in detail.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `godot_prewarm.toml`: Authored Godot-only prewarm transition config staged into the runtime package, currently defining explicit forward/back terrain-related warmup groups for the main-menu backdrop, regional-map backdrop, and site-session terrain textures by logical screen id so the native adapter can preload scene or resource targets without guessing policy.
- `godot_prewarm_plan.md`: Design plan for a Godot-adapter-authored transition-based prewarm system, covering explicit forward/back transition warmup targets, future `godot_prewarm.toml` schema direction, supported target kinds such as scene/resource/optional instantiate-once, and open questions for retained-resource policy and adapter integration.
- `modifier_presets.toml`: Adapter-facing modifier labels and descriptions keyed by shared `modifier_id`.
- `reputation_unlocks.toml`: Adapter-facing reputation-unlock labels and descriptions keyed by shared `unlock_id`.
- `technology_nodes.toml`: Adapter-facing technology-node labels and descriptions keyed by shared globally unique `tech_node_id` values that match the gameplay tech rows and every other technology-node reference table.
