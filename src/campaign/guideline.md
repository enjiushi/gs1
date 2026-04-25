# src/campaign/

Campaign-owned state containers plus the campaign system layer that advances long-term progress.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_state.h`: Aggregate campaign state owned outside individual campaign subsystems.
- `faction_progress_state.h`: Regional/faction progress state tracked at campaign scope, including non-decreasing faction reputation plus assistant unlock tracking.
- `loadout_planner_state.h`: State for loadout planning, selected-site support assembly, site-prep decisions, and the content-authored support quota per completed contributor.
- `systems/`: Campaign system declarations and implementations, including faction-reputation and technology progression handlers.
- `technology_state.h`: Campaign technology progression, purchased-node records with remembered reputation-spend sources, campaign-wide total-reputation tracking for plant unlocks, and persistent campaign cash.
