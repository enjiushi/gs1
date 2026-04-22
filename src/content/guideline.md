# src/content/

Prototype content storage, indexing, validation, and config-table loading used to feed gameplay definitions.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `content_database.h`: Aggregate content database types that hold loaded definitions.
- `content_index.h`: Lookup/index helpers over content definitions.
- `content_loader.h`: Content loading entry points and loader-facing contracts.
- `content_loader.cpp`: Config-table parsing and startup loading for the current externalized prototype content slices.
- `content_lookup.cpp`: Runtime-backed definition accessors used by gameplay systems after content is loaded.
- `content_validator.h`: Validation entry points for loaded content data.
- `content_validator.cpp`: Validation checks for loaded content tables and cross-table references.
- `defs/`: Per-domain content definition structs.
- `tables/`: External TSV authoring tables for the config-backed prototype content slices.
- `prototype_content.h`: Prototype content package declarations, including authored site objective modes/config, regional-support seeds, site faction ownership, site-completion rewards, and tech-pick thresholds.
- `prototype_content.cpp`: Prototype content construction and seed-data implementation, including authored site objective configs, support packages, nearby-aura seeds, faction-linked site rewards, and prototype campaign-support item seeds that align with the 10-plant roster.
