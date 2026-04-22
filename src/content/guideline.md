# src/content/

Prototype content storage, indexing, validation, and TOML-backed content loading used to feed gameplay definitions.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `content_database.h`: Aggregate content database types that hold loaded definitions.
- `content_index.h`: Lookup/index helpers over content definitions.
- `content_loader.h`: Content loading entry points and loader-facing contracts.
- `content_loader.cpp`: TOML parsing and startup loading for the externalized prototype content slices, including the content-backed campaign/site graph, campaign setup, site phone-listing seeds, site action tuning with per-action worker nourishment cost, modifier presets, technology progression authoring, and multi-channel structure support values used by resolved tile computation.
- `content_lookup.cpp`: Runtime-backed definition accessors used by gameplay systems after content is loaded.
- `content_validator.h`: Validation entry points for loaded content data.
- `content_validator.cpp`: Validation checks for loaded authored content, campaign/site graph/setup references, seeded phone listings, site action coverage, modifier preset coverage, technology progression references, and cross-file references.
- `defs/`: Per-domain content definition structs.
- `tables/`: External TOML authoring files for the prototype content slices, including the site/campaign graph content, seeded phone listings, and site-action tuning values that now cover nourishment cost as well as duration, energy, and hydration.
- `prototype_content.h`: Prototype content package declarations, including authored site objective modes/config, campaign setup/loadout data, regional-support seeds, site faction ownership, site-completion rewards, and seeded phone listings.
- `prototype_content.cpp`: Table-backed prototype campaign/site accessors used by campaign/site factories.
