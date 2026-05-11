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
- `events/`: Engine feedback event translation types.
- `gs1_pch.h`: Stable internal precompiled-header umbrella for common STL and Flecs includes shared across heavy C++ build targets.
- `host/`: Engine-agnostic runtime-DLL host bridge code shared by smoke hosts and future engine adapters, now including the shared projection cache that carries app/menu, regional-map, dense row-major site-tile, inventory/task/phone/craft/placement/protection, and recent-cue state alongside the drained engine-message stream used by the Godot adapter and smoke hosts.
- `messages/`: Internal gameplay message contracts and dispatch helpers.
- `runtime/`: Core runtime loop, queues, clocks, and execution services, including timed-modifier remaining-game-hour projection for smoke/viewer consumers plus the phase-1 site-scene-ready handshake that keeps newly started site runs in `SITE_LOADING` until the host/adapter confirms scene resources are ready.
- `site/`: Active site-run state, ECS world helpers, and site-owned systems, including harvest action execution plus worker-pack harvest output routing, campaign-aware recipe/item unlock filtering, and Village-specific shovel, excavation, buff, and custom-food runtime hooks.
- `support/`: Shared lightweight support types such as IDs.
- `testing/`: Runtime-side helpers that back the system-test framework.
- `ui/`: View-model and presenter state projected for the frontend/runtime host.
