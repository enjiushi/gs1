# src/content/

Prototype content storage, indexing, and validation shells used to feed gameplay definitions.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `content_database.h`: Aggregate content database types that hold loaded definitions.
- `content_index.h`: Lookup/index helpers over content definitions.
- `content_loader.h`: Content loading entry points and loader-facing contracts.
- `content_validator.h`: Validation entry points for loaded content data.
- `defs/`: Per-domain content definition structs.
- `prototype_content.h`: Prototype content package declarations, including authored regional-support seeds, site faction ownership, site-completion reputation rewards, and tech-pick thresholds.
- `prototype_content.cpp`: Prototype content construction and seed-data implementation, including authored support packages, nearby-aura seeds, faction-linked site rewards, and prototype tech-pick thresholds.
