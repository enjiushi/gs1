# src/campaign/

Campaign-owned state containers plus the campaign system layer that advances long-term progress.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_state.h`: Aggregate campaign state owned outside individual campaign subsystems.
- `faction_progress_state.h`: Regional/faction support and progress state tracked at campaign scope.
- `loadout_planner_state.h`: State for loadout planning and site-prep decisions.
- `systems/`: Campaign system declarations and implementations.
- `technology_state.h`: Campaign technology progression and unlock state.
