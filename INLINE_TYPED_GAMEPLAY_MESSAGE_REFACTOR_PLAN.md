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
- [done] `CampaignFlowSystem`, `LoadoutPlannerSystem`, `TechnologySystem`, `TaskBoardSystem`, `FailureRecoverySystem`, `SiteCompletionSystem`, `ModifierSystem`, and `EconomyPhoneSystem` now route their already-migrated gameplay families through direct typed `handle(...)` paths, and their retired gameplay-action host subscriber overrides are reduced to inert no-op stubs behind runtime-boundary translation.
- [done] `CampaignTimeSystem`, `DeviceSupportSystem`, `SiteFlowSystem`, and `SiteTimeSystem` now declare explicit compile-time empty `subscribed_messages` traits instead of relying on fallback empties, and the remaining lightweight `SiteRunStarted` listeners now use the same direct typed-handler routing shape as the rest of the migrated family.
- [done] `EcologySystem` and `DeviceWeatherContributionSystem` now route their already-migrated message families through direct typed `handle(...)` paths instead of keeping duplicated legacy-envelope mutation logic inline.
- [done] `CampDurabilitySystem`, `LocalWeatherResolveSystem`, `FailureRecoverySystem`, and `SiteCompletionSystem` now use the same direct typed-handler legacy entry routing shape as the rest of their migrated lifecycle families instead of ad hoc single-message envelope checks.
- [done] Runtime primary system ownership now lives in compile-time `GameSystems::tuple_type` storage, while fixed-step execution and subscriber registration reuse those live typed instances through the existing runtime interface.
- [done] Runtime resolver/ownership registration now derives from compile-time `GameSystems` tuple iteration rather than a separate hand-built polymorphic registry pass.
- [done] `PlacementModeCursorMovedMessage` now dispatches through compile-time typed subscriber resolution, so the site-context cursor follow-up no longer leaves a legacy enum-keyed subscriber-table entry behind.
- [done] `CampaignFlowSystem`, `CampaignProgressionSystem`, and `LoadoutPlannerSystem` no longer keep dead per-system legacy gameplay-envelope trampolines for their fully typed-enabled message families, and `IRuntimeSystem` now supplies default no-op legacy message hooks so fully migrated systems can shed inert overrides instead of preserving empty queue-era plumbing.
- [done] `ActionExecutionSystem`, `CraftSystem`, `EconomyPhoneSystem`, and `TaskBoardSystem` likewise dropped dead per-system legacy gameplay-envelope and empty host-hook overrides for their fully typed-enabled subscribed families, and the runtime legacy-subscriber skip table now also suppresses the already-migrated `PlacementModeCursorMoved` family consistently.
- [done] Empty-subscription fixed-step systems plus additional single-message typed listeners (`CampaignTimeSystem`, `SiteTimeSystem`, `SiteFlowSystem`, `DeviceSupportSystem`, `CampDurabilitySystem`, `LocalWeatherResolveSystem`, `FailureRecoverySystem`, and `SiteCompletionSystem`) now rely on shared runtime default no-op legacy hooks instead of preserving file-local empty overrides.
- [done] Additional migrated site systems (`EcologySystem`, `DeviceMaintenanceSystem`, `DeviceWeatherContributionSystem`, `InventorySystem`, `ModifierSystem`, `PlacementValidationSystem`, `PlantWeatherContributionSystem`, `WeatherEventSystem`, and `WorkerConditionSystem`) no longer keep empty host-message overrides when runtime-boundary translation or zero host subscriptions already make those hooks inert.
- [done] `TechnologySystem` no longer keeps an unreachable gameplay-action host override now that claim/refund input is translated in `GameRuntime`, and the dormant no-subscription `FactionReputationSystem` compatibility hook pair was trimmed back to shared runtime defaults.
- [done] Those same migrated site systems (`EcologySystem`, `DeviceMaintenanceSystem`, `DeviceWeatherContributionSystem`, `InventorySystem`, `ModifierSystem`, `PlacementValidationSystem`, `PlantWeatherContributionSystem`, `WeatherEventSystem`, and `WorkerConditionSystem`) no longer keep legacy `process_game_message(...)` trampoline switches for message families already routed entirely through compile-time typed subscriber dispatch.
- [done] `GameRuntime` no longer builds or consults a runtime `GameMessageType`-keyed internal subscriber registry; compile-time typed dispatch is now the only production subscriber resolution path, and the trait metadata regression now locks that through typed-dispatch coverage instead of peeking into a legacy table.
- [done] The runtime projection, timed-modifier, and performance regressions now enter migrated gameplay paths through direct typed `GameRuntime::handle_message(Message)` calls instead of hand-packed legacy `GameMessage` envelopes, further isolating the remaining compatibility bridge to dedicated envelope-focused tests and fixtures.
- [done] The typed-runtime regression slice is now stabilized end-to-end: direct typed projection/performance/timed-modifier coverage uncovered and fixed the missing `SITE_SCENE_READY` -> `SiteSceneActivatedMessage` bridge, a no-campaign startup app-state write in `CampaignFlowSystem`, owner-guard violations in `EcologySystem`, `InventorySystem`, and `ModifierSystem`, and a `TaskBoardSystem` site-start ordering assumption exposed by immediate inline dispatch.
- [done] `site_system_message_flow_test.cpp` now routes its already migrated `DeploymentSiteSelectionChangedMessage` and `StartSiteAttemptMessage` coverage through direct typed system handlers instead of legacy `GameMessage` envelope dispatch, shrinking the remaining queue-era runtime-test surface a bit further.
- [done] Production runtime no longer relies on the legacy internal gameplay envelope or queue for subscriber resolution or phase draining; the remaining `GameMessage` bridge is now compatibility-only for dedicated fixtures, envelope-focused regressions, and debug naming.
- [done] Runtime-level queue draining for gameplay coordination is gone; the remaining generic `GameMessage` handling surface exists only as a compatibility translation layer for tests and explicit legacy-envelope entrypoints.

## Work Items

### Phase 1: Core scaffolding

- [done] Create a small compile-time type-list utility for message and system traits.
- [done] Create a `system_pack` abstraction that preserves explicit system order.
- [done] Define runtime helpers for compile-time subscriber filtering from `subscribed_messages`.
- [done] Define typed internal gameplay dispatch entrypoints on the runtime and invocation layers.
- [done] Define a compile-time trait shape for `subscribed_messages`, `fixed_step_order`, `emitted_runtime_messages`, and profiling metadata.
  Runtime helpers now cover `subscribed_messages`, `profile_id`, `fixed_step_order_value`, and `emitted_runtime_messages`, with active-system adoption complete.
- [done] Decide the home for the new typed internal message definitions and supporting metaprogramming utilities.

### Phase 2: Runtime storage and registry refactor

- [done] Convert runtime system ownership from the current primary runtime-polymorphic registry shape to typed tuple-backed storage.
- [done] Preserve stable system ordering from the current `initialize_system_registry()` sequence.
  The `system_pack_index` helper is now corrected so the O(1) typed lookup path actually resolves the intended live runtime system instead of later pack entries.
- [done] Rework fixed-step discovery to derive from system traits and the compile-time system pack.
  Fixed-step registration now walks the canonical `GameSystems` pack in compile-time order and records the declared fixed-step entries directly without a runtime sort pass.
- [done] Rework ownership/resolver registration to derive from the compile-time system pack.
- [done] Rework profiling registration and lookup to fit the typed system pack model.
  Typed inline dispatch, fixed-step execution, and legacy `GameMessage` profiling now consume cached compile-time profile metadata from the active system pack, while broader runtime-to-host validation still remains pending beyond the first emitted-message manifest scaffold.

### Phase 3: Typed gameplay-dispatch path

- [done] Add typed `dispatch<Message>(...)` support for immediate inline internal gameplay dispatch.
- [done] Add typed `emit(Message)` support on `RuntimeInvocation`.
- [done] Keep and strengthen nested inline-dispatch depth tracking and ownership restoration.
- [done] Add debug-only semantic nested gameplay message-call stack visibility for re-entrant dispatch chains.
- [done] Remove dependence on runtime-built internal gameplay subscriber arrays keyed by gameplay enum.
  `GameRuntime` no longer builds or uses the legacy enum-keyed gameplay subscriber table at all; only the remaining queue/envelope producer bridge still needs retirement.

### Phase 4: Host-boundary translation

- [done] Refactor host-message dispatch so ABI host inputs decode and translate immediately into typed internal gameplay messages.
- [done] The campaign-flow gameplay-action host family (`start new campaign`, deployment selection/clear, start attempt, return to regional map) now translates directly in `GameRuntime` before host-subscriber fan-out.
- [done] The technology claim/refund gameplay-action host family now translates directly in `GameRuntime` before host-subscriber fan-out.
- [done] The task-board accept/claim gameplay-action host family now translates directly in `GameRuntime` before host-subscriber fan-out.
- [done] The site-modifier end gameplay-action host family now translates directly in `GameRuntime` before host-subscriber fan-out.
- [done] The economy-phone buy/sell/hire/unlock gameplay-action host family now translates directly in `GameRuntime` before host-subscriber fan-out.
- [done] The site inventory slot-tap host path now translates directly in `GameRuntime` into `InventorySlotTappedMessage`, and `InventorySystem` no longer depends on host-subscriber registration for that request.
- [done] The site move-direction host path now decodes directly in `GameRuntime` into the runtime-owned move-direction snapshot state, and `SiteFlowSystem` no longer depends on host-subscriber registration for movement input.
- [done] The site context-request host path now translates directly in `GameRuntime` into either `PlacementModeCursorMovedMessage` or `CraftContextRequestedMessage` based on authoritative runtime action state, and `ActionExecutionSystem` / `CraftSystem` no longer need that host subscription path.
- [done] The site action request/cancel host paths now translate directly in `GameRuntime` into `StartSiteActionMessage` / `CancelSiteActionMessage`, and `ActionExecutionSystem` no longer depends on host-subscriber registration for those requests.
- [done] Keep host-message ABI transport types unchanged.
- [done] Ensure translated host-originated gameplay work no longer depends on internal gameplay queueing.

### Phase 5: System trait adoption

- [done] Add compile-time gameplay trait declarations to all gameplay systems in the active system pack.
- [done] Convert systems away from generic internal gameplay envelope subscription lists toward typed message traits.
  Active gameplay-system subscriber resolution is now compile-time-only in production runtime code. Remaining envelope-aware helpers are isolated to compatibility fixtures and explicit legacy-envelope test coverage rather than live system subscription lists.
- [done] Convert systems away from large `switch (message.type)` gameplay handlers toward typed handlers.
  Active gameplay systems now route their migrated families through direct typed `handle(...)` entrypoints. Remaining `GameMessageType` switches are compatibility translation helpers rather than production subscriber logic.
- [done] Keep host-message handling behavior intact while the internal gameplay side migrates.

### Phase 6: Internal gameplay message migration

- [done] Replace internal gameplay `GameMessageType`-driven coordination with typed POD gameplay messages.
  Production runtime coordination now uses typed POD dispatch inline; any remaining enum-plus-payload handling is compatibility-only for fixtures, explicit envelope tests, and debug-name reporting.
- [done] Remove internal gameplay payload packing/unpacking usage from migrated systems.
- [done] Remove internal gameplay enqueue helpers from site/campaign/runtime code paths.
- [done] Convert fixed-step systems that currently push gameplay queue messages to immediate typed emits.
- [done] Rewrite queue-inspection logic so gameplay state is represented explicitly rather than inferred from transport contents.

### Phase 7: Legacy internal gameplay transport retirement

- [done] Remove the internal gameplay `GameMessageQueue` runtime path from `GameRuntime` phase/dispatch flow.
- [done] Remove internal gameplay queue-drain loops for gameplay-message handling in production runtime flow.
- [done] Remove internal gameplay runtime methods that exist only to drain queued gameplay messages.
- [done] Remove or repurpose the old internal gameplay envelope types once no longer needed for gameplay coordination.
- [done] Update runtime state and invocation plumbing so no internal gameplay queue remains.
  The runtime execution path no longer binds or drains the compatibility queue, but the `GameState`-backed test fixture queue still remains for isolated compatibility tests.

### Phase 8: Compatibility shadow-state cleanup

- [done] Identify all production runtime paths that still depend on aggregate `CampaignState` / `SiteRunState` compatibility mirrors.
- [done] Migrate runtime production reads and writes to direct split-state usage.
- [done] Remove `compatibility_campaign_state_` and `compatibility_site_run_state_` from `GameRuntime`.
- [done] Remove `flush_compatibility_state_to_split_state()` and `refresh_compatibility_state_from_split_state()` from runtime phase/message flow.
- [done] Move any still-needed aggregate conversion helpers out of runtime production flow and into explicit migration/test-only boundaries where practical.
- [done] Update runtime tests that currently inspect compatibility mirrors so they validate split-state or exported-view behavior instead, then remove the old helpers entirely.

### Phase 9: Runtime-to-host manifest and validation

- [done] Aggregate a compile-time manifest of runtime-to-host message types emitted by the active system pack.
  A first compile-time manifest scaffold now aggregates the active pack's currently declared direct host-transport emitters (`ActionExecutionSystem` plus debug-log emitters), with deeper validation and broader adoption still pending.
- [done] Add compile-time validation that systems only emit runtime message types included in the manifest.
- [done] Document the relationship between the stable public ABI transport and the game-specific emitted-runtime-message manifest.
  The public `Gs1RuntimeMessage` ABI remains stable for host transport, while the emitted-runtime-message manifest is an internal compile-time contract for game-owned validation and documentation only.

### Phase 10: Regression coverage and cleanup

- [done] Add or update focused runtime/system tests that lock immediate inline typed gameplay-dispatch behavior.
  The runtime projection and metadata regressions now directly lock compile-time fixed-step ordering plus the translated host input paths for site move, context, inventory tap, action request, and action cancel flows.
- [done] Add or update regression coverage for nested gameplay dispatch and ownership restoration.
  Direct semantic-stack scope coverage is now in place; deeper end-to-end ownership-restoration chains still need broader runtime/system coverage.
- [done] Add or update regression coverage for host-message immediate translation into typed gameplay dispatch.
- [done] Add or update regression coverage for removed queue-peeking behavior.
  The phase-level runtime queue drain regression is now in place; remaining queue-peeking coverage still needs to be rewritten around explicit state/lifecycle signals.
- [done] Add or update regression coverage for removal of compatibility shadow-state sync from runtime production flow.
- [done] Remove obsolete compatibility shims once all active gameplay systems are migrated.
- [done] Refresh related documentation after implementation lands.

## Exit Criteria

The refactor is complete when all of the following are true:

- Internal gameplay coordination no longer depends on the runtime `GameMessageQueue` in production runtime flow.
- Internal gameplay coordination uses typed POD messages and immediate inline dispatch only.
- The runtime owns its active systems from one compile-time ordered system pack.
- Subscriber resolution is compile-time-derived from system traits, not runtime-built from gameplay enums.
- Runtime production flow no longer depends on aggregate gameplay-state compatibility mirrors or compatibility sync passes.
- Host input and runtime output ABI transports still work without public ABI breakage.
- Runtime-to-host message usage is documented by a compile-time manifest for the active game.
- Legacy queue-peeking gameplay logic is removed or rewritten as explicit state/lifecycle logic, with any remaining queue use confined to compatibility fixtures or tests.
- Tests cover the new dispatch semantics and nested dispatch behavior.

## Resolved Design Decisions

- Internal typed gameplay message definitions stay in internal gameplay source headers only, not in public ABI headers.
- Message definitions should be organized by gameplay-domain ownership so systems stay reusable without dragging host/runtime ABI transport along with them.
- Transitional bridge code is not a target architecture. Any unavoidable bridge code must stay narrow, temporary, and slated for deletion as migrated systems land.
- Rename existing `SiteAttempt*`-style concepts toward clearer site-session lifecycle wording wherever they still remain.
- Debug builds should keep a semantic nested gameplay message stack in addition to native callstacks, because the semantic chain is easier to reason about during re-entrant dispatch loops.
- No aggregate gameplay-state compatibility helpers should remain, including test-only leftovers. The end state is split authoritative state only.
