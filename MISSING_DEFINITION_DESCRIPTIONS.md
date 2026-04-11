# Missing Definition Descriptions

Source references:

- [GDD.md](GDD.md)
- [SYSTEM_DESIGN_STATUS.md](SYSTEM_DESIGN_STATUS.md)
- [GAME_STRUCTURE.md](GAME_STRUCTURE.md)
- [CONTENT_AUTHORING_CONTRACT.md](CONTENT_AUTHORING_CONTRACT.md)

Purpose: show which formal definitions are still missing after the authoring-contract pass, and distinguish prototype-safe deferrals from real later architecture work.

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

Resolved now:

- The missing authoring-side contract now exists in [CONTENT_AUTHORING_CONTRACT.md](CONTENT_AUTHORING_CONTRACT.md).
- Task templates, tutorial-task overrides, task tiers, reward bands, reward candidates, event templates, forecast profiles, and authored prototype support packages now have explicit authored field contracts.
- Validation rules for these content rows are now defined as tool/build-time requirements instead of implicit runtime assumptions.

What was locked by that contract:

- exact authored field lists for task, event, reward, and support-package content
- generation-order rules for reward draft assembly
- separation between authored rows and runtime instances
- explicit validation rules for content tools and data import

Expected output:

- Use [CONTENT_AUTHORING_CONTRACT.md](CONTENT_AUTHORING_CONTRACT.md) directly during system design.

## Recommended Next Document Order

If the goal is to support the next system-design stage cleanly, turn the missing definitions into formal contracts in this order:

1. gameplay module/system-design docs that assign concrete game-state ownership directly inside the existing `GAME_STRUCTURE.md` framework
2. `SAVE_BOUNDARIES_CONTRACT.md` later, only when save/load actually enters implementation scope

That order follows the current prototype priority: the authoring contract is now locked, ownership should be assigned next inside the normal system-design pass, and save/load remains an explicit later TODO.
