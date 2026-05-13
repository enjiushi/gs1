# Gameplay State Refactor Plan

This document turns the requested architecture shift into a staged refactor plan for the current GS1 gameplay DLL and Godot adapter. For this pass, ignore tests and both smoke-host variants; keep the Godot host in scope.

## Goals

The target architecture is:

- The gameplay DLL owns gameplay state, not UI state.
- The exported gameplay state should contain gameplay data only, not UI data, dirty flags, or intermediate logic/projection state.
- The engine host owns UI state, scene state, and presentation state.
- Host-to-gameplay messages remain as input/intention contracts.
- Internal gameplay messages remain only where they express gameplay intent or gameplay facts that happened, not authoritative state transfer.
- The host can read gameplay state directly through a const ABI-safe surface of pointers/counts/query entry points instead of rebuilding state from runtime message snapshots/upserts.
- Systems still have explicit ownership over mutable gameplay state slices.
- The gameplay DLL remains the sole authority for gameplay rules and gameplay data mutation.

## Non-Goals

This refactor should not try to do all of the following at once:

- Rewriting all gameplay logic away from the existing system model.
- Replacing Flecs/ECS wholesale during the same migration.
- Making the entire internal runtime state trivially copyable in one step.
- Collapsing host input contracts and gameplay intent contracts into one giant untyped command channel.

## Current Structure Summary

Today the codebase has three overlapping data flow styles:

- Direct runtime-owned aggregate state in [src/runtime/game_state.h](/d:/testgame/gs1_upstream/src/runtime/game_state.h)
- Internal `GameMessage` publish/subscribe flow in [src/messages/game_message.h](/d:/testgame/gs1_upstream/src/messages/game_message.h)
- Runtime-to-host projection messages and snapshots in [include/gs1/types.h](/d:/testgame/gs1_upstream/include/gs1/types.h)

The current `GameState` is not ABI-safe to expose directly because it contains C++ owning/runtime types such as:

- `std::optional`
- `std::deque`
- `std::shared_ptr`
- `std::vector`
- Flecs-backed world ownership through `SiteWorld`

The gameplay DLL also still owns presentation-oriented state such as:

- `UiPresentationState`
- `PresentationRuntimeState`
- `SiteProtectionPresentationState`
- site-side projected tile and inventory dirty/update caches
- phone-panel state that is partly UI/session state rather than pure gameplay

That means the desired end state is not a small cleanup. It is a boundary redesign across gameplay, ABI, and adapter responsibilities.

## Confirmed Decisions

These points are now considered decided for the plan:

- Expose all gameplay data slices through the exported state surface, but only actual game data.
- Do not expose dirty flags, projection caches, or similar logic/intermediate state as gameplay state.
- Systems should avoid storing dirty/intermediate logic state inside the long-lived exported game state.
- Use simple coarse runtime-to-host change granularity first; finer granularity can wait for a later refactor.
- For Flecs-heavy or archetype-backed data such as dense tiles, prefer query functions for now.
- Remove snapshot/state-transfer messages and other dead message families first, then inspect what runtime messages remain.
- Add query/helper APIs only when necessary rather than designing a huge query surface up front.
- Do the gameplay-side refactor first.
- Do not block the gameplay refactor on tests, smoke host, or Godot adapter compatibility; repair them afterward one by one.

## Current Execution Rule

For this refactor pass, prioritize gameplay-DLL architecture changes only, plus the Godot host path.

- Do not spend refactor time preserving either smoke-host variant, shared-host-bridge compatibility, or automated-test compatibility unless one of them directly blocks gameplay-side progress.
- Temporary compile breakage or runtime breakage in smoke hosts and tests is acceptable during this migration.
- Do not treat smoke-host, host/test fallout, or automated-test repair as required follow-up work for the same change set unless the user explicitly asks for that repair pass.
- When choosing between gameplay cleanup and compatibility glue, prefer the gameplay cleanup.

This rule is intentionally stricter than the earlier execution note so the refactor does not balloon into a whole-repo compatibility project.

## Recommended Target Model

### 1. Split internal gameplay state from exported read state access

Use two layers instead of trying to force the existing internal runtime object graph into a POD ABI shape:

- `GameRuntimeState` or similar: internal mutable runtime state, may keep ECS, vectors, ownership, caches, allocators, etc.
- `Gs1GameStateView` or similar: exported const ABI-safe read access surface composed only of trivial standard-layout structs, scalar fields, pointers, counts, stable IDs, and optional query helper entry points.

Recommendation:

- Do not expose the current internal `GameState*` directly to the host.
- Expose a pointer to a dedicated exported read-only state-view struct whose schema is explicitly designed for ABI stability.
- That view should be built once and stay stable as a view object.
- The view should primarily contain pointers into authoritative gameplay-owned data plus counts and helper metadata.
- Only rebuild the view fields when an exposed pointer must change, such as when a backing container reallocates or when a different gameplay substate becomes active.

This gives the host direct state reads without forcing all gameplay internals to become POD or paying a broad copy cost.

### 2. Keep messages as intents, requests, and notifications of completion

Messages should remain for:

- host input intents: start campaign, select site, begin action, cancel action, claim reward
- internal gameplay broadcasts of intent or completion that other systems may observe
- lifecycle/intention coordination: site scene ready, return to regional map, end modifier requested
- runtime-to-host state-change notifications so host controllers know when to poll/rebuild specific UI or scene data

Messages should stop carrying authoritative state payloads whose only purpose is projection, including:

- full UI snapshots
- UI visibility snapshots
- panel entry upserts/removals
- tile projection deltas meant only for rendering
- meter-changed projection messages used only to feed UI
- data-transfer messages whose payload duplicates readable state already available through the authoritative game state view/query APIs

Also:

- delete old messages that are no longer used after the migration
- explicitly remove snapshot and state-transfer message families rather than leaving dead compatibility paths behind
- start with simple coarse host-facing state-change messages rather than solving fine-grained refresh granularity now

### 3. Move all UI/session/presentation ownership to engine-side code

Gameplay should stop owning:

- panel open/close state
- selected UI tab/section state
- unread badge state that is purely presentational
- inventory window/view session state
- regional overlay open state
- presentation dirty tracking used only to drive host redraw/update

Gameplay may still own:

- campaign/site current phase/app mode, because that is gameplay flow, not pure UI

Recommendation:

- Separate "gameplay mode" from "UI surface state" aggressively.
- If a value can be reconstructed by the engine from gameplay state plus local UI interaction history, it should live in the engine.
- For host-side UI-only decisions that need gameplay validation or derived answers, add direct gameplay query/helper APIs instead of routing them through messages.
- Queries should be read-only and side-effect free.
- Queries are the right fit for things like placement legality, contextual action availability, tile inspection data, or other "ask gameplay now" operations that must answer immediately.

### 4. Treat ECS as internal authoritative storage, not exported ABI data

For ECS, the safest direction is:

- keep Flecs/internal ECS private to the gameplay DLL
- expose only stable readable slices or query surfaces derived from ECS into the exported `Gs1GameStateView`
- keep stable gameplay entity IDs across frames
- let the engine query arrays/tables/views of stable POD data, not ECS internals

Recommendation for ECS handling:

- Do not expose Flecs world pointers, entity handles, or component storage directly across the DLL ABI.
- Prefer pointer-and-count access when the underlying storage is already ABI-safe enough to expose through a stable view.
- Prefer explicit query helper functions when the underlying data is ECS-backed, container-backed, sparse, expensive to flatten, or unsafe to expose directly.
- Keep ECS ownership and mutation rules exactly on the gameplay side.

Current direction:

- expose all gameplay slices eventually
- for Flecs/archetype-backed slices, especially dense site tile access, use query functions first rather than raw exported pointers

This preserves your system ownership model while removing snapshot/upsert projection traffic.

## Proposed Architecture Changes

### A. Public ABI changes

Add a new public read-only gameplay state API:

- `gs1_get_game_state_view(runtime, out_ptr, out_revision)` or equivalent
- optional query helper functions for ECS-backed or otherwise unsafe/heavy state slices
- optional targeted getters for large slices if a direct pointer field is not a good ABI choice

Likely new public types:

- `Gs1GameStateView`
- `Gs1CampaignStateView`
- `Gs1SiteStateView`
- `Gs1InventoryStateView`
- `Gs1TaskBoardStateView`
- `Gs1TechStateView`
- `Gs1TileView`
- `Gs1PlantView`
- `Gs1DeviceView`

Likely retained public types:

- `Gs1HostMessage`
- `Gs1Phase1Request/Result`
- `Gs1Phase2Request/Result`
- profiling APIs

Note on `Gs1Phase1Request/Result` and `Gs1Phase2Request/Result`:

- these are not host messages
- they are the current split runtime-step ABI used by the host
- today the host both submits host messages and separately advances gameplay through `gs1_run_phase1(...)` and `gs1_run_phase2(...)`
- if we later decide this split is unnecessary, that can be a separate API cleanup, but it is not the same concern as the state-view refactor

Likely reduced/deprecated public types:

- most `Gs1RuntimeMessage` payload types used only for snapshots/upserts/UI projection

### B. Runtime state changes

Restructure the current runtime state into:

- internal gameplay-owned mutable state
- exported read-only state-view object
- transient queues for host intents and internal gameplay broadcasts

Target cleanup direction:

- gameplay state should converge toward actual gameplay data
- dirty flags, projection caches, and intermediate logic state should be removed from exported gameplay state and reduced inside long-lived gameplay state where practical

Expected removals or reductions:

- [src/runtime/ui_presentation_state.h](/d:/testgame/gs1_upstream/src/runtime/ui_presentation_state.h)
- [src/runtime/presentation_runtime_state.h](/d:/testgame/gs1_upstream/src/runtime/presentation_runtime_state.h)
- projection dirty caches inside [src/site/site_run_state.h](/d:/testgame/gs1_upstream/src/site/site_run_state.h)
- gameplay-owned UI state in [src/ui/](/d:/testgame/gs1_upstream/src/ui)

### C. Internal message changes

Refactor `GameMessageType` into clearer categories:

- host/gameplay intent requests
- gameplay facts/broadcasts that other systems may observe
- lifecycle coordination
- completion/result notifications only where another gameplay owner genuinely needs to react

Expected message cleanup:

- remove internal messages that only exist to push data into presentation systems
- remove internal messages that mirror readable state one-to-one
- keep only messages that express gameplay intention or gameplay facts that occurred
- do not treat messages as directed requests to a specific owner/system

Good long-term rule:

- if a payload says "here is the new state for rendering", it should probably become state-view data
- if a payload says "a gameplay action/intention occurred" or "a gameplay-relevant fact happened", it may stay a message

### D. Engine host / adapter changes

Host and engine adapters should:

- continue sending host messages/intents into gameplay
- stop relying on authoritative gameplay UI snapshot messages
- read the exported game state view directly
- build scenes and UI from state plus stable gameplay IDs
- keep their own local UI/session state
- still wait for gameplay-driven app/scene-flow messages before switching scenes or entering gameplay phases

Important rule:

- the host may propose a scene/app-flow change by message
- the gameplay DLL remains authoritative over app state and scene flow
- the host must wait for gameplay's answer message before doing scene transitions that depend on gameplay state

This affects:

- [src/host/runtime_session.h](/d:/testgame/gs1_upstream/src/host/runtime_session.h)
- Godot adapter code under [engines/godot/native/](/d:/testgame/gs1_upstream/engines/godot/native)
- smoke host/viewer code under [tests/smoke/](/d:/testgame/gs1_upstream/tests/smoke)

### E. System boundary changes

The current "systems claim state access" rule is still useful and should stay.

Recommended adjustment:

- systems claim mutable access only to their owned internal runtime state
- systems may read exported/shared gameplay state slices as const
- systems should not mutate exported host-facing view structs directly unless they own the pointer metadata or helper output that backs that slice

Best pattern:

- runtime tick order stays as the existing gameplay order
- authoritative internal data mutates in the systems that own it
- the exported view object itself remains fixed when possible
- only pointer/count metadata that became invalid should be repaired/rebound

## Refactor Phases

### Phase 0: Design freeze and state taxonomy

Produce a concrete inventory of every current state field and classify it as:

- gameplay-authoritative internal state
- exported gameplay read pointer/query surface
- engine-owned UI/presentation state
- transient intent queue data
- temporary migration-only compatibility state

Deliverables:

- state taxonomy doc/table
- message inventory with keep/remove/replace decisions
- ABI draft for the exported state view and query/helper API set

### Phase 1: Introduce the exported read-state ABI and begin runtime-message cleanup

Add the new read-only game-state API while beginning the runtime-message cleanup.

Work:

- define POD ABI structs in `include/gs1/types.h` or split state-view headers
- add API entry point(s) in [include/gs1/game_api.h](/d:/testgame/gs1_upstream/include/gs1/game_api.h)
- add query/helper APIs for data that should not be exposed by raw pointer
- build a fixed exported view object that points into authoritative gameplay-owned data
- add simple coarse runtime messages so hosts/controllers know what to refresh or rebuild

Goal:

- let hosts begin reading state directly before all controllers are migrated

### Phase 2: Remove gameplay-owned UI state from the runtime

Move UI session concerns out of gameplay.

Work:

- delete or shrink gameplay UI state containers
- move panel/view/session decisions to host-side controllers
- stop generating UI projection snapshots/upserts
- keep only gameplay flow state where rules require it
- remove gameplay UI systems that existed only to manage projected UI state or snapshot emission

Goal:

- gameplay owns gameplay only

### Phase 3: Replace host projection consumption with direct state reads

Update the smoke host and Godot adapter to render from `Gs1GameStateView`.

Work:

- replace snapshot/upsert consumers with state readers
- preserve stable gameplay-ID to engine-object mapping
- convert UI controllers to build from gameplay state + local UI state
- keep host input submission unchanged wherever possible
- choose per controller whether it should poll every frame or refresh only on specific gameplay change messages

Goal:

- engine no longer waits for UI snapshots to know what exists

Execution note:

- this host/adapter migration can happen after the gameplay-side refactor lands
- temporary breakage in smoke host or Godot adapter is acceptable during the refactor phase
- compatibility repair for hosts/tests is explicitly out of scope for the current pass unless requested later

### Phase 4: Reduce runtime-to-host messages to the minimal useful set

Audit `Gs1RuntimeMessage` and remove projection-oriented traffic.

Candidates to keep:

- logs
- lifecycle notifications
- app-state/scene-flow answers from gameplay to host
- simple coarse state-change messages that tell controllers what should refresh
- optional one-shot cues that are truly event-like and not naturally derivable from state

Candidates to remove:

- panel setup batches
- snapshot begin/end structures
- upsert/remove item streams
- visibility projection streams
- meter and tile projection messages when the same data is readable in state

Goal:

- runtime-to-host messaging becomes small, semantic, and optional rather than the primary read path

### Phase 5: Simplify internal gameplay messaging

Now that projection systems are gone, trim internal `GameMessage` usage.

Work:

- remove presentation-only subscribers
- remove state-transfer messages that existed only for UI projection
- rename remaining messages toward intent/fact/broadcast terminology
- document which systems broadcast which messages and which systems subscribe to them

Goal:

- internal messaging becomes clearer and less noisy

### Phase 6: Rework tests around state-view assertions

Update automated coverage to assert against authoritative gameplay state and exported state view.

Work:

- system tests assert internal state mutations directly where appropriate
- host/adapter tests assert exported read-state layout and contents
- smoke/system tests stop depending on projection-message transcripts for correctness

Goal:

- tests align with the new architecture instead of pinning old projection behavior

Execution note:

- this phase is deferred for the current pass
- do not pause gameplay-side refactor work just to rewrite or restore tests

## Detailed Recommendations

### Recommendation 1: Do not make the entire internal `GameState` POD

This is the most important recommendation.

Trying to make the whole internal runtime state POD would force harmful compromises:

- loss of useful containers/ownership patterns
- awkward manual memory management
- likely larger and riskier rewrite than needed
- pressure to expose ECS internals directly across ABI

Better approach:

- keep internal runtime state ergonomic and gameplay-friendly
- create a separate exported POD state view for ABI consumption
- keep that exported state focused on actual gameplay data rather than dirty/projection/intermediate state

### Recommendation 2: Separate authoritative state from query shape

The host does not need the internal storage format.
The host needs a stable read shape.

That means the exported state should be designed for:

- ABI safety
- engine simplicity
- stable IDs
- direct iteration
- minimal indirection

Not for:

- being the same type as gameplay internals
- preserving internal ownership/container choices

### Recommendation 3: Expose all gameplay slices, using pointers where safe and query helpers where necessary

For host reads, prefer direct pointer access when all of these are true:

- the backing storage address can remain stable for useful periods
- the element type is ABI-safe
- the host naturally wants to iterate the whole slice
- exposing the storage does not leak internals we may want to change freely

Prefer query helper functions when any of these are true:

- the data lives in ECS or other internal storage we do not want to expose
- the slice is sparse or expensive to flatten
- the host usually needs targeted lookup rather than full iteration
- the backing container may reallocate unpredictably
- the layout is not ABI-safe

Good pointer-based slices may include things like:

- sites
- campaign progression arrays
- some inventory/storage slot arrays
- some fixed-size gameplay structs

Good query-based slices may include things like:

- dense tile data when the adapter builds instancing data tile-by-tile
- ECS-backed plants/devices/entities
- contextual legality checks
- contextual action availability
- heavy derived phone/task listing materialization if raw state iteration is not cheap enough

### Recommendation 4: Move phone-panel state out of gameplay unless it affects rules

Current `PhonePanelState` mixes UI/session concerns with gameplay-adjacent listing data.

Suggested split:

- gameplay owns the actual data source: tasks, listings, rewards, inventory, economy
- host owns which tab is open, what is highlighted, what badge has been dismissed, cart presentation, scroll state, etc.

Current decision:

- cart is host draft state
- only checkout/purchase actions are gameplay intents

### Recommendation 5: Keep scene readiness and similar host/gameplay coordination messages

Your note about host messages mostly staying the same makes sense.

Keep messages like:

- `SITE_SCENE_READY`
- direct action requests
- menu/campaign/site intent messages

These are good contract messages because they express intent or lifecycle coordination rather than state transfer.

## ECS Advice

Your uncertainty around ECS is the right place to pause, because this can go wrong if we over-expose it.

Recommended ECS strategy:

1. Keep ECS private inside gameplay.
2. Continue using ECS for simulation and sparse ownership where it already fits.
3. Expose ECS-backed state either through stable read pointers where safe or through dedicated query helper functions where pointer exposure is not a good fit.
4. Preserve stable gameplay IDs on all projected/queryable entities.
5. Avoid making the host depend on ECS archetypes, Flecs ids, or component layouts.

For exported read state/query APIs, prefer:

- tile arrays for dense map data
- flat arrays for sparse entities such as plants/devices/actions
- index/count references instead of nested STL containers
- helper query functions when direct pointer exposure is not stable enough or would leak internals

Possible hybrid layout:

- `campaign_view`
- `site_view`
- `site_view.tiles` pointer/count or tile query functions
- `site_view.plants` pointer/count or plant query functions
- `site_view.devices` pointer/count or device query functions
- `site_view.inventory_containers[]`
- `site_view.tasks[]`

This gives the engine direct reads where that is cheap and query helpers where that is safer, while gameplay stays free to simulate through ECS internally.

## Pointer Rebind Strategy

Because some exported slices will point into container-backed data, pointer invalidation must be handled explicitly.

Current direction:

- the exported view object stays fixed
- when a backing container reallocates, the affected pointer/count fields are rebound
- for now we accept that certain pointers must be reassigned after mutation paths that may reallocate

Recommended implementation guard:

- define the exported pointer-backed slices in one compile-time registry/trait list
- define which slices are backed by potentially reallocating containers
- drive the rebuild/rebind function from that registry so we do not forget to rebind one field manually

Goal:

- make pointer rebind coverage mechanically complete instead of relying on memory or comments

## Risks

### Technical risks

- ABI breakage if the new exported state view is not versioned carefully
- temporary duplication while both projection messages and state view exist
- subtle gameplay/UI regressions when moving session/UI state out of gameplay
- pointer invalidation when backing containers reallocate
- test churn because many current tests are message-driven

### Design risks

- temptation to reintroduce UI state into gameplay for convenience
- too many ad hoc query helpers if we do not keep the public query surface disciplined

## Open Questions

These are the main questions I think we should answer before implementation starts:

1. Which gameplay state slices are safe and worthwhile to expose by direct pointer/count, and which should only be exposed through query helpers?
2. What exact simple coarse runtime-to-host state-change messages do we want in the first pass?
3. What compile-time registry/trait shape should drive pointer rebind coverage for reallocating containers?
4. Which current runtime messages remain as host-relevant answers/events after snapshots and state-transfer messages are removed?
5. What is the minimal first batch of query helper APIs we actually need to unblock the refactor?

## Suggested First Implementation Slice

I recommend we do the first real implementation slice in this order:

1. Define the state taxonomy.
2. Design the public `Gs1GameStateView` ABI plus query-helper surface.
3. Add the read-only state API and the first simple coarse state-change messages.
4. Migrate one vertical slice first, likely campaign/regional-map state and app-state-driven scene flow.
5. Migrate one site slice next, likely player meters plus inventory.
6. Remove old snapshot/data-transfer messages and then remove the gameplay UI systems that existed only for them.
7. Repair hosts and tests afterward, one by one.

This reduces risk and gives us a working side-by-side migration path.

## What I Think Is Best

The strongest version of this refactor is:

- gameplay DLL keeps full authority over gameplay simulation
- gameplay DLL exposes a dedicated ABI-safe const state-view object that mainly contains pointers/counts and query hooks
- engine hosts own all UI/session/presentation state
- host-to-gameplay messages remain semantic intent contracts
- internal gameplay messages remain only for semantic gameplay broadcasts and essential lifecycle coordination
- ECS stays private and internal

The main thing I would avoid is trying to expose the current internal `GameState` directly just because it already exists. The right version of the idea is to expose a separate exported POD state-view layer that points into the real authoritative runtime state where safe and falls back to query helpers where direct exposure is not a good fit.

## Planned Work Items

If we proceed with this direction, the implementation plan I would follow is:

1. Audit and classify current state, messages, and projection paths.
2. Draft the new ABI state-view schema and query-helper set.
3. Add the new state-view API plus simple host-facing state-change messages.
4. Introduce stable pointer/count exposure for safe slices and query helpers for ECS/heavy/internal slices.
5. Remove gameplay UI/presentation ownership subsystem by subsystem.
6. Delete obsolete runtime message types and gameplay presentation systems.
7. Trim internal `GameMessage` down to semantic intent/fact broadcast semantics.
8. Repair smoke host, Godot adapter, and tests afterward one by one against the new gameplay-side architecture.
