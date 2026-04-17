# Agent Guide

This file is a quick orientation guide for agents working in this repository.

## Repository Map

- `CMakeLists.txt`: Builds the gameplay DLL and smoke host executables.
- `include/gs1/export.h`: Public DLL export/import macros.
- `include/gs1/game_api.h`: C ABI entry points exposed by the gameplay DLL.
- `include/gs1/status.h`: Public status/result codes.
- `include/gs1/types.h`: Public API data types, message/event payload structs, and runtime-facing enums.
- `src/app/`: Runtime bootstrap, API glue, scene coordination, and campaign/site factory entry points.
- `src/campaign/`: Campaign state and campaign-level systems such as flow, loadout planning, regional support, and technology.
- `src/messages/`: Internal game message IDs, payloads, handlers, and dispatcher.
- `src/content/`: Prototype content database, loader/validator shells, and content definition types.
- `src/events/`: Translated engine feedback event types. Internal gameplay cross-system messages should use `GameMessage`, not runtime gameplay events.
- `src/runtime/`: Core runtime loop, fixed-step runner, host-event dispatch plus transient phase-control handling, message queues, random service, and engine message emission.
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

- All structs should try to be trivial. Prefer simple data-only layouts and avoid adding behavior or lifecycle management to structs unless there is a clear need.
- Prefer POD-like structs for data containers: keep them trivial and standard-layout when practical, with no virtual functions, custom constructors/destructors, or owning containers unless the design clearly needs them. This keeps layout predictable and gives the compiler the best chance to optimize copies, binary payloads, and cache-friendly data paths.

## Regression Test Rule

- When a bug is found in or traced to a specific gameplay system, add or update a system test that reproduces that bug and locks the expected behavior before considering the fix complete.
- Prefer the narrowest system-level test that covers the regression. If the bug depends on ECS state, seed the ECS data directly in the test setup rather than routing around the system boundary.
- If the current implementation intentionally cannot express the full design behavior yet, keep the regression test focused on the bug we fixed and document any still-missing broader behavior in the system test coverage docs.

## Session Handoff Rule

- At the start of any task or implementation session, create a temporary Markdown file at `.agent-progress/<session-name>.md`.
- Use that file as a live task tracker for the session and list every intended work item before doing it.
- Every listed item must include a status and only the statuses `pending`, `processing`, or `done` may be used.
- Update the file as work progresses so another agent can immediately see what is planned, what is in flight, and what is finished.
- Keep `.agent-progress/` out of commits and remove the temporary session file once every listed item is `done` and no further session work remains.

## System Ownership Rule

- Treat every gameplay system as a self-contained owner of a specific state slice.
- A system may directly mutate only the state it owns.
- Do not add direct system-to-system calls for gameplay coordination.
- Do not directly mutate another system's owned state even if both states are reachable from the same runtime object or context struct.
- ECS systems should iterate the archetypes or entity sets that actually carry the components they care about.
- Do not scan a broad dense archetype such as all tiles just to discover a sparse component such as devices or other optional occupants.
- If sparse gameplay data needs tile linkage, give that data its own ECS entity with a tile-coordinate component instead of hanging the sparse payload off every tile entity.
- Prefer owner-scoped ECS access helpers that make read-only versus read/write component access explicit.
- When mutating ECS data, update only the owned components that changed; avoid whole-aggregate read/modify/write paths when only one field or component is owned by that system.
- If one system needs another ownership domain to change, express that request through internal `GameMessage` flow and let subscribed owning systems resolve it.
- Treat `GameMessage` definitions as publish/observe contracts, not private RPCs between two named systems.
- Define a message payload around gameplay meaning published by the producer; do not shape it around one specific consumer's implementation details.
- The producer must not assume which systems listen to a message. Zero, one, or many systems may consume it.
- If several systems respond to the same message, each listener may update only its own owned state and should emit follow-up messages for further cross-owner effects.
- There are no internal gameplay events; internal gameplay cross-system communication uses `GameMessage`.
- The message queue must dispatch a message only to systems that explicitly subscribe to that message type.
- Translated feedback-event queues must use the same enum-indexed subscriber-list dispatch pattern as messages.
- If a system is not subscribed to a message or translated feedback-event type, it must not process that message.
- Systems should coordinate only through subscribed message processing, translated feedback-event processing, and owned/public state observation, never through direct peer calls or direct peer-state writes.
- Be conservative about changing established message layouts. Prefer changing listener behavior over editing shared message definitions unless the gameplay meaning itself needs to change.
- When touching existing code, preserve or strengthen this boundary rather than bypassing it for convenience.
- This rule exists so we can write focused system tests and let multiple agent sessions modify different systems asynchronously with less hidden coupling and fewer message-definition merge conflicts.
