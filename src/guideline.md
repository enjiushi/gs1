# src/

Gameplay/runtime implementation grouped by ownership domain and integration boundary.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `app/`: Bootstrap, API glue, scene coordination, and factory entry points.
- `campaign/`: Campaign-owned state and campaign-level systems.
- `content/`: Prototype content database, indexing, and definition types, including per-plant harvest-output authoring.
- `events/`: Engine feedback event translation types.
- `gs1_pch.h`: Stable internal precompiled-header umbrella for common STL and Flecs includes shared across heavy C++ build targets.
- `messages/`: Internal gameplay message contracts and dispatch helpers.
- `runtime/`: Core runtime loop, queues, clocks, and execution services.
- `site/`: Active site-run state, ECS world helpers, and site-owned systems, including harvest action execution plus worker-pack harvest output routing.
- `support/`: Shared lightweight support types such as IDs.
- `testing/`: Runtime-side helpers that back the system-test framework.
- `ui/`: View-model and presenter state projected for the frontend/runtime host.
