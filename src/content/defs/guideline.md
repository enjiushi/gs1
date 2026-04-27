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
- `excavation_defs.h`: Excavation depth/tier authoring types plus runtime accessors for depth-specific energy/find tuning and tier-scoped loot pools.
- `event_defs.h`: Authored event definitions used by event/state systems.
- `gameplay_tuning_defs.h`: Shared gameplay-tuning definition records and lookup for authored numeric factors consumed by worker-condition, ecology, modifier, device-support, and camp-durability systems, including worker background energy-recovery tuning, the simplified weather-vs-resistance growth-pressure scaling, and the authored full-range real-minutes control for the linear density-change curve, with the fallback baseline set to `5` real minutes.
- `item_defs.h`: Item definition types plus runtime accessors over the loaded item table, including authored internal cash-point values used for task/reward valuation, the starter checkerboard item, the full prototype 10-plant seed roster, the harvested-good item roster tied back to source plants, and excavation-only stone merchandise ids.
- `modifier_defs.h`: Modifier preset definition records and runtime accessors applied by modifier/weather flows.
- `plant_defs.h`: Plant/ecology definition types plus runtime accessors over the loaded plant table, including the full China-desert 10-plant prototype roster, explicit plant focus tags, the authored six-meter roster-pool model (`salt`, `heat`, `wind`, `dust`, `fertility`, `output`), derived salt-to-salinity support helpers for support-focused plants, authored footprint-multiple shelter reach plus nearby-protection ratios, per-plant harvest output metadata, and the starter straw checkerboard definition with its authored `2x2` footprint plus tuned steady-wither behavior used by directional shelter resolution.
- `recipe_defs.h`: General recipe definition records shared by authored content.
- `reward_defs.h`: Reward definition records and runtime accessors for grants and payouts, including the shared prototype task-reward pool, simple modifier rewards, immediate unlockable reveals, and delivery-crate-routed item bundle candidates.
- `site_content_defs.h`: Site template/content definitions for active site runs.
- `structure_defs.h`: Structure/buildable definition types plus runtime accessors over the loaded structure table, including authored multi-channel support values for resolved tile contribution state.
- `task_defs.h`: Task board/authored task records plus runtime accessors, including single-value target/required-count/threshold fields that use `0` or `None` as procedural sentinels, authored expected in-game hours and risk multipliers for task valuation, expanded progress kinds such as living-plant non-withering duration checks plus item submission, optional action/item/plant/recipe/structure selector hints, and site-keyed onboarding task seed overrides with optional fixed reward candidates.
- `technology_defs.h`: Prototype progression definition records and runtime accessors, including `8` faction base-tech tiers, `2` exclusive enhancement slots per base tech, per-node faction reputation requirements plus internal cash-point valuation that converts to player-facing cash, authored reputation-scaled effect parameters, optional item/plant/structure/recipe grant payloads, per-plant total-reputation unlock thresholds, starter plants available from campaign start, and placeholder faction tech nodes.
