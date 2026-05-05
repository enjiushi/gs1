# project/adapter_metadata/

Adapter-owned catalog tables for presentation-only metadata that should not be loaded by the gameplay DLL.

## Usage
- Read this file before scanning the folder in detail.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `modifier_presets.toml`: Adapter-facing modifier labels and descriptions keyed by shared `modifier_id`.
- `reputation_unlocks.toml`: Adapter-facing reputation-unlock labels and descriptions keyed by shared `unlock_id`.
- `technology_nodes.toml`: Adapter-facing technology-node labels and descriptions keyed by shared globally unique `tech_node_id` values that match the gameplay tech rows and every other technology-node reference table.
