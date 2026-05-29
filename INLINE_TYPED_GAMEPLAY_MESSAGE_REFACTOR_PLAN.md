# Inline Typed Gameplay Message Refactor Plan

## Goal

Refactor internal gameplay-DLL message dispatch from the current runtime `GameMessage` enum-plus-payload transport and mixed inline-plus-queued behavior into compile-time typed immediate inline dispatch.

This refactor applies only to internal gameplay-DLL coordination for now.

The host/runtime transport boundary stays intact for this pass:

- `Gs1HostMessage` remains the host-to-runtime ABI input transport.
- `Gs1RuntimeMessage` remains the runtime-to-host ABI output transport.
- Internal gameplay coordination moves to typed POD message structs with immediate inline dispatch only.

## Target Result

After this refactor:

- Internal gameplay messages are ordinary POD message structs instead of `GameMessageType` plus payload packing.
- Internal gameplay dispatch is always immediate and inline.
- Internal gameplay dispatch no longer uses the legacy runtime `GameMessageQueue`.
- Systems are declared in one compile-time ordered `system_pack`.
- Systems are stored as a typed `std::tuple<Systems...>`.
- Each system declares the gameplay message types it subscribes to via a compile-time trait.
- Subscriber lookup is derived at compile time from the ordered system pack and system traits.
- Host ABI messages are decoded at the runtime boundary and translated immediately into typed internal gameplay messages.
- Runtime-to-host transport stays ABI-stable, while the runtime also exposes a compile-time manifest of which runtime message types this game may emit.
- Re-entrant inline dispatch stays guarded, with extra debug visibility for nested message-call chains.
- The runtime no longer keeps aggregate `CampaignState` / `SiteRunState` compatibility shadow copies or frame/message-boundary sync bridges such as `flush_compatibility_state_to_split_state()` and `refresh_compatibility_state_from_split_state()`.
- Runtime production code uses split authoritative state sets directly instead of routing through legacy aggregate gameplay-state mirrors.

## Non-Goals

- Do not change the host ABI transport types in `include/gs1/types.h`.
- Do not prune the public `Gs1RuntimeMessage` ABI header in this pass.
- Do not introduce direct system-to-system calls as a replacement for gameplay messages.
- Do not keep a deferred internal gameplay-message queue model.
- Do not preserve transport-peeking patterns that treat queued messages as hidden gameplay state.
- Do not preserve aggregate gameplay-state compatibility mirrors inside runtime production paths once direct split-state replacements are available.

## Design Summary

### 1. Internal gameplay messages become typed POD structs

Internal gameplay messages should be plain trivial data structs.

These typed internal gameplay messages must stay out of the host/runtime ABI headers.
They should live in internal gameplay source headers only, organized by gameplay-domain ownership so systems remain reusable without coupling them to public transport definitions.

Preferred direction:

- shared internal gameplay message contracts stay under `src/messages/`
- domain-specific message definitions may live next to the relevant gameplay-domain code when that keeps ownership clearer
- no internal typed gameplay message definitions go into `include/gs1/`

Example direction:

```cpp
struct SiteRunStartedMessage final
{
    std::uint32_t site_id;
    std::uint32_t site_run_id;
    std::uint32_t site_archetype_id;
    std::uint32_t attempt_index;
    std::uint64_t attempt_seed;
};
```

These message structs are identified by C++ type, not by an internal gameplay enum tag.

### 2. Internal gameplay dispatch is always immediate inline

All internal gameplay message delivery should be immediate and inline.

Example direction:

```cpp
runtime.dispatch(SiteRunStartedMessage {...});
```

or:

```cpp
invocation.emit(SiteRunStartedMessage {...});
```

There is no internal gameplay-message enqueue path in the target design.

### 3. Systems are declared in one compile-time ordered pack

The runtime should define one canonical ordered list of gameplay systems.

Example direction:

```cpp
using GameSystems = system_pack<
    CampaignFlowSystem,
    LoadoutPlannerSystem,
    CampaignProgressionSystem,
    TechnologySystem,
    ...
    SiteCompletionSystem>;
```

This order is authoritative for:

- construction order
- gameplay-message subscriber order
- fixed-step registration order inputs
- ownership registration
- profiling registration

Subscriber order must remain explicit and stable so behavior does not drift from today's effective registration order.

### 4. Systems are stored in `std::tuple<Systems...>`

The runtime should own systems primarily as typed storage:

```cpp
std::tuple<Systems...> systems_;
```

This enables:

- compile-time typed lookup
- compile-time construction
- type-safe access without runtime casts
- reuse of the same ordered system pack for multiple runtime concerns

### 5. Systems declare traits explicitly

Each system should declare explicit compile-time traits.

Example direction:

```cpp
struct SiteTimeSystem
{
    using subscribed_messages = type_list<>;
    using emitted_runtime_messages = type_list<PresentationDirtyRuntimeMessage>;
    static constexpr std::optional<std::uint32_t> fixed_step_order = 1U;
    static constexpr auto profile_id = GS1_RUNTIME_PROFILE_SYSTEM_SITE_TIME;
};
```

At minimum, the refactor should support:

- `subscribed_messages`
- `fixed_step_order`
- `emitted_runtime_messages`
- existing ownership declarations or equivalent ownership traits

### 6. Subscriber lookup is derived at compile time

The runtime should not maintain a runtime-built internal gameplay subscriber table keyed by gameplay enum.

Instead, given:

- an ordered `system_pack`
- each system's `subscribed_messages`

the runtime derives `subscribers_of_t<Message>` at compile time by filtering systems whose trait contains that message type.

Example direction:

```cpp
template <typename Message, typename System>
inline constexpr bool subscribes_to_v =
    type_list_contains_v<typename System::subscribed_messages, Message>;
```

Then dispatch iterates subscribers in system-pack order.

### 7. Systems handle typed messages directly

Systems should stop receiving generic internal gameplay envelopes for gameplay coordination.

Preferred direction:

```cpp
Gs1Status handle(RuntimeInvocation&, const SiteRunStartedMessage&);
Gs1Status handle(RuntimeInvocation&, const SiteRefreshTickMessage&);
```

Avoid large internal gameplay `switch (message.type)` logic after migration.

Shared template helpers and `if constexpr` are allowed where useful, but the main dispatch surface should be explicit typed handlers.

### 8. Host ABI messages translate immediately at the boundary

Host-to-runtime transport remains ABI-based, but once a host message crosses into gameplay runtime code, it should be decoded and translated immediately into typed internal gameplay dispatch.

Example direction:

```cpp
Gs1Status handle_host_message(const Gs1HostMessage& message)
{
    switch (message.type)
    {
    case GS1_HOST_EVENT_GAMEPLAY_ACTION:
        return dispatch(StartNewCampaignMessage {...});
    ...
    }
}
```

The important rule is that the internal gameplay side should not convert host inputs into queued internal gameplay envelopes.

### 9. Runtime-to-host transport stays ABI-stable for now

`Gs1RuntimeMessage` remains the transport to the host in this refactor.

However, the gameplay runtime should also expose a compile-time manifest of which runtime message types this game may emit, derived from the active system pack.

That manifest is for:

- validation
- documentation
- tests
- future refactors

It is not a public ABI-pruning step in this pass.

### 10. Re-entrant inline dispatch remains guarded

Because internal gameplay dispatch becomes always-inline, the runtime must keep protections around nested dispatch:

- re-entrant mutation ownership guard stays active
- nested message-call chains should be observable in debug builds
- cycle-prone message flows should be detectable quickly during development

The runtime should retain and strengthen the existing nested-dispatch safety direction rather than relaxing it.

Native callstacks are useful but not sufficient on their own, because they show C++ frames rather than the semantic gameplay message chain.
The debug target here is a lightweight semantic message stack such as:

```cpp
OpenMainMenuMessage
-> SiteRunStartedMessage
-> PlacementReservationRequestedMessage
-> PlacementReservationAcceptedMessage
```

This should stay debug-only and minimal.

### 11. State must be fully mutated before emit

With immediate inline dispatch, subscribers observe current authoritative state right away.

That means producer systems must follow this rule:

- finish the owned-state mutation
- then emit the gameplay message

Do not emit while the producer's owned state is only partially updated.

### 12. Queue-peeking behavior should be removed, not preserved

The current code still has cases where systems inspect pending internal gameplay queue contents to decide what to do later in the same frame.

That is not part of the target design.

If that behavior encodes real gameplay state, it should be rewritten as real owned state or lifecycle state, not preserved as transport inspection.

### 13. Legacy aggregate compatibility state should be removed

The runtime currently still carries compatibility shadow copies of aggregate gameplay state and syncs them to and from split authoritative state.

That is not part of the target architecture.

The target direction is:

- split state sets are the only authoritative runtime gameplay state
- runtime production code does not keep aggregate `CampaignState` or `SiteRunState` mirrors
- runtime execution does not depend on compatibility sync passes before or after phase/message handling
- no aggregate compatibility helpers should remain in production or test-only form after the refactor is complete

The refactor should remove:

- `compatibility_campaign_state_`
- `compatibility_site_run_state_`
- `flush_compatibility_state_to_split_state()`
- `refresh_compatibility_state_from_split_state()`

and migrate any remaining production callers to direct split-state usage.

### 14. No direct system-to-system calls

Even after typed dispatch arrives, gameplay coordination should still flow through gameplay messages, not through direct system calls.

This preserves:

- owner boundaries
- testability
- multi-subscriber coordination
- low coupling between systems

## Migration Direction

The migration should introduce the compile-time typed infrastructure first, then port systems incrementally while keeping the host/runtime ABI stable.

The migration should keep intermediate states buildable.

However, the refactor target is not to preserve legacy internal gameplay behavior through long-lived bridge layers.
The intended direction is:

- move systems onto the latest typed inline structure directly
- keep any unavoidable temporary bridge code narrow and short-lived
- delete migration bridges as soon as the affected systems are converted
- do not institutionalize compatibility paths for old internal gameplay-message behavior

## Progress Tracker

Legend:

- `[x]` not started
- `[>]` in progress
- `[done]` complete

## Current Migration Notes

- [done] Typed inline dispatch scaffolding exists in runtime for compile-time subscriber derivation from the canonical ordered `GameSystems` pack.
- [done] `SiteRefreshTickMessage` is migrated as the first fixed-step-driven typed internal gameplay-message family.
- [done] The placement-reservation request/accept/reject/release family is migrated end-to-end for `PlacementValidationSystem` and `ActionExecutionSystem`, with request/release emitters now using immediate typed inline dispatch.
- [done] `SiteAttemptEndedMessage` queue-peeking duplicate guards were removed from site runtime flow so those systems now rely on authoritative run state and immediate inline emit instead of pending transport inspection.
- [done] `CampaignFlowSystem` now declares typed subscriptions for its core campaign lifecycle messages, translates subscribed host gameplay actions into immediate typed internal messages, and emits `SiteRunStartedMessage` / `SiteSceneActivatedMessage` through `RuntimeInvocation::emit_game_message(...)`.
- [done] `EconomyPhoneSystem`, `PlacementValidationSystem`, and `TaskBoardSystem` now consume `SiteRunStartedMessage` through typed handlers instead of only through legacy envelope switching.
- [done] The remaining `SiteRunStartedMessage` site-system listeners now declare typed subscriptions/handlers, and the runtime dispatches `SiteRunStartedMessage` through compile-time subscriber resolution instead of falling back through the legacy envelope bridge.
- [done] Runtime production compatibility shadow state has been removed, including `compatibility_campaign_state_`, `compatibility_site_run_state_`, `flush_compatibility_state_to_split_state()`, and `refresh_compatibility_state_from_split_state()`.
- [done] `GameRuntime::find_system<System>()` now uses an O(1) compile-time pack index into `systems_by_pack_index_` instead of linear type matching.
- [done] The worker-meter, site-tile-state, and typed reward follow-up slices now dispatch immediate typed messages inline for `WorkerMetersChangedMessage`, `SiteTileStateChangedMessage`, `EconomyMoneyAwardRequestedMessage`, `InventoryDeliveryRequestedMessage`, `SiteUnlockableRevealRequestedMessage`, and `RunModifierAwardRequestedMessage`.
- [done] Touched internal debug log probes no longer emit dead-end internal `PresentLog` gameplay messages; they translate directly to runtime log transport instead.
- [done] `ActionExecutionSystem` no longer uses the internal gameplay queue for action-start, action-fail, or placement-commit-reject observation; those paths now publish through runtime-to-host action/placement/cue transport while gameplay coordination stays typed inline.
- [done] Debug builds now keep a shared semantic gameplay-message stack for both typed inline dispatch and legacy inline `GameMessage` dispatch, with focused regression coverage for push/pop visibility and clean unwind.
- [done] Runtime trait scaffolding now reads compile-time `profile_id` / `fixed_step_order_value` metadata with runtime virtual fallbacks, and the full active `GameSystems` pack now declares that metadata.
- [done] Legacy `GameMessage` subscriber registration now caches per-system profile metadata up front, so both fixed-step execution and legacy internal-message profiling consume trait-backed metadata without re-querying virtuals on the hot path.
- [done] Runtime trait scaffolding now reads compile-time `emitted_runtime_messages` metadata, and the active `GameSystems` pack now aggregates a compile-time runtime-to-host message manifest with regression coverage for the current direct host-transport emitters.
- [done] Legacy `GameMessage` envelopes for already-migrated typed families now bridge straight into compile-time typed subscriber dispatch, and the runtime no longer registers legacy enum-keyed subscriber-table entries for those migrated families.
- [done] The campaign-flow gameplay-action host path now translates directly at the runtime boundary into typed internal gameplay messages, so those host actions no longer depend on `CampaignFlowSystem` host-message subscriber registration before reaching typed dispatch.
- [done] The technology claim/refund gameplay-action host path now translates directly at the runtime boundary into typed internal gameplay messages, and the technology message family no longer needs legacy enum-keyed subscriber-table entries for those claim/refund requests.
- [done] The task-board accept/claim gameplay-action host path now translates directly at the runtime boundary into typed internal gameplay messages, and the task-request message family no longer needs legacy enum-keyed subscriber-table entries or `TaskBoardSystem` gameplay-action host subscription.
- [done] The site-modifier end gameplay-action host path now translates directly at the runtime boundary into typed internal gameplay messages, and the modifier-end request family no longer needs legacy enum-keyed subscriber-table entries or `ModifierSystem` gameplay-action host subscription.
- [done] The economy-phone buy/sell/hire/unlock gameplay-action host path now translates directly at the runtime boundary into typed internal gameplay messages, and the storefront request families no longer need legacy enum-keyed subscriber-table entries or `EconomyPhoneSystem` gameplay-action host subscription.
- [done] `TechnologySystem`, `TaskBoardSystem`, `ModifierSystem`, and `EconomyPhoneSystem` now route their already-migrated gameplay families through direct typed `handle(...)` paths, and their retired gameplay-action host subscriber overrides are reduced to inert no-op stubs behind runtime-boundary translation.
- [>] Runtime storage now has compile-time `GameSystems` ordering and O(1) typed lookup, but primary ownership/iteration still remains runtime-polymorphic rather than tuple-backed.
- [>] Some non-migrated internal gameplay paths still rely on the legacy `GameMessage` envelope, but that is a temporary migration state to be removed rather than a supported long-term bridge design.
- [>] Runtime-level queue draining and generic `process_game_message(const GameMessage&)` dispatch still exist for the remaining unmigrated gameplay message families.

## Work Items

### Phase 1: Core scaffolding

- [done] Create a small compile-time type-list utility for message and system traits.
- [done] Create a `system_pack` abstraction that preserves explicit system order.
- [done] Define runtime helpers for compile-time subscriber filtering from `subscribed_messages`.
- [done] Define typed internal gameplay dispatch entrypoints on the runtime and invocation layers.
- [>] Define a compile-time trait shape for `subscribed_messages`, `fixed_step_order`, `emitted_runtime_messages`, and profiling metadata.
  Runtime helpers now cover `subscribed_messages`, `profile_id`, `fixed_step_order_value`, and `emitted_runtime_messages`, with active-system adoption complete.
- [done] Decide the home for the new typed internal message definitions and supporting metaprogramming utilities.

### Phase 2: Runtime storage and registry refactor

- [>] Convert runtime system ownership from the current primary runtime-polymorphic registry shape to typed tuple-backed storage.
- [done] Preserve stable system ordering from the current `initialize_system_registry()` sequence.
  The `system_pack_index` helper is now corrected so the O(1) typed lookup path actually resolves the intended live runtime system instead of later pack entries.
- [>] Rework fixed-step discovery to derive from system traits and the compile-time system pack.
  Fixed-step registration now caches trait-backed order/profile metadata for the full active `GameSystems` pack, but the runtime still instantiates and iterates systems through the polymorphic registry.
- [>] Rework ownership/resolver registration to derive from the compile-time system pack.
- [>] Rework profiling registration and lookup to fit the typed system pack model.
  Typed inline dispatch, fixed-step execution, and legacy `GameMessage` profiling now consume cached compile-time profile metadata from the active system pack, while broader runtime-to-host validation still remains pending beyond the first emitted-message manifest scaffold.

### Phase 3: Typed gameplay-dispatch path

- [done] Add typed `dispatch<Message>(...)` support for immediate inline internal gameplay dispatch.
- [done] Add typed `emit(Message)` support on `RuntimeInvocation`.
- [done] Keep and strengthen nested inline-dispatch depth tracking and ownership restoration.
- [done] Add debug-only semantic nested gameplay message-call stack visibility for re-entrant dispatch chains.
- [>] Remove dependence on runtime-built internal gameplay subscriber arrays keyed by gameplay enum.
  Migrated gameplay-message families no longer populate or use the legacy enum-keyed subscriber table, but unmigrated families still do.

### Phase 4: Host-boundary translation

- [>] Refactor host-message dispatch so ABI host inputs decode and translate immediately into typed internal gameplay messages.
- [done] The campaign-flow gameplay-action host family (`start new campaign`, deployment selection/clear, start attempt, return to regional map) now translates directly in `GameRuntime` before host-subscriber fan-out.
- [done] The technology claim/refund gameplay-action host family now translates directly in `GameRuntime` before host-subscriber fan-out.
- [done] The task-board accept/claim gameplay-action host family now translates directly in `GameRuntime` before host-subscriber fan-out.
- [done] The site-modifier end gameplay-action host family now translates directly in `GameRuntime` before host-subscriber fan-out.
- [done] The economy-phone buy/sell/hire/unlock gameplay-action host family now translates directly in `GameRuntime` before host-subscriber fan-out.
- [x] Keep host-message ABI transport types unchanged.
- [>] Ensure translated host-originated gameplay work no longer depends on internal gameplay queueing.

### Phase 5: System trait adoption

- [done] Add compile-time gameplay trait declarations to all gameplay systems in the active system pack.
- [>] Convert systems away from generic internal gameplay envelope subscription lists toward typed message traits.
- [>] Convert systems away from large `switch (message.type)` gameplay handlers toward typed handlers.
- [x] Keep host-message handling behavior intact while the internal gameplay side migrates.

### Phase 6: Internal gameplay message migration

- [>] Replace internal gameplay `GameMessageType`-driven coordination with typed POD gameplay messages.
- [>] Remove internal gameplay payload packing/unpacking usage from migrated systems.
- [>] Remove internal gameplay enqueue helpers from site/campaign/runtime code paths.
- [>] Convert fixed-step systems that currently push gameplay queue messages to immediate typed emits.
- [x] Rewrite queue-inspection logic so gameplay state is represented explicitly rather than inferred from transport contents.

### Phase 7: Legacy internal gameplay transport retirement

- [>] Remove the internal gameplay `GameMessageQueue` runtime path.
- [>] Remove internal gameplay queue-drain loops for gameplay-message handling.
- [>] Remove internal gameplay runtime methods that exist only to drain queued gameplay messages.
- [>] Remove or repurpose the old internal gameplay envelope types once no longer needed for gameplay coordination.
- [>] Update runtime state and invocation plumbing so no internal gameplay queue remains.

### Phase 8: Compatibility shadow-state cleanup

- [x] Identify all production runtime paths that still depend on aggregate `CampaignState` / `SiteRunState` compatibility mirrors.
- [x] Migrate runtime production reads and writes to direct split-state usage.
- [x] Remove `compatibility_campaign_state_` and `compatibility_site_run_state_` from `GameRuntime`.
- [x] Remove `flush_compatibility_state_to_split_state()` and `refresh_compatibility_state_from_split_state()` from runtime phase/message flow.
- [x] Move any still-needed aggregate conversion helpers out of runtime production flow and into explicit migration/test-only boundaries where practical.
- [x] Update runtime tests that currently inspect compatibility mirrors so they validate split-state or exported-view behavior instead, then remove the old helpers entirely.

### Phase 9: Runtime-to-host manifest and validation

- [>] Aggregate a compile-time manifest of runtime-to-host message types emitted by the active system pack.
  A first compile-time manifest scaffold now aggregates the active pack's currently declared direct host-transport emitters (`ActionExecutionSystem` plus debug-log emitters), with deeper validation and broader adoption still pending.
- [>] Add compile-time validation that systems only emit runtime message types included in the manifest.
- [>] Document the relationship between the stable public ABI transport and the game-specific emitted-runtime-message manifest.

### Phase 10: Regression coverage and cleanup

- [>] Add or update focused runtime/system tests that lock immediate inline typed gameplay-dispatch behavior.
- [>] Add or update regression coverage for nested gameplay dispatch and ownership restoration.
  Direct semantic-stack scope coverage is now in place; deeper end-to-end ownership-restoration chains still need broader runtime/system coverage.
- [>] Add or update regression coverage for host-message immediate translation into typed gameplay dispatch.
- [>] Add or update regression coverage for removed queue-peeking behavior.
- [done] Add or update regression coverage for removal of compatibility shadow-state sync from runtime production flow.
- [>] Remove obsolete compatibility shims once all active gameplay systems are migrated.
- [>] Refresh related documentation after implementation lands.

## Exit Criteria

The refactor is complete when all of the following are true:

- Internal gameplay coordination no longer depends on the runtime `GameMessageQueue`.
- Internal gameplay coordination uses typed POD messages and immediate inline dispatch only.
- The runtime owns its active systems from one compile-time ordered system pack.
- Subscriber resolution is compile-time-derived from system traits, not runtime-built from gameplay enums.
- Runtime production flow no longer depends on aggregate gameplay-state compatibility mirrors or compatibility sync passes.
- Host input and runtime output ABI transports still work without public ABI breakage.
- Runtime-to-host message usage is documented by a compile-time manifest for the active game.
- Legacy queue-peeking gameplay logic is removed or rewritten as explicit state/lifecycle logic.
- Tests cover the new dispatch semantics and nested dispatch behavior.

## Resolved Design Decisions

- Internal typed gameplay message definitions stay in internal gameplay source headers only, not in public ABI headers.
- Message definitions should be organized by gameplay-domain ownership so systems stay reusable without dragging host/runtime ABI transport along with them.
- Transitional bridge code is not a target architecture. Any unavoidable bridge code must stay narrow, temporary, and slated for deletion as migrated systems land.
- Rename existing `SiteAttempt*`-style concepts toward clearer site-session lifecycle wording wherever they still remain.
- Debug builds should keep a semantic nested gameplay message stack in addition to native callstacks, because the semantic chain is easier to reason about during re-entrant dispatch loops.
- No aggregate gameplay-state compatibility helpers should remain, including test-only leftovers. The end state is split authoritative state only.
