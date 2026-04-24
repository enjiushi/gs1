# src/content/defs/

Definition types and runtime accessors for the major authored content domains.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_content_defs.h`: Campaign-level authored content records.
- `faction_defs.h`: Prototype faction identity and assistant unlock definition records.
- `craft_recipe_defs.h`: Crafting recipe definition types plus runtime accessors over the loaded recipe table, including recipe-authored craft duration and worker energy/hydration/nourishment costs.
- `event_defs.h`: Authored event definitions used by event/state systems.
- `gameplay_tuning_defs.h`: Shared gameplay-tuning definition records and lookup for authored numeric factors consumed by worker-condition, ecology, modifier, device-support, and camp-durability systems.
- `item_defs.h`: Item definition types plus runtime accessors over the loaded item table, including the authored starter checkerboard item, the full prototype 10-plant seed roster, and the harvested-good item roster tied back to source plants.
- `modifier_defs.h`: Modifier preset definition records and runtime accessors applied by modifier/weather flows.
- `plant_defs.h`: Plant/ecology definition types plus runtime accessors over the loaded plant table, including the full China-desert 10-plant prototype roster, authored wind-protection range/power values, per-plant harvest output metadata, and the starter straw checkerboard definition with its authored `2x2` footprint plus tuned steady-wither behavior used by directional shelter resolution.
- `recipe_defs.h`: General recipe definition records shared by authored content.
- `reward_defs.h`: Reward definition records and runtime accessors for grants and payouts, including the shared prototype task-reward pool, simple modifier rewards, immediate unlockable reveals, and delivery-crate-routed item bundle candidates.
- `site_content_defs.h`: Site template/content definitions for active site runs.
- `structure_defs.h`: Structure/buildable definition types plus runtime accessors over the loaded structure table, including authored multi-channel support values for resolved tile contribution state.
- `task_defs.h`: Task board/authored task generator records plus runtime accessors, including the site-one multi-faction task pool, expanded progress kinds such as living-plant non-withering duration checks, procedural amount/threshold ranges, and optional action/item/plant/structure selector hints resolved per generated task instance.
- `technology_defs.h`: Prototype progression definition records and runtime accessors, including total-reputation-gated base-tech tiers, faction-reputation-gated enhancement tiers, total-reputation-gated plant unlock tiers, starter plants available from campaign start, and placeholder base-tech plus enhancement nodes with cash costs.
