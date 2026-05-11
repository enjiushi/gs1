# src/campaign/systems/

Campaign-level systems that react to gameplay messages and mutate only campaign-owned state.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_flow_system.h`: Campaign flow system interface for high-level progression transitions and fixed-step-derived campaign clock pacing, now advertising direct runtime message/host subscription lists through the shared system interface.
- `campaign_flow_system.cpp`: Campaign flow system implementation for campaign state transitions, authoritative runtime app-state handoff, tech-tree open/close and faction-tab flow from both the regional map and active site sessions, site-completion reward message emission, and adjacent-site availability promotion without duplicating already-visible prototype site markers in the revealed-site cache.
- `campaign_time_system.h`: Campaign time system interface for fixed-step-derived campaign clock advancement.
- `campaign_time_system.cpp`: Campaign time system implementation that owns campaign clock/day progression.
- `campaign_system_context.h`: Shared read/write access bundle passed into campaign systems.
- `faction_reputation_system.h`: Campaign faction-reputation system interface, including direct runtime message subscription declarations through the shared system interface.
- `faction_reputation_system.cpp`: Campaign faction-reputation implementation for non-decreasing per-faction trust totals plus assistant unlocks.
- `loadout_planner_system.h`: Loadout planner system interface, including direct runtime message subscription declarations through the shared system interface.
- `loadout_planner_system.cpp`: Loadout planner system implementation, including the content-authored baseline deployment package and content-authored per-contributor support quota.
- `regional_support_system.h`: Regional support system interface for campaign support updates.
- `technology_system.h`: Technology system interface for research/unlock progression, including direct runtime message/host subscription declarations through the shared system interface.
- `technology_system.cpp`: Technology system implementation for the shared total-reputation unlock ladder that now advances every `100` reputation across the non-starter plant roster plus ten shared basic food/drink recipes, while leaving the baseline-on-start build/tool utility path to Field Kitchen, Workbench, Storage Crate, Hammer, Medicine, and Shovel intact, alongside the linear `32`-tier-per-faction tech ladder that now auto-unlocks directly from matching faction-reputation requirements, treats faction-granted recipes/items/structures as authoritative unlock sources, and exposes reputation-scaled tech-effect parameter helpers for the revised Village/Bureau/University split.
