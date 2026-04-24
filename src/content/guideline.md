# src/content/

Prototype content storage, indexing, validation, and TOML-backed content loading used to feed gameplay definitions.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `content_database.h`: Aggregate content database types that hold loaded definitions, including the shared gameplay-tuning slice consumed by multiple runtime systems plus site-keyed onboarding task seed records.
- `content_index.h`: Lookup/index helpers over content definitions.
- `content_loader.h`: Content loading entry points and loader-facing contracts.
- `content_loader.cpp`: TOML parsing and startup loading for the externalized prototype content slices, including the content-backed campaign/site graph, campaign setup, site phone-listing seeds, authored site default-weather and starter-plant bootstrap data, shared gameplay tuning tables for worker condition, ecology, modifier, device-support, and camp-durability systems, single-value task-template metadata with `0`/`None` procedural sentinels, site-keyed onboarding task seed overrides with optional fixed reward candidates, site action tuning with per-action worker nourishment cost, per-plant harvest-output metadata, modifier presets, neutral base-tech tiers plus faction-enhancement progression authoring, and multi-channel structure support values used by resolved tile computation.
- `content_lookup.cpp`: Runtime-backed definition accessors used by gameplay systems after content is loaded, including the shared gameplay-tuning lookup.
- `content_validator.h`: Validation entry points for loaded content data.
- `content_validator.cpp`: Validation checks for loaded authored content, campaign/site graph/setup references, site default-weather and starter-plant bootstrap values, shared gameplay-tuning ranges, seeded phone listings, single-value task-template references, site-keyed onboarding task seed references, site action coverage, harvest-item coherence, modifier preset coverage, neutral-tech and faction-enhancement progression references, and cross-file references.
- `content_loader.cpp`: TOML parsing and startup loading for the externalized prototype content slices, including the content-backed campaign/site graph, campaign setup, site phone-listing seeds, authored site default-weather and starter-plant bootstrap data, shared gameplay tuning tables for worker condition, ecology, modifier, device-support, and camp-durability systems, single-value task-template metadata with `0`/`None` procedural sentinels, site-keyed onboarding task seed overrides with optional fixed reward candidates, site action tuning with per-action worker nourishment cost plus authored weather-to-action-cost coefficients and recipe-authored craft duration and worker meter costs, per-plant harvest-output metadata, modifier presets, neutral base-tech tiers plus faction-enhancement progression authoring, and multi-channel structure support values used by resolved tile computation.
- `content_lookup.cpp`: Runtime-backed definition accessors used by gameplay systems after content is loaded, including the shared gameplay-tuning lookup.
- `content_validator.h`: Validation entry points for loaded content data.
- `content_validator.cpp`: Validation checks for loaded authored content, campaign/site graph/setup references, site default-weather and starter-plant bootstrap values, shared gameplay-tuning ranges, seeded phone listings, single-value task-template references, site-keyed onboarding task seed references, site action coverage plus non-negative weather-to-action-cost coefficients, harvest-item coherence, modifier preset coverage, neutral-tech and faction-enhancement progression references, and cross-file references.
- `defs/`: Per-domain content definition structs, including the shared gameplay-tuning definition used by multiple gameplay systems.
- `tables/`: External TOML authoring files for the prototype content slices, including the site/campaign graph content, seeded phone listings, harvest-output item rows, site-action tuning values, and the shared gameplay-tuning table for cross-system numeric factors.
- `prototype_content.h`: Prototype content package declarations, including authored site objective modes/config, site default-weather and starter-plant bootstrap data, campaign setup/loadout data, regional-support seeds, site faction ownership, site-completion rewards, and seeded phone listings.
- `prototype_content.cpp`: Table-backed prototype campaign/site accessors used by campaign/site factories.
