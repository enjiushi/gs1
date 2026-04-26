# Agent Guide

This file is a quick orientation guide for agents working in this repository.

## Folder Guideline Workflow

- `AGENTS.md` is the root entry point for repository navigation.
- Before exploring a folder in detail, check whether that folder contains `guideline.md`. If it does, read it first for a short description of the folder plus the files and child folders inside it.
- If you open a child folder, repeat the same check there and continue recursively as needed.
- When a file or folder is added, removed, renamed, or meaningfully changed, update the `guideline.md` in its parent folder in the same change.
- When a root-level file or folder is added, removed, renamed, or meaningfully changed, update `AGENTS.md` because it acts as the root guideline.
- The maintained guideline set covers the source-controlled project folders. Generated or ephemeral folders such as `build/`, `out/`, `.git/`, and `.agent-progress/` do not need `guideline.md` maintenance unless a task explicitly targets them.

## Repository Map

- `.agent-progress/`: Session-progress and merge-coordination scratch area used by worktree/session rules.
- `build/`: Local CMake build tree and IDE-generated build artifacts.
- `include/`: Public headers exposed to the gameplay DLL host boundary. Read `include/guideline.md` before drilling deeper.
- `out/`: Generated binaries, logs, exported headers, and local browser/debug profiles.
- `scripts/`: PowerShell helpers for building and running tests. Read `scripts/guideline.md` before drilling deeper.
- `src/`: Gameplay/runtime implementation code grouped by ownership domain. Read `src/guideline.md` before drilling deeper.
- `tests/`: Runtime, smoke, and system test code plus scripted/system-test assets. Read `tests/guideline.md` before drilling deeper.
- `third_party/`: Vendored external dependencies. Read `third_party/guideline.md` before drilling deeper.
- `CMakeLists.txt`: Builds the gameplay DLL plus the smoke, visual smoke, smoke-host threading regression, runtime-test, and system-test executables, including the campaign faction-reputation and technology sources plus the owner-specific plant/device weather contribution systems used by runtime/UI coverage.
- `launch_visual_game.ps1`: Convenience wrapper for the visual smoke host that forwards PowerShell `-Verbose` into host-side verbose logging.
- `include/gs1/export.h`: Public DLL export/import macros.
- `include/gs1/game_api.h`: C ABI entry points exposed by the gameplay DLL.
- `include/gs1/status.h`: Public status/result codes.
- `include/gs1/types.h`: Public API data types, message/event payload structs, runtime-facing enums, claimed-task/phone-panel transport fields, site-weather event timeline transport fields, tech-tree claim/refund action identifiers, and harvest-action plus harvested-output transport payloads.
- `src/app/`: Runtime bootstrap, API glue, scene coordination, and campaign/site factory entry points.
- `src/campaign/`: Campaign state and campaign-level systems such as flow, dedicated campaign time progression, loadout planning, regional support, and the faction-branch tech progression model with remembered reputation spend plus top-down refunds.
- `src/messages/`: Internal game message IDs, payloads, handlers, and dispatcher.
- `src/content/`: Prototype content database, loader/validator shells, and content definition types, including authored wind-shelter plant stats, per-plant harvest-output metadata, item and unlockable internal cash-point valuation data, one-by-one plant reputation unlock thresholds, and faction-branch tech progression tables.
- `src/events/`: Translated engine feedback event types. Internal gameplay cross-system messages should use `GameMessage`, not runtime gameplay events.
- `src/runtime/`: Core runtime loop, fixed-step runner, host-event dispatch plus transient phase-control handling, message queues, random service, and engine message emission.
- `src/site/`: Active site-run state and site systems for dedicated site time progression, actions, weather, owner-specific plant/device local-weather contribution passes, ecology, directional local wind shelter, inventory, economy, task board, camp durability, harvested-output flow, and completion/failure flow.
- `src/support/`: Shared support types such as typed IDs.
- `src/ui/`: View-model and presenter state for HUD, phone, notifications, inspection, regional map, and UI presentation.
- `tests/smoke/`: Smoke host, runtime DLL loader, script runner, live-state JSON, live HTTP server, and visual smoke UI, including the right-click harvest action path.
- `tests/smoke/scripts/`: Scripted smoke-test scenarios.
- `scripts/`: PowerShell build and smoke-test helpers.
- `GDD.md`: Game design document, including the directional wind-shadow and plant-footprint shelter rules, the current resistance-driven plant shelter model where plants project outward heat/wind/dust shelter from their tolerance/resistance values without self-shielding their own footprint and now snap wind shelter to 8 readable directions with full orthogonal lee lanes plus half-strength immediate diagonals, and growth pressure compares local weather against effective resistance on the same `0.01` scale, the current profile-based harsh-weather/runtime weather-event contract with start/peak/peak-duration/end timing and post-event baseline recovery direction, the worker meter-relationship chapter with the unified `prevState -> resolve frame deltas -> currentState` computation model plus derived `Energy Cap`, derived `Work Efficiency`, and hydration/nourishment-scaled background energy recovery, the direct local-weather-to-action-cost multiplier rules layered onto authored hydration/nourishment/energy action costs plus max-channel weather-scaled morale action costs and weather-driven morale background drift, the resolved tile contribution plus terrain-factor computation notes in the meter-relationship chapter, the site-one onboarding task-pool direction, the single near-camp delivery-crate direction that starts with site-one's `1` water plus `8` straw-checkerboard loadout already packed inside, the immediate-delivery-crate insertion plus overflow-queue direction for later purchases and item rewards, phone-based claimed-task history direction, the staged four-mode site-objective direction, the neutral base-tech plus faction-enhancement tech-tree direction, the internal-cash-point-to-visible-cash pricing rule for techs and direct-purchase unlockables, the straw-checkerboard special-plant `2x2` tile-occupancy rule, the site-result-panel `OK` return contract, and the 30-real-minute in-game day timing contract.
- `GAME_STRUCTURE.md`: High-level gameplay/runtime structure notes.
- `GAME_SYSTEM_DESIGN_V1.md`: System design reference, including directional local-weather resolution responsibilities, the snapped 8-direction wind-shelter rule with full orthogonal lee lanes plus half-strength immediate diagonals, resolved per-tile support-channel ownership, factor/weight/bias terrain meter computation notes, the worker-condition responsibility notes for delta-bucket meter resolution plus derived `energyCap` and `workEfficiency` snapshots and hydration/nourishment-scaled background energy recovery, direct local-weather-to-action-cost scaling on authored hydration/nourishment/energy action costs, start/peak/peak-duration/end weather-event timelines with simple return-to-baseline weather recovery, staged site-objective implementation notes, site-start loadout seeding into the single near-camp delivery crate with site-one's `1` water plus `8` straw-checkerboard baseline, the prototype China-desert 10-plant roster with four starter plants plus three total-reputation unlock tiers, persistent campaign cash used for neutral base-tech and faction-enhancement purchases, the internal-cash-point-to-visible-cash pricing rule for techs and direct-purchase unlockables, claimed-task history direction for task rewards, site-one onboarding task-pool notes, the `SiteResult` panel `OK`-return contract, and the canonical fixed-step time conversion rules that keep one in-game day at 30 real-time minutes.
- `SYSTEM_DESIGN_STATUS.md`: Current system status and implementation tracking, including site-objective readiness notes, the current weather/post-event recovery model summary, the total-reputation plant track, and the neutral base-tech plus faction-enhancement purchase direction.
- `SYSTEM_TEST_COVERAGE.md`: Per-system automated coverage expectations, including placeholder-versus-implemented test scope for campaign, site, and runtime behavior plus total-reputation plant-tier coverage, snapped 8-direction wind-shelter coverage for local weather, neutral base-tech and faction-enhancement purchase coverage with internal cash-point-derived prices, immediate delivery-crate insertion with overflow-queue regressions, direct local-weather-to-action-cost action-execution coverage, and worker background energy-recovery expectations.
- `CONTENT_AUTHORING_CONTRACT.md`: Content authoring contract and data expectations, including the current simplified harsh-weather timeline fields.
- `AUDIO_ASSET_PLAN.md`: Audio sourcing and integration plan covering recommended ambience, SFX, and prototype music libraries, budget paths, and host-side playback parameter guidance aligned with the current density-based desert soundscape direction.
- `MISSING_DEFINITION_DESCRIPTIONS.md`: Notes for definitions that still need descriptions.
- `TASK_BOARD_OPEN_QUESTIONS.md`: Follow-up product-contract questions for task-board reward presentation, claim lifetime, reward delivery semantics, and prototype board scope.

## Code Style

- All structs should try to be trivial. Prefer simple data-only layouts and avoid adding behavior or lifecycle management to structs unless there is a clear need.
- Prefer POD-like structs for data containers: keep them trivial and standard-layout when practical, with no virtual functions, custom constructors/destructors, or owning containers unless the design clearly needs them. This keeps layout predictable and gives the compiler the best chance to optimize copies, binary payloads, and cache-friendly data paths.

## Regression Test Rule

- When a bug is found in or traced to a specific gameplay system, add or update a system test that reproduces that bug and locks the expected behavior before considering the fix complete.
- Prefer the narrowest system-level test that covers the regression. If the bug depends on ECS state, seed the ECS data directly in the test setup rather than routing around the system boundary.
- If the current implementation intentionally cannot express the full design behavior yet, keep the regression test focused on the bug we fixed and document any still-missing broader behavior in the system test coverage docs.

## Session Handoff Rule

- Unless the user explicitly says to create a new session, run the task in the main worktree.
- The main worktree does not use the session-occupancy rules in this section. These rules apply only to non-main worktrees used for explicit sessions.
- After a session chooses a non-main worktree, create the fixed Markdown file `.agent-progress/session.md` in that worktree. Do not use task names or session names in the tracker file name.
- Treat `.agent-progress/session.md` as both the live task tracker for that session and the occupancy marker showing that the worktree is in use.
- Use that file to list every intended work item before doing it, and keep appending new task items as the session takes on more work. Do not erase earlier task entries from the file.
- Every listed item must include a status and only the statuses `pending`, `processing`, or `done` may be used.
- Update the file as work progresses so another agent can immediately see what is planned, what is in flight, and what is finished.
- Keep `.agent-progress/` out of commits.
- Unless the user explicitly says to end the session, the current non-main session continues to occupy that worktree.
- Remove `.agent-progress/session.md` only after the session's changes have been successfully merged back to `main`. Do not remove it earlier, even if the current task list is complete.
- When a task is done, the agent must always tell the user which worktree it is currently using.

## Worktree Alignment Rule

- If a task starts from a git worktree, align that worktree branch with the current local `main` branch before making task-specific changes.
- If the user explicitly starts a new session in a specific non-main worktree, do not realign that targeted worktree with `main` before starting the session work.
- Resolve or report any alignment conflicts before proceeding with implementation in that worktree.

## Merge Coordination Rule

- Worktrees are no longer manually assigned.
- Create or choose a non-main worktree only when the user explicitly asks to start a new session or otherwise clearly requires work outside the main worktree.
- If the user explicitly asks to start a new session in a specific non-main worktree, use that exact worktree for the session.
- An explicitly targeted non-main worktree overrides the occupied/free selection constraints for choosing a worktree, so the session should skip the occupancy scan and work there directly.
- If the work is staying on the main worktree, the non-main session occupancy rules do not apply.
- When a task needs a non-main git worktree, the agent must discover the available candidates from `git worktree list` and choose a specific free worktree on its own.
- Determine whether a candidate worktree is free by checking whether `.agent-progress/session.md` exists in that worktree. If that file exists, the worktree is occupied and the session must choose another candidate.
- Inspect candidate worktrees one by one. If the current candidate already has `.agent-progress/session.md`, skip it and continue to the next candidate worktree.
- If no candidate worktree is free, wait 10 seconds and repeat the one-by-one worktree scan until a free worktree becomes available.
- Use `.agent-progress/merge-log.md` as the shared merge coordination file for merges from a source worktree into a target worktree, including merges into `main`, unless the source is `main` and the target is another worktree.
- Merges from `main` into another worktree do not use this rule and do not need to read or write `.agent-progress/merge-log.md`.
- If `.agent-progress/merge-log.md` does not exist when an in-scope merge is about to start, create it in `.agent-progress/` before doing anything else related to that merge.
- Every in-scope merge entry in `.agent-progress/merge-log.md` must include a unique 64-bit unsigned integer merge ID, the source worktree name, the target worktree name, and a state of either `processing` or `done`.
- Generate a merge ID only when appending a new in-scope merge entry. The first appended merge entry uses ID `0`, and each later merge entry uses the latest recorded merge ID plus `1`.
- Before appending a new in-scope merge entry, inspect `.agent-progress/merge-log.md`. If any merge entry is currently marked `processing`, wait 10 seconds and check again. Repeat until no merge entry is still `processing`.
- If a request says an in-scope merge may start only after merge ID `X`, wait until `.agent-progress/merge-log.md` contains a `done` merge entry whose ID is greater than `X`. Recheck every 10 seconds. Once that condition is satisfied, also perform the no-`processing` check before appending the new merge entry.
- When an in-scope merge can start, append the new merge entry with state `processing` before running the merge itself.
- When an in-scope merge finishes, update that merge entry from `processing` to `done`.

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
