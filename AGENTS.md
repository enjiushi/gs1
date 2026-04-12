# Agent Guide

This file is a quick orientation guide for agents working in this repository.

## Repository Map

- `CMakeLists.txt`: Builds the gameplay DLL and smoke host executables.
- `include/gs1/export.h`: Public DLL export/import macros.
- `include/gs1/game_api.h`: C ABI entry points exposed by the gameplay DLL.
- `include/gs1/status.h`: Public status/result codes.
- `include/gs1/types.h`: Public API data types, command/event payload structs, and runtime-facing enums.
- `src/app/`: Runtime bootstrap, API glue, scene coordination, and campaign/site factory entry points.
- `src/campaign/`: Campaign state and campaign-level systems such as flow, loadout planning, regional support, and technology.
- `src/commands/`: Internal game command IDs, payloads, handlers, and dispatcher.
- `src/content/`: Prototype content database, loader/validator shells, and content definition types.
- `src/events/`: Runtime and engine feedback event types.
- `src/runtime/`: Core runtime loop, fixed-step runner, input snapshots, command queues, random service, and engine command emission.
- `src/site/`: Active site-run state and site systems for actions, weather, ecology, inventory, economy, task board, camp durability, and completion/failure flow.
- `src/support/`: Shared support types such as typed IDs.
- `src/ui/`: View-model and presenter state for HUD, phone, notifications, inspection, regional map, and UI presentation.
- `tests/smoke/`: Smoke host, runtime DLL loader, script runner, live-state JSON, live HTTP server, and visual smoke UI.
- `tests/smoke/scripts/`: Scripted smoke-test scenarios.
- `scripts/`: PowerShell build and smoke-test helpers.
- `GDD.md`: Game design document.
- `GAME_STRUCTURE.md`: High-level gameplay/runtime structure notes.
- `GAME_SYSTEM_DESIGN_V1.md`: System design reference.
- `SYSTEM_DESIGN_STATUS.md`: Current system status and implementation tracking.
- `CONTENT_AUTHORING_CONTRACT.md`: Content authoring contract and data expectations.
- `MISSING_DEFINITION_DESCRIPTIONS.md`: Notes for definitions that still need descriptions.

## Code Style

- Prefer POD-like structs for data containers: keep them trivial and standard-layout when practical, with no virtual functions, custom constructors/destructors, or owning containers unless the design clearly needs them. This keeps layout predictable and gives the compiler the best chance to optimize copies, binary payloads, and cache-friendly data paths.
