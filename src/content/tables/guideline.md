# src/content/tables/

External prototype content tables loaded by the gameplay DLL at startup.

## Usage
- Read this file before scanning the folder in detail.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_setup.tsv`: Config-backed prototype campaign setup table for starting cash, support quota per contributor, and baseline deployment items.
- `sites.tsv`: Config-backed prototype campaign/site authoring table for site graph, objective config, support exports, nearby aura seeds, completion rewards, camp anchors, and initial reveal/availability flags.
- `phone_listings.tsv`: Config-backed seeded phone-listing table for per-site buy, unlockable, and contractor offers.
- `items.tsv`: Config-backed item authoring table for runtime item definitions and item-domain metadata, including the full prototype seed-item roster for the China-desert ten-plant set.
- `plants.tsv`: Config-backed plant authoring table for ecology, shelter, and planting behavior across the full prototype China-desert 10-plant roster.
- `structures.tsv`: Config-backed structure authoring table for buildables, storage, and crafting stations.
- `craft_recipes.tsv`: Config-backed craft recipe authoring table for station/output/ingredient mappings.
- `task_templates.tsv`: Config-backed authored task template table for task board progress metadata, item/recipe targets, and faction reputation rewards.
- `reward_candidates.tsv`: Config-backed reward candidate table for money, deliveries, unlockable reveals, run modifiers, and reputation rewards.
- `site_actions.tsv`: Config-backed site action tuning table for action duration, worker energy/hydration/nourishment costs, placement occupancy, and reservation/approach/movement flags.
- `modifier_presets.tsv`: Config-backed modifier preset table for nearby-aura and run-modifier channel totals consumed by the site modifier system.
- `technology_tiers.tsv`: Config-backed faction technology tier table for per-faction reputation requirements and display names.
- `total_reputation_tiers.tsv`: Config-backed total-reputation tier table for global plant unlock thresholds and labels.
- `reputation_unlocks.tsv`: Config-backed total-reputation unlock table for post-starter plant unlocks and future non-plant unlock types.
- `technology_nodes.tsv`: Config-backed technology node table for faction tech entries, cash costs, entry kinds, and placeholder labels/descriptions.
- `initial_unlocked_plants.tsv`: Config-backed starter-plant table for the plants available from campaign start before total-reputation unlocks.
