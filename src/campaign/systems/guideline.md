# src/campaign/systems/

Campaign-level systems that react to gameplay messages and mutate only campaign-owned state.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_flow_system.h`: Campaign flow system interface for high-level progression transitions, explicit ownership of app/campaign flow plus `SiteRunMeta`, and direct `RuntimeInvocation`-based host/game subscription spans on the shared runtime-system interface.
- `campaign_flow_system.cpp`: Campaign flow system implementation for campaign state transitions, authoritative runtime app-state handoff, owner-resolved campaign clock deltas, narrow campaign/site lifecycle installation through `RuntimeInvocation` helpers instead of direct cross-owner split-state seeding, site-run creation through the site-run factory's split-campaign overload, direct host gameplay action handling for start/select/deploy/return intents without first bouncing through a centralized router or synthetic gameplay request messages, and adjacent-site availability promotion without duplicating already-visible prototype site markers in the revealed-site cache.
- `campaign_progression_system.h`: Generic campaign progression authority interface that owns faction-progress and campaign-progression split state for scoped token balances plus threshold grants.
- `campaign_progression_system.cpp`: Generic campaign progression authority implementation for inline progression-event handling, reputation token mutation, one-shot threshold firing, assistant unlock mirroring, and generic `TargetGranted` emission on the migrated campaign path.
- `campaign_time_system.h`: Campaign time system interface for fixed-step-derived campaign clock advancement through the shared `RuntimeInvocation` system interface while directly owning the dedicated split `CampaignTime` state set.
- `campaign_time_system.cpp`: Campaign time system implementation that advances the dedicated `CampaignTime` split state directly, including derived remaining-day updates, without routing through campaign-flow-owned message indirection.
- `faction_reputation_system.h`: Campaign faction-reputation system interface, including direct runtime message subscription spans through the shared `RuntimeInvocation` system interface.
- `faction_reputation_system.cpp`: Compatibility campaign faction-reputation implementation retained while the generic campaign progression authority owns the migrated faction-reputation mutation path.
- `loadout_planner_system.h`: Loadout planner system interface, including direct runtime message subscription spans through the shared `RuntimeInvocation` system interface.
- `loadout_planner_system.cpp`: Loadout planner system implementation, including the content-authored baseline deployment package and content-authored per-contributor support quota, now driven from tagged runtime campaign-state access.
- `regional_support_system.h`: Regional support system interface for campaign support updates.
- `technology_system.h`: Technology system interface for research/unlock progression, including direct runtime message/host subscription spans through the shared `RuntimeInvocation` system interface while preserving external read-only unlock query helpers.
- `technology_system.cpp`: Technology system implementation for content gating and the migrated tech-target grant path, still preserving compatibility total-reputation unlock helpers for baseline plants/recipes while beginning the shift toward explicit tech-node grant storage and `TargetGranted` consumption instead of owning token mutation itself.
