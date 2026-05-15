# src/

Gameplay/runtime implementation grouped by ownership domain and integration boundary.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `app/`: Bootstrap, API glue, scene coordination, and factory entry points.
- `campaign/`: Campaign-owned state and campaign-level systems.
- `content/`: Prototype content database, indexing, and definition types, including per-plant primary-plus-secondary harvest-output authoring, the shared total-reputation unlock ladder, Village shovel/food recipe tables, and the linear `32`-tier faction-tech content gating data.
- `events/`: Legacy placeholder folder retained only for guidance; internal gameplay coordination uses `GameMessage` flow under `messages/`.
- `gs1_pch.h`: Stable internal precompiled-header umbrella for common STL and Flecs includes shared across heavy C++ build targets.
- `host/`: Engine-agnostic runtime-DLL host bridge code shared by smoke hosts and future engine adapters, including the shared DLL loader, runtime-session wrapper around split phase execution plus runtime-message draining, adapter-config helpers, adapter-metadata loading, and wrappers for the read-only gameplay-state view and tile-query ABI.
- `messages/`: Internal gameplay message contracts and dispatch helpers.
- `runtime/`: Core runtime loop, queues, clocks, execution services, split gameplay state-set storage, `StateManager` ownership/query wiring, and the exported read-only state-view/tile-query helpers used by hosts and adapters.
- `site/`: Active site-run state, ECS world helpers, and site-owned systems, including split-backed site-state access helpers, harvest action execution plus worker-pack harvest output routing, campaign-aware recipe/item unlock filtering, Village-specific shovel, excavation, buff, and custom-food runtime hooks, and now dedicated runtime-system site presentation follow-up for action/cue/protection UI reactions instead of app-layer post-message coordination, with system entrypoints constructing their own local runtime contexts directly instead of routing through file-local context wrapper helpers when no extra abstraction is needed.
- `support/`: Shared lightweight support types such as IDs.
- `testing/`: Runtime-side helpers that back the system-test framework.
- `ui/`: Legacy UI folder retained only for its guideline after the orphaned gameplay-owned view-model and presenter headers were removed from the refactor path.
