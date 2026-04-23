# src/content/tables/

External prototype content TOML files loaded by the gameplay DLL at startup.

## Usage
- Read this file before scanning the folder in detail.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_setup.toml`: Prototype campaign setup content for starting cash, support quota per contributor, and baseline deployment items.
- `sites.toml`: Prototype campaign/site authoring content for site graph, objective config, support exports, nearby aura seeds, completion rewards, camp anchors, site default-weather baselines, authored starter-plant patches such as Site 1's harvestable near-camp starter growth, and initial reveal/availability flags.
- `phone_listings.toml`: Seeded phone-listing content for per-site buy, unlockable, and contractor offers.
- `items.toml`: Item authoring content for runtime item definitions and item-domain metadata, including the full prototype seed-item roster plus harvested-good items for the China-desert ten-plant set.
- `plants.toml`: Plant authoring content for ecology, shelter, planting behavior, tuned two-specialty resistance/support roles, and per-plant harvest thresholds/output tuning across the full prototype China-desert 10-plant roster.
- `structures.toml`: Structure authoring content for buildables, storage, crafting stations, and authored resolved-tile support channels such as wind, heat, dust, fertility, salinity, and irrigation output.
- `craft_recipes.toml`: Craft recipe authoring content for station/output/ingredient mappings.
- `task_templates.toml`: Authored task generator content for the balanced three-faction site-one task pool, including procedural target/threshold ranges, optional selector hints for runtime-picked item/plant/recipe/structure/action values, living-plant stability duration tasks, and the current zero-reward per-task contract.
- `reward_candidates.toml`: Reward candidate content for money, deliveries, unlockable reveals, run modifiers, and reputation rewards.
- `site_actions.toml`: Site action tuning content for action duration, worker energy/hydration/nourishment costs, placement occupancy, and reservation/approach/movement flags, including the shared harvest action profile.
- `modifier_presets.toml`: Modifier preset content for nearby-aura and run-modifier channel totals consumed by the site modifier system.
- `technology_tiers.toml`: Neutral base-technology tier content for per-tier total-reputation requirements and display names.
- `faction_technology_tiers.toml`: Faction enhancement tier content for per-faction reputation requirements and display names.
- `total_reputation_tiers.toml`: Total-reputation tier content for global plant unlock thresholds and labels.
- `reputation_unlocks.toml`: Total-reputation unlock content for post-starter plant unlocks and future non-plant unlock types.
- `technology_nodes.toml`: Technology node content for neutral base techs plus faction-owned enhancements, cash costs, entry kinds, and placeholder labels/descriptions.
- `initial_unlocked_plants.toml`: Starter-plant content for the plants available from campaign start before total-reputation unlocks.
