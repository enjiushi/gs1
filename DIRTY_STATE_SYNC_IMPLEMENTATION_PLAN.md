# Dirty State Sync Implementation Plan

## Goal
Replace frame-wide adapter refreshes with a systematic dirty-state pipeline where runtime-owned state sets mark their own changes, adapter controllers subscribe to the narrow state sets they care about, and ECS-owned changes are surfaced through owning systems instead of being scanned from the adapter side.

## Core Direction
- Keep gameplay/runtime state authoritative.
- Keep adapter controllers ignorant of gameplay internals.
- Use tiny state sets with explicit ownership boundaries.
- Use dirty flags or dirty buckets, not version-heavy polling.
- Let adapter controllers reconcile keyed local caches only when their subscribed state sets are dirty.

## Implementation Steps
1. Add a runtime-owned dirty journal for state-set changes.
2. Make state mutation helpers mark the owning state set dirty when writable state changes.
3. Expose a compact dirty snapshot to the exported state view so the adapter can ask which state sets changed.
4. Update the Godot adapter service to route dirty state sets to subscribed controllers instead of broadcasting a frame-wide refresh.
5. Keep cheap, tiny UI values on direct per-frame pull if they are already trivial to read and apply.
6. Use keyed delta reconciliation for lists, tiles, scene objects, inventories, and other larger structures.
7. Add regression coverage for state-set dirty propagation and adapter invalidation behavior.

## Design Rules
- A state set may be dirty even when only one field inside it changed.
- A controller should subscribe to the smallest state set that covers its UI or scene responsibility.
- ECS systems should mark their owning projection state set dirty when they mutate owned ECS data.
- The adapter should never scan the whole game state to discover dirtiness.
- If a surface is tiny, per-frame pull is acceptable and simpler than building extra dirty machinery.
- If a surface is list-like or object-like, dirty-triggered keyed reconciliation is required.

## First Target Surfaces
- Site HUD and site/session shell state.
- Inventory panel state.
- Phone panel state.
- Task list and modifier list state.
- Regional map site list state.
- Site world tile/object projection state.

## Non-Goals For The First Slice
- Do not redesign the entire public ABI at once.
- Do not add per-entity revision chains everywhere.
- Do not remove the remaining small adapter-local polling where it is already cheap.
- Do not force all controllers into a single giant dirty bucket.

## Verification
- Add a focused runtime test that proves dirty state is recorded for mutated state sets.
- Add a focused adapter-side regression that proves subscribed controllers only refresh when their state set is dirty.
- Build and run the affected runtime and Godot adapter targets once the first slice lands.
