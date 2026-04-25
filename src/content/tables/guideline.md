# src/content/tables/

External prototype content TOML files loaded by the gameplay DLL at startup.

## Usage
- Read this file before scanning the folder in detail.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_setup.toml`: Prototype campaign setup content for starting cash, support quota per contributor, and baseline deployment items.
- `sites.toml`: Prototype campaign/site authoring content for regional-map tile placement, objective config, support exports, nearby aura seeds, completion rewards, camp anchors, site default-weather baselines, authored starter-plant patches such as Site 1's harvestable near-camp starter growth, and initial reveal/availability flags.
- `phone_listings.toml`: Seeded phone-listing content for per-site buy, unlockable, and contractor offers, including internal cash-point valuation for direct-purchase unlockables that converts to player-facing cash.
- `items.toml`: Item authoring content for runtime item definitions and item-domain metadata, including consumable meter deltas, internal cash-point valuation used by task/reward scoring, the full prototype seed-item roster, and harvested-good items for the China-desert ten-plant set.
- `plants.toml`: Plant authoring content for ecology, planting behavior, simplified resistance-driven outward shelter, terrain-rehab support, and per-plant harvest thresholds/output tuning across the full prototype China-desert 10-plant roster.
- `structures.toml`: Structure authoring content for buildables, storage, crafting stations, and authored resolved-tile support channels such as wind, heat, dust, fertility, salinity, and irrigation output.
- `craft_recipes.toml`: Craft recipe authoring content for station/output/ingredient mappings plus recipe-authored craft duration and worker energy/hydration/nourishment costs.
- `task_templates.toml`: Authored task template content using single target/required-count/threshold values, where `0` or `None` means the runtime must resolve that field procedurally, plus authored expected in-game hours and risk multipliers for valuation and optional selector hints for runtime-picked item/plant/recipe/structure/action values.
- `site_onboarding_task_seeds.toml`: Site-keyed onboarding task seed content that instantiates exact task instances for a specific site, with optional fixed reward-candidate assignment.
- `reward_candidates.toml`: Reward candidate content for money, deliveries, unlockable reveals, run modifiers, and reputation rewards.
- `site_actions.toml`: Site action tuning content for shared action duration and worker energy/hydration/nourishment/morale costs, authored local-weather-to-meter cost coefficients, placement occupancy, and reservation/approach/movement flags, with craft duration and worker meter costs authored per recipe and the shared harvest action profile still loaded here.
- `modifier_presets.toml`: Modifier preset content for nearby-aura and run-modifier channel totals consumed by the site modifier system.
- `gameplay_tuning.toml`: Shared numeric gameplay-tuning table for worker-condition, ecology, modifier, device-support, and camp-durability factors, including worker background energy-recovery tuning and the shared `0.01` weather-vs-resistance growth-pressure comparison that should remain data-authored instead of hardcoded.
- `technology_tiers.toml`: Faction tech-tree tier content for tier ordering and display names.
- `reputation_unlocks.toml`: Total-reputation unlock content for one-by-one post-starter plant unlocks and future non-plant unlock types.
- `technology_nodes.toml`: Technology node content for one faction-owned tech per tier branch, authored reputation plus internal cash-point valuation that converts to player-facing cash, entry kinds, and placeholder labels/descriptions.
- `initial_unlocked_plants.toml`: Starter-plant content for the two plants available from campaign start before later reputation unlocks.
