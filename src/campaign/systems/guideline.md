# src/campaign/systems/

Campaign-level systems that react to gameplay messages and mutate only campaign-owned state.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_flow_system.h`: Campaign flow system interface for high-level progression transitions.
- `campaign_flow_system.cpp`: Campaign flow system implementation, including regional-map tech-tree open/close flow and site-completion reward message emission.
- `campaign_system_context.h`: Shared read/write access bundle passed into campaign systems.
- `faction_reputation_system.h`: Campaign faction-reputation system interface.
- `faction_reputation_system.cpp`: Campaign faction-reputation implementation for per-faction trust, occupied reputation accounting inputs, and assistant unlocks.
- `loadout_planner_system.h`: Loadout planner system interface.
- `loadout_planner_system.cpp`: Loadout planner system implementation.
- `regional_support_system.h`: Regional support system interface for campaign support updates.
- `technology_system.h`: Technology system interface for research/unlock progression.
- `technology_system.cpp`: Technology system implementation for tiered faction-tech claims, amplification exclusivity, occupied-reputation cost accounting, and faction-tab query helpers.
