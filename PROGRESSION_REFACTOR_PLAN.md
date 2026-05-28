# Progression Refactor Plan

This document tracks the medium-scope progression refactor agreed for this repository.

It has three jobs:
- describe the target architecture
- list the medium refactor checklist with statuses
- record what is intentionally left undone for a later deeper refactor

## Status Legend

- `pending`
- `processing`
- `done`

## Target State

The refactor target is a generic progression pipeline that removes game-specific token-delta and tech-purchase message patterns from the migrated path.

### Core Rules

1. Token-changing systems do not subscribe to generic `delta requested` messages.
2. Token-changing systems subscribe to semantic gameplay facts or generic progression events.
3. Internal `GameMessage` dispatch is inline, so token mutation and follow-up grants happen before control returns to the sender.
4. Sender systems may check read-only state before emitting purchase/progression messages.
5. Invalid purchase or duplicate grant cases are programming bugs and should assert in debug rather than being silently ignored or converted into gameplay rejection flows.
6. Target-owner systems remain narrow and only manage their own target state and side effects inside their ownership domain.
7. Multiple systems may subscribe to the same generic grant message and each system should only apply the side effects it owns.

### Canonical Generic IDs

- `token_kind_id`: stable authored id for a token family
- `target_kind_id`: stable authored id for a target family
- `purchase_entry_id`: stable authored id for one authored purchase path
- `target_id`: real content id inside the target family
- `scope_id`: authored scope instance id used by scoped token kinds such as faction reputation

### Canonical Generic Messages

- `ProgressionEventOccurred`
  - generic progression/event input that a token authority can interpret through authored config
- `PurchaseEntrySelected`
  - generic purchase input carrying `purchase_entry_id`
- `TargetGranted`
  - generic progression output carrying `target_kind_id`, `target_id`, and `grant_kind`

### Grant Kinds

- `Unlocked`
- `Available`
- `Effective`

For the current technology migration, the important states are `Available` and `Effective`.

### Target-System Responsibilities

- `TechnologySystem`
  - manages tech target state only
  - consumes `TargetGranted`
  - does not own token balances, costs, thresholds, or purchase rules

- `ModifierSystem`
  - may subscribe to `TargetGranted`
  - applies modifier-owned side effects when authored target-to-modifier mappings exist

- Other owner systems
  - may subscribe to `TargetGranted`
  - only apply side effects inside their ownership domains

### Token-System Responsibilities

The medium refactor expects generic token-authority behavior, whether implemented as one generic token system or through a small number of generalized progression/token systems.

Token authority behavior:
- own token balances
- own scoped token balance mutation
- consume semantic gameplay facts or `ProgressionEventOccurred`
- resolve threshold unlocks
- resolve purchase spends from `PurchaseEntrySelected`
- emit `TargetGranted`

### Threshold Behavior

- threshold grants fire once
- threshold grants are permanent
- one threshold source may grant multiple targets through authored rows

### Purchase Behavior

- sender checks state before emitting purchase
- purchase system resolves price, token kind, scope, granted target, and grant kind from authored purchase data
- invalid purchase is a programming bug and should assert in debug or return an error status
- no gameplay-facing rejection message is planned for the medium refactor

### Duplicate Grant Behavior

Duplicate or illegal grants should assert in debug.

Examples:
- `Locked -> Available` is legal
- `Available -> Effective` is legal
- `Locked -> Effective` is legal when the authored grant path intends direct effectiveness
- duplicate `Available` is illegal
- duplicate `Effective` is illegal
- `Effective -> Available` is illegal

## Medium Refactor Checklist

### Slice 1: Generic Progression Core

- `done` Add generic ids and config-table model for `token_kind_id`, `target_kind_id`, and `purchase_entry_id`.
- `done` Add scoped token-kind support for current faction-based token use cases.
- `done` Add generic message payloads and enum entries for `ProgressionEventOccurred`, `PurchaseEntrySelected`, and `TargetGranted`.
- `done` Define and wire authored config access for:
  - `token_kind_defs`
  - `target_kind_defs`
  - `token_event_defs`
  - `threshold_unlock_defs`
  - `purchase_defs`
- `done` Decide exact runtime home for generic token-authority logic and register it in the runtime system registry.

### Slice 2: Technology Migration

- `processing` Refactor `TechnologySystem` so it no longer subscribes to token-delta or tech-claim style legacy progression messages in the migrated path.
- `processing` Make `TechnologySystem` consume `TargetGranted` and manage only tech target state.
- `processing` Add assertions for duplicate or illegal grant transitions in `TechnologySystem`.
- `processing` Move current reputation-driven tech availability/effectiveness behavior onto generic threshold and purchase config rows.
- `pending` Remove fully replaced technology-specific legacy message types and handlers once no migrated path still uses them.

### Slice 3: Purchase and Shop/Offer Migration

- `pending` Refactor current purchase flows to emit `PurchaseEntrySelected` instead of target-and-token-specific purchase messages in migrated paths.
- `pending` Migrate shop or unlockable purchase handling to generic purchase-entry resolution.
- `pending` Make shop/offer owner systems consume `TargetGranted` where appropriate.
- `pending` Remove fully replaced purchase-specific legacy message types and handlers once no migrated path still uses them.

### Slice 4: Progression Side Effects

- `pending` Add authored target-to-modifier mapping so `ModifierSystem` can react to relevant `TargetGranted` messages.
- `pending` Update existing owner systems that should consume generic grants directly instead of relying on tech-specific side-effect wiring.
- `pending` Keep side effects in existing owner systems rather than introducing a new relay system unless a later refactor proves it necessary.

### Slice 5: Producer Migration

- `processing` Replace migrated producers of legacy progression/token/purchase messages with:
  - semantic gameplay fact messages where the fact is still valuable on its own
  - `ProgressionEventOccurred` where a generic progression event is the intended input
  - `PurchaseEntrySelected` for authored purchase paths
- `pending` Remove dead legacy producers once the new path is active.

### Slice 6: Validation and Tests

- `processing` Add system/runtime coverage for scenario 1: token threshold grants tech/shop targets directly.
- `pending` Add system/runtime coverage for scenario 2: threshold grants tech availability and purchase grants effectiveness.
- `pending` Add system/runtime coverage for scenario 3: direct purchase grants tech effectiveness.
- `pending` Add coverage for duplicate-grant assertions.
- `pending` Add coverage for one-time threshold behavior.
- `pending` Add coverage for same-target multiple purchase-entry behavior.
- `processing` Update docs and folder guidelines affected by the refactor.

## Left Undone By The Medium Refactor

The items below are intentionally out of scope if we stop at the medium refactor.

### Deeper Genericization Not Required Yet

- full replacement of every rich gameplay fact message in the entire runtime with generic progression-event messages
- deep redesign of non-progression gameplay messages unrelated to token, threshold, or purchase flows
- generalized refund or rollback flows
- generalized reversible unlock flows
- generalized downgrade/deactivation flows for already-effective targets

### Broader Data/Domain Expansion

- additional scope kinds beyond what current faction use cases require
- support for arbitrarily complex cost formulas beyond the current authored purchase-row model
- support for highly dynamic purchase-entry generation at runtime
- support for more advanced multi-step authored progression chains if current target grant states remain sufficient

### Broader Target-Family Migration

- migration of every existing unlockable family in the project if some families are still outside the current progression pipeline
- migration of unrelated domain systems that do not currently need generic progression participation

### Optional Later Cleanup

- collapsing any remaining compatibility helpers that only exist to bridge tests or aggregate campaign-state access
- broader naming cleanup across all progression-related content and runtime docs after the new pipeline stabilizes
- any future consolidation of token-authority logic if the medium refactor lands with more than one generalized progression/token system

## Working Notes

### Agreed Design Points

- scoped token kinds are required in v1 because current faction progression needs them
- purchase messages should be generic and keyed by `purchase_entry_id`
- purchase rules own token kind and cost in config rather than in the purchase message payload
- duplicate grants should assert rather than silently no-op in production logic
- multiple owner systems may subscribe to `TargetGranted`
- target systems should stay narrow and not absorb token or threshold logic

### Open Implementation Decisions

- exact payload layout for `ProgressionEventOccurred`
- exact authored format and loader path for the new generic progression tables
- whether the generic token-authority behavior lands as one new system or as a narrow refactor of existing progression-owning systems
- exact migration order for legacy message removal once the first slices are implemented

### Chosen Medium-Scope Migration Order

- Step 1: add generic progression authored tables, ids, lookups, validation, and messages
- Step 2: migrate campaign-owned reputation progression onto generic progression events and generic grants
- Step 3: migrate `TechnologySystem` to generic grant consumption while preserving current public unlock query helpers
- Step 4: migrate site cash purchase handling onto generic purchase entries inside the existing site economy ownership boundary
- Step 5: migrate the minimum producer set for the new path and remove the fully replaced legacy messages

### Chosen Runtime Ownership Shape

- campaign progression authority owns reputation-scoped token balances and threshold-based grants
- site economy authority owns site-cash token balances and site-cash purchase spends
- both authorities use the same generic authored config and the same generic message contracts

### Current Medium-Pass Status Notes

- The current code pass adds a new `CampaignProgressionSystem`, generic progression defs/messages/lookups, and a dedicated split `CampaignProgression` state set.
- Current content loading derives generic progression rows from existing reputation-unlock and technology-node content instead of introducing a second external authoring file yet.
- `TaskBoardSystem` reputation rewards now emit `ProgressionEventOccurred` on the migrated path.
- `TechnologySystem` now consumes `TargetGranted` for tech-node target state, but compatibility total-reputation mirroring is still kept for old helper paths and non-tech target kinds have not been migrated into explicit grant storage yet.
- Runtime-side aggregate test access now uses a narrow compatibility cache bridged through split-state helpers, and the stale legacy projection-stream runtime regression has been replaced by a `Gs1GameStateView`/tile-query contract test aligned with `RUNTIME_MESSAGE_INVENTORY.md`.
- Focused runtime targets currently compile again: `gs1_gameplay_core`, `gs1_runtime_message_chain_test`, `gs1_runtime_projection_test`, `gs1_runtime_performance_test`, and `gs1_timed_modifier_projection_test`.
- `gs1_system_test_suite` still has one unrelated compile blocker outside this refactor slice: `tests/system/source/environment_worker_device_system_tests.cpp` currently redefines `sheltered_tile`.
- Site-cash purchase migration, deeper technology effectiveness semantics, broader scenario coverage, and legacy-message removal are still unfinished and intentionally remain in the next pass.
