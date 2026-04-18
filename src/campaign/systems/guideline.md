# src/campaign/systems/

Campaign-level systems that react to gameplay messages and mutate only campaign-owned state.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_flow_system.h`: Campaign flow system interface for high-level progression transitions.
- `campaign_flow_system.cpp`: Campaign flow system implementation.
- `campaign_system_context.h`: Shared read/write access bundle passed into campaign systems.
- `loadout_planner_system.h`: Loadout planner system interface.
- `loadout_planner_system.cpp`: Loadout planner system implementation.
- `regional_support_system.h`: Regional support system interface for campaign support updates.
- `technology_system.h`: Technology system interface for research/unlock progression.
