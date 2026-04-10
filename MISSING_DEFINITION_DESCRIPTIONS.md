# Missing Definition Descriptions

Source references:

- [GDD.md](d:/testgame/gs1_upstream/GDD.md)
- [SYSTEM_DESIGN_STATUS.md](d:/testgame/gs1_upstream/SYSTEM_DESIGN_STATUS.md)
- [GAME_STRUCTURE.md](d:/testgame/gs1_upstream/GAME_STRUCTURE.md)

Purpose: show, for each still-missing definition, what is already defined in the current GDD and what is still missing before the next system-design stage.

## Reading Guide

- `Already defined now` means the current GDD already gives enough direction that the system shape is visible.
- `**Still missing**` marks the exact parts that are not locked yet.
- `Deferred for now` means the topic is real, but intentionally outside the current prototype implementation scope.
- `Expected output` means the concrete document or contract that the next pass should produce.
- `GDD.md` should stay conceptual; exact field lists, structs, and schemas belong in the later system-design documents.
- `GAME_STRUCTURE.md` already defines the global architecture pipeline, ECS-world authority, command/event communication, world-level engine commands, and engine-adapter boundary.
- The earlier separate `Persistence Ownership By System` gap is no longer tracked here. `GAME_STRUCTURE.md` already fixes the generic ownership model, and the concrete game-module ownership can be assigned directly during normal system-design work.
- Resolved prototype design questions should be removed from this document rather than kept as fake missing items.

## 1. Save-Data Boundaries

Already defined now:

- Many systems already distinguish campaign-persistent versus site-session state at a concept level.
- The prototype currently does not implement save/load.
- `SaveLoadModule` should be treated as a later TODO module, not as active prototype scope right now.
- The current retry design already says failed local session state is discarded and the next attempt starts from normal site-entry conditions.
- The ECS world is already defined as the gameplay source of truth, so save design should focus on world-owned gameplay state rather than engine-side projections.

Deferred for now:

- **Final save-game boundaries**
- **What is serialized**
- **What is reconstructed**
- **What the minimal prototype save must contain**
- **How save versioning will work later**

Why this matters:

- Save design still matters for later long-session architecture, but it should not block the current prototype because save/load is intentionally out of scope for now.

Expected output:

- Later, one save-boundary map when save/load work actually enters scope.

## 2. Explicit Authoring Contract For Task Templates, Event Templates, And Reward-Draft Templates

Already defined now:

- Tasks, events, and reward drafts are clearly meant to be data-driven.
- Task tiers, reward bands, and tier-to-band reward direction are already defined conceptually in the GDD.
- [GAME_STRUCTURE.md](d:/testgame/gs1_upstream/GAME_STRUCTURE.md) already defines the higher-level schema split between global shared schemas, reusable feature schemas, and game-specific schemas.
- Normal contract-board tasks are generated at runtime from authored templates, while onboarding tasks are authored overrides.
- The GDD already defines extreme-event archetypes like `HeatWave`, `Sandstorm`, and `CompoundFront`; those are generated site events, not the same thing as `Per-Site Modifier`s.

Clarified contract direction now:

### Task template fields

- `taskTemplateId`
- `taskType`: `CollectItem` or `ReachProgressTarget`
- `taskTier`
- `publisherFactionId`
- `isOnboardingAuthored`
- `targetAmount`
- `CollectItem` task fields: `itemId`
- `ReachProgressTarget` task fields: `progressTargetKind` (`Plant` or `Device`) and `progressTargetId`
- Runtime task instances should then track one shared `currentProgressAmount` against the authored `targetAmount`, regardless of task type.

### Event template fields

- `eventTemplateId`
- `eventArchetypeId`
- authored pressure profile for `eventHeatPressure`, `eventWindPressure`, and `eventDustPressure`
- authored phase-duration ranges for `Warning`, `Build`, `Peak`, `Decay`, and `Aftermath`
- authored forecast profile when one event archetype needs different warning behavior

Current event rule:

- event templates are authored data
- the concrete event type that appears in a run is generated at runtime from eligible event templates
- extreme events are separate from `Per-Site Modifier`s; modifiers are reward/support effects, while events are site-wide hazard archetypes feeding the weather model

### Reward-draft template fields

- reward candidates should identify reward family, linked content id, and amount
- reward families currently needed: `Money`, `Item`, `Unlockable`, `Modifier`
- `Item` rewards need `itemId` and `amount`
- `Unlockable` rewards need `unlockableId` and `amount`
- `Modifier` rewards need `modifierId` and `amount`
- money reward amount is authored by task tier
- non-money reward amounts are derived from that tier's money-value baseline

### Which fields are authored directly

- task templates
- onboarding task definitions and tutorial-task overrides
- event templates and their phase/pressure profiles
- money reward amount per task tier
- reward candidate ids and reward-family eligibility

### Which fields are generated at runtime

- normal contract-board task instances
- the concrete reward family and reward options shown inside one draft
- the concrete event type selected for the current site/run
- runtime task progress values such as `currentProgressAmount`
- resolved non-money reward amounts derived from the selected task tier's money baseline

### What validation rules content tools should enforce

- every task template must choose exactly one supported `taskType`
- `CollectItem` tasks must have a valid `itemId` and a positive `targetAmount`
- `ReachProgressTarget` tasks must have a valid `progressTargetKind`, a valid target id, and a positive `targetAmount`
- one task template must not mix `CollectItem` fields with `ReachProgressTarget` fields
- onboarding tasks must be marked authored-only so the normal generator cannot surface them as repeatable runtime tasks
- every reward candidate id must exist and must match its reward family
- every task tier used by tasks must have an authored base money reward amount
- derived non-money reward amounts must resolve to positive valid amounts
- every event template must use valid non-negative phase-duration ranges and a valid event-archetype pressure profile

Meaning of that validation-rules line:

- It means the content editor, spreadsheet importer, or data-build checker should reject bad definitions before they reach runtime. For example, a `CollectItem` task with no `itemId`, or a modifier reward that points to a missing modifier row, should fail in tools instead of silently breaking in game.

Why this matters:

- Engineering cannot build clean data-driven pipelines without final authoring schemas.

Expected output:

- One content-authoring contract.

## Recommended Next Document Order

If the goal is to support the next system-design stage cleanly, turn the missing definitions into formal contracts in this order:

1. `CONTENT_AUTHORING_CONTRACT.md`
2. gameplay module/system-design docs that assign concrete game-state ownership directly inside the existing `GAME_STRUCTURE.md` framework
3. `SAVE_BOUNDARIES_CONTRACT.md` later, only when save/load actually enters implementation scope

That order follows the current prototype priority: lock content authoring first, keep ownership inside the normal system-design pass, and leave save/load as an explicit later TODO.
