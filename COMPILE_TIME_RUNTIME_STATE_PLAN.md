# Compile-Time Runtime State Plan

## Feasibility

Yes, this requirement is implementable.

The clean version is:

1. Define each game's runtime at compile time from a `GameDefinition`.
2. Derive the runtime-owned state pack from the enabled system list.
3. Derive system access permissions from compile-time access lists.
4. Keep the exported adapter view as a separate ABI-safe projection layer built from that typed state pack.

This fits the repo's current direction because `src/runtime/runtime_state_access.h` already has:

- tag-based state access
- owner/read distinctions
- compile-time permission checks

The main change is to replace the fixed `GameState` aggregate with a generated state pack and make system registration/view building come from the same game definition.

## Core Goal

Move from:

- one fixed `GameState` for all games

To:

- one compile-time `GameDefinition`
- one generated runtime state pack per game
- one compile-time validated system pack per game
- one game-specific exported state-view builder per game

## Recommended Architecture

### 1. Add a compile-time game definition

Define a game by types, not runtime data.

Example shape:

```cpp
template <class... Systems>
struct system_pack
{
};

template <class SystemPack, class ViewPolicy>
struct game_definition
{
    using systems = SystemPack;
    using view_policy = ViewPolicy;
};
```

For GS1:

```cpp
using Gs1Definition = game_definition<
    system_pack<
        CampaignFlowSystem,
        CampaignTimeSystem,
        TechnologySystem,
        SiteTimeSystem,
        WeatherEventSystem
        // ...
    >,
    Gs1ViewPolicy>;
```

This becomes the single compile-time source of truth for:

- enabled systems
- owned state slices
- allowed cross-system reads
- exported adapter view

### 2. Replace fixed `GameState` with a generated state pack

Do not make the top-level runtime state fully dynamic.

Use a compile-time state map:

```cpp
template <class Tag, class State>
struct state_slot
{
    State value {};
};

template <class... Slots>
struct state_pack : Slots...
{
};
```

Then derive the pack from enabled systems:

```cpp
template <class System>
struct owned_state;

template <class Definition>
using generated_state_pack = /* unique state_slot<Tag, State>... */;
```

Recommended rule:

- each state slice has one tag
- each tag has one concrete state type
- each tag has one owning system

Examples of slices:

- `AppStateTag -> AppRuntimeState`
- `CampaignTag -> CampaignState`
- `ActiveSiteTag -> SiteRunState`
- `MessageQueueTag -> GameMessageQueue`
- `RuntimeMessageQueueTag -> std::deque<Gs1RuntimeMessage>`
- `FixedStepConfigTag -> FixedStepConfigState`

This keeps the state layout fixed per game build while letting different games assemble different combinations.

### 3. Keep dynamic containers inside owned state slices

Your outer runtime state should be compile-time fixed.
Your inner containers can still be dynamic where game data is naturally variable-sized.

Recommended defaults:

- ordered dense collections: `std::vector`
- stable-front/back queues: `std::deque`
- optional singletons: `std::optional`
- keyed lookup with mostly read-heavy access: flat hash map if the repo adopts one, otherwise `std::unordered_map`
- sparse ECS-owned object sets: keep using ECS entities/components instead of duplicating them into custom maps

Suggestion:

- avoid a general-purpose `std::any` or type-erased runtime bag for owned gameplay state
- use dynamic containers only inside specific state slices where variable sizing is a real gameplay need

That gives you compile-time composition without giving up practical runtime storage.

### 4. Make access rules explicit and compile-time validated

Your current `system_state_tags<System>` mechanism is the right direction, but it should be split into clearer contracts:

```cpp
template <class System>
struct owned_state_tags;

template <class System>
struct readable_state_tags;

template <class System>
struct writable_state_tags;
```

Recommended rules:

- a system may write only tags it owns
- a system may read tags listed in `readable_state_tags`
- ownership must be unique across the game definition
- every readable/writable tag must exist in the generated state pack
- every enabled system must have a complete access declaration

This lets the compiler reject:

- missing state declarations
- duplicate owners
- invalid readers
- systems enabled for a game without required state

### 5. Support specialized code by access-profile type

Your requirement for combination-based specialization is also implementable.

Use an access profile type:

```cpp
template <class OwnedList, class ReadList>
struct access_profile
{
};

template <class System, class Profile>
struct system_logic;
```

Then each concrete system binds to its profile:

```cpp
using InventoryAccess = access_profile<
    type_list<InventoryTag>,
    type_list<CampaignTag, RulesTag>>;

template <>
struct system_logic<InventorySystem, InventoryAccess>
{
    static void run(auto& access);
};
```

And dispatch through:

```cpp
template <class System>
void run_system(RuntimeInvocation& invocation)
{
    using profile = typename resolved_access_profile<System>::type;
    system_logic<System, profile>::run(make_game_state_access<System>(invocation));
}
```

If there is no specialization, compilation fails.

Recommended improvement:

- prefer one specialization key per system profile rather than specializing on arbitrary subsets all over the codebase
- keep unsupported combinations impossible by construction when practical, instead of relying only on a late compile failure

### 6. Generate system registry from the same definition

Today `GameRuntime` owns a vector of `IRuntimeSystem`.
That can still work, but creation should come from the game definition.

Recommended direction:

```cpp
template <class Definition>
struct runtime_registry_builder;
```

This builder should:

- instantiate only enabled systems
- register message subscriptions only for enabled systems
- build fixed-step order only for enabled systems
- validate that each enabled system's dependencies are present

Important note:

- you do not need to remove runtime polymorphism immediately
- compile-time composition for state and registration can coexist with `IRuntimeSystem` as a migration step

### 7. Keep exported state views as a projection layer

This is the most important design constraint.

Do not expose the templated internal state pack directly across the DLL boundary.

Keep:

- internal runtime state as typed C++ templates
- exported state view as trivial ABI-safe structs

For GS1, the current `Gs1GameStateView` pattern is still valid, but the builder should become game-definition-driven:

```cpp
template <class Definition>
struct exported_view_builder;
```

For a game-specific adapter build, this works well because:

- the game's enabled systems are fixed at compile time
- the adapter is built against the same definition
- the adapter can decode the exported view shape for that game

Recommended rule:

- the view schema is game-specific but ABI-stable within that game build line

That means:

- `GS1` can keep `Gs1GameStateView`
- another game should likely get its own public view header instead of pretending one shared root schema fits all future games

### 8. Consider splitting gameplay state from runtime service state

While refactoring, separate:

- gameplay-authoritative state
- runtime service state

Gameplay-authoritative examples:

- campaign progression
- site run
- world state

Runtime service examples:

- fixed-step config
- runtime message queue
- host input snapshots
- profiling accumulators

This keeps system ownership cleaner and makes future cross-game reuse easier.

## Suggested Type-Level Model

Recommended metadata traits:

```cpp
template <class Tag>
struct state_traits
{
    using value_type = void;
};

template <class Tag>
struct state_owner
{
    using type = void;
};

template <class System>
struct system_access;

template <class System>
struct system_dependencies;
```

Where `system_access<System>` might expose:

```cpp
using owns = type_list</* ... */>;
using reads = type_list</* ... */>;
```

Then add compile-time validators:

- `validate_unique_state_owners<Definition>()`
- `validate_declared_states_exist<Definition>()`
- `validate_system_dependencies<Definition>()`
- `validate_access_lists<Definition>()`
- `validate_view_builder_inputs<Definition>()`

## Migration Strategy

### Phase 1. Formalize tags and traits

Goals:

- keep current runtime behavior
- introduce canonical tag/type/owner traits

Tasks:

- replace ad-hoc tag access declarations with uniform traits
- define one state tag per runtime slice
- define `state_traits<Tag>::value_type`
- define `state_owner<Tag>::type`

### Phase 2. Introduce generated state pack

Goals:

- keep current GS1 feature set
- remove hard dependency on one hand-written `GameState`

Tasks:

- add `state_slot` and `state_pack`
- add compile-time accessors by tag
- create a GS1-generated pack matching the current state layout
- adapt `RuntimeInvocation` to use tag lookups instead of hard-coded members

### Phase 3. Move to game definition driven registry

Goals:

- register systems from one compile-time definition

Tasks:

- add `Gs1Definition`
- build enabled systems from the definition
- validate dependencies and owners at compile time
- preserve current subscription/runtime ordering behavior

### Phase 4. Add access-profile specialization path

Goals:

- support system logic chosen by compile-time access combinations

Tasks:

- add canonical `access_profile`
- route system execution through profile resolution
- add intentionally unsupported-profile compile failures

### Phase 5. Move view building behind game definition

Goals:

- keep Godot/smoke host integration working
- detach view building from one fixed runtime struct

Tasks:

- add game-specific exported view builder trait
- make GS1 view building consume tagged state access
- validate required slices for the GS1 adapter view

### Phase 6. Add regression coverage

Goals:

- make architecture changes safe

Tests to add:

- compile-time validation tests for duplicate state owners
- compile-time validation tests for undeclared access tags
- compile-time validation tests for missing system specializations
- runtime regression tests proving GS1 exported views still match expected behavior
- Godot adapter build verification after the view boundary changes

## Recommended Constraints

To keep this design from becoming too clever:

1. Keep state ownership at the slice level, not per-field.
2. Keep system access declarative and centralized.
3. Do not make the exported ABI depend on template layout details.
4. Avoid runtime type erasure for core gameplay state.
5. Validate the game definition aggressively at compile time.
6. Let systems share reads freely only when the dependency is intentional and declared.
7. Keep ECS-heavy site/world state inside its owned slice instead of spreading ECS references across unrelated slices.

## Risks

### Risk 1. Template complexity explosion

If every system invents custom metaprogramming patterns, the architecture will become hard to debug.

Mitigation:

- standardize a small set of traits
- centralize validators
- keep system declarations boring

### Risk 2. ABI confusion

If internal templated state is treated as the exported DLL contract, cross-module compatibility will become fragile.

Mitigation:

- keep a strict projection boundary
- version exported game views explicitly

### Risk 3. Over-generalizing too early

If the framework tries to solve every future game shape up front, the migration may stall.

Mitigation:

- first make GS1 definition-driven
- extract only the abstractions that survive that real migration cleanly

### Risk 4. Access-profile combinatorics

If systems are specialized for too many access combinations, maintenance cost rises quickly.

Mitigation:

- keep only a few intentional profiles
- prefer dependency validation over unlimited specialization

## Recommendation

Proceed with this refactor, but in this order:

1. Formalize state tags, owners, and access traits.
2. Replace the fixed aggregate with a generated tagged state pack.
3. Generate system registration from a game definition.
4. Keep adapter/exported views as a separate ABI-safe projection layer.
5. Add access-profile specialization only where it provides clear value.

## What "Perfect" Looks Like

For this repo, the best end state is not a fully dynamic universal engine-state container.

The best end state is:

- compile-time game definition
- compile-time generated owned state pack
- compile-time validated system permissions
- game-specific exported view projection
- minimal runtime type erasure
- clear DLL ABI boundary

That gives you reuse across games without losing performance, ownership clarity, or adapter stability.
