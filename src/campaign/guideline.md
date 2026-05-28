# src/campaign/

Campaign-owned state containers plus the campaign system layer that advances long-term progress.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `campaign_state.h`: Aggregate campaign state still used by factories and tests, while authoritative runtime-owned campaign storage is split into separate state sets for core, regional-map meta plus per-list slices, faction-progress, campaign-progression token/grant memory, technology, loadout-planner meta plus per-list slices, and flattened site metadata slices. State-set payloads in the split path are kept pointer-free.
- `campaign_progression_state.h`: Pointer-free campaign progression/token state used by the generic progression authority for scoped token balances, total-reputation mirroring, and one-shot threshold firing memory.
- `faction_progress_state.h`: Regional/faction progress state tracked at campaign scope, including non-decreasing faction reputation plus assistant unlock tracking.
- `loadout_planner_state.h`: State for loadout planning, selected-site support assembly, site-prep decisions, and the content-authored support quota per completed contributor, with split-friendly meta/container entry types alongside the compatibility aggregate.
- `systems/`: Campaign system declarations and implementations, including the generic campaign progression authority for reputation-scoped token balances and threshold grants, compatibility faction-reputation handling, and technology progression handlers for the linear `32`-tier-per-faction tech ladder and the trimmed shared total-reputation baseline unlocks, with campaign flow/time/progression/loadout/technology mutating split runtime campaign state sets through dedicated access helpers instead of treating the aggregate campaign compatibility object as the runtime source of truth.
- `technology_state.h`: Campaign technology progression state, currently keeping compatibility total-reputation mirroring plus explicit authored tech-node grant-state storage so technology ownership can migrate toward generic `TargetGranted` flow.
