# src/content/defs/

Data-only definition headers for the major authored content domains.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_content_defs.h`: Campaign-level authored content records.
- `faction_defs.h`: Prototype faction identity and assistant unlock definition records.
- `craft_recipe_defs.h`: Crafting recipe authoring records for craft-system use.
- `event_defs.h`: Authored event definitions used by event/state systems.
- `item_defs.h`: Item definition records and item-domain metadata, including the authored starter checkerboard item.
- `modifier_defs.h`: Modifier definition records applied by modifier/weather flows.
- `plant_defs.h`: Plant/ecology definition records, including authored wind-protection range/power values and the starter straw checkerboard definition with its authored `2x2` footprint plus tuned steady-wither behavior used by directional shelter resolution.
- `recipe_defs.h`: General recipe definition records shared by authored content.
- `reward_defs.h`: Reward definition records for grants and payouts, including the shared prototype task-reward pool, simple modifier rewards, immediate unlockable reveals, and delivery-based item bundle candidates.
- `site_content_defs.h`: Site template/content definitions for active site runs.
- `structure_defs.h`: Structure/buildable definition records.
- `task_defs.h`: Task board/authored task definition records, including the site-one onboarding task pool, progress-kind metadata, and lookup helpers.
- `technology_defs.h`: Technology tree definition records, including prototype faction tabs, four temporary modifier tiers per faction, base-tech entries, and mutually exclusive amplification choices.
