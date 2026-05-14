# State Resolver Architecture Plan

This document proposes a refactor path from the current direct tagged-state-access model toward a `StateManager`-centered runtime structure where:

- `StateManager` is the only cross-owner read path
- a resolver is simply an `IRuntimeSystem` that owns one or more state sets
- each resolver registers itself for the specific `StateSetId` values it owns
- every state set always has a default resolver installed in `StateManager`
- cross-owner mutation still goes through `GameMessage`

The goal is not to weaken ownership. The goal is to keep strict ownership of mutable state while removing hard wiring between peer systems.

## Implementation Status

This section tracks the current GS1 refactor status relative to this plan.

### What Has Been Done

- Added split authoritative runtime storage for app, campaign, and site state sets instead of relying only on the old aggregate `GameState` shape.
- Added `StateSetId` inventory and typed state-set traits/wrappers in `src/runtime/state_set.h`.
- Added `StateManager` in `src/runtime/state_manager.h` and `src/runtime/state_manager.cpp`.
- Registered runtime systems as owners/resolvers by explicit `StateSetId` ownership instead of implicit aggregate access.
- Enforced the additional cache-layout rule:
  - every non-container state set wrapper is explicitly aligned to 64 bytes
  - compile-time validation checks that contract
- Updated `RuntimeInvocation` and runtime state access so split-backed reads/writes can still hydrate and flush compatibility aggregate views where needed.
- Added split-backed site-world access paths so site systems can work against their owned slices directly instead of always tunneling through assembled `SiteRunState`.
- Migrated the main site owner systems to the split-backed ownership model, including:
  - action execution
  - camp durability
  - craft
  - device maintenance
  - device support
  - device weather contribution
  - ecology
  - economy phone
  - failure recovery
  - inventory
  - local weather resolve
  - modifier
  - placement validation
  - plant weather contribution
  - site completion
  - site flow
  - site time
  - task board
  - weather event
  - worker condition
- Migrated campaign flow away from direct gameplay-system dependency on `RuntimeActiveSiteRunTag` by using split-backed active-site helpers.
- Kept cross-owner mutation on `GameMessage`.
- Updated build wiring so the new state-manager implementation is compiled into the gameplay core.
- Updated repo guidance docs and folder guidelines to describe the split-state runtime direction.

### Current Verified Build State

Verified locally on the current refactor branch/worktree:

- `cmake --build build --config Debug --target gs1_gameplay_core`
- `cmake --build build --config Debug --target gs1_game`

Both builds are green.

### What Is Still Compatibility-Only

The following pieces still exist mainly to support compatibility during the transition:

- `RuntimeActiveSiteRunTag` support in `src/runtime/runtime_state_access.h`
- fallback aggregate-site access path in `src/site/systems/site_system_context.h`
- compatibility hydration/flush paths that assemble aggregate campaign/site views from split state sets

These are no longer the primary gameplay-system path for the migrated systems, but they still exist to avoid breaking older access surfaces during the transition.

### What Is Next

The next refactor slice should focus on reducing transitional duplication and tightening the architecture around the now-working split-state path:

- consolidate duplicated split-state assembly/write-back helpers so campaign/site compatibility hydration does not reimplement the same mapping logic in multiple places
- decide whether the compatibility aggregate-site path should remain temporarily or be removed after downstream callers are fully migrated
- move more read-only runtime helpers and exported view builders to use split state directly where practical instead of assembling aggregate compatibility objects first
- review whether `RuntimeActiveSiteRunTag` can be deleted entirely once no remaining compatibility callers need it
- continue aligning follow-up plans with `COMPILE_TIME_RUNTIME_STATE_PLAN.md`

### TODO List

- deduplicate campaign/site state-set assembly and write-back helper code
- audit remaining compatibility-only access surfaces and remove any no longer needed
- consider replacing broad compatibility `CampaignState` or `SiteRunState` hydration in read-only paths with narrower direct split-state reads
- add focused tests for:
  - duplicate `StateSetId` owner registration rejection
  - split-state fallback/default resolver behavior
  - compatibility hydration/write-back correctness for migrated systems
- review warning policy for the expected MSVC `C4324` alignment warnings caused by intentional `alignas(64)` state-set wrappers
- keep future system additions aligned with:
  - explicit owned-state-set registration
  - `StateManager` cross-owner reads
  - `GameMessage` cross-owner mutation

## Intent

Today the runtime already has the right high-level ownership rule:

- each system owns a specific mutable state slice
- other systems may read some slices
- cross-owner mutation should happen through `GameMessage`

That direction should stay.

The pain point is structural coupling:

- a caller often needs to know that another specific system exists
- fallback behavior is hard to express cleanly when an owner is absent
- feature composition becomes wiring-heavy instead of state-set-driven

The refactor should move to this model:

- introduce a central `StateManager`
- define one resolver slot per owned state set
- keep a full default resolver set inside `StateManager`
- initialize all default resolvers at runtime startup in `StateSetId` order
- allow gameplay systems to register themselves as the active resolver for the state sets they own
- keep `RuntimeInvocation` as transient runtime context only
- let systems read through `StateManager::query(...)`
- let only the owning resolver mutate the state set it owns

## Additional State Layout Rule

For this refactor, every non-container state set must be explicitly aligned to a 64-byte cache line.

Rule:

- if a state set is a non-container state, its storage wrapper must be `alignas(64)`
- the runtime should validate that contract with compile-time checks
- container-backed state sets may still use their natural container alignment, but the state-set wrapper itself may still be aligned when practical

This rule exists so the runtime-owned authoritative storage for scalar or aggregate non-container state sets follows a predictable cache-line layout instead of leaving that to compiler-default packing.

## Core Rules

### 1. `StateManager` is the cross-owner read path

Cross-owner reads go through `StateManager`, not through peer-system interfaces.

Rule:

- non-owners read through `StateManager::query(...)`
- owners write through the owned resolver path
- cross-owner writes still go through `GameMessage`

### 2. A resolver is an owner system

A resolver is not a separate interface category.

A resolver is:

- an `IRuntimeSystem`
- a system that owns one or more state sets
- a system that registers itself in `StateManager` for those owned state sets

If a system owns no state sets, it is not a resolver.

### 3. Every state set has a default resolver

Each `StateSetId` must always have a default resolver installed in `StateManager`.

Default resolvers should represent explicit fallback semantics such as:

- empty capability
- disabled feature
- neutral authored behavior
- unsupported action that safely no-ops

This is what allows plug-in and plug-out behavior without breaking readers.

### 4. Resolver registration is by `StateSetId`

The runtime should not infer resolver ownership from interface inheritance.

Instead:

- the system declares which state sets it owns
- `StateManager` registers that system for those `StateSetId` values
- `StateManager` tracks both the default resolver and the active resolver for each state set

One system may register for multiple state sets if it owns multiple state sets.

### 5. Default tracking belongs to `StateManager`

`IRuntimeSystem` should not expose `is_default_resolver()`.

The runtime already knows which resolver is the default because `StateManager` installs and tracks it by `StateSetId`.

That means:

- default-ness is registration state, not system identity
- `StateManager` decides whether the default resolver or a custom resolver is active for a given state set
- default ticking should iterate state sets in `StateSetId` order

## Current-Refactor-Aligned Examples

Examples that match the current refactor direction better:

- `WeatherEventSystem` may register as the resolver for `StateSetId::RawWeather`
- `LocalWeatherResolveSystem` may register as the resolver for `StateSetId::ResolvedWeather`
- `TechnologySystem` may register as the resolver for the technology-related state sets it owns
- `InventorySystem` may register as the resolver for the inventory-related state sets it owns
- `PlacementValidationSystem` reads state, but if it owns no state set, it is not a resolver

The key point is not that a system implements a resolver interface.

The key point is that a system owns specific state sets and registers itself for those state sets in `StateManager`.

## Proposed Runtime Model

## A. State slices

Keep gameplay state grouped by ownership domain:

- app state
- campaign state
- active site-run state
- site ECS-owned sparse and dense domains
- runtime clocks and transient input snapshots

These remain the authoritative data containers.

## B. `StateManager`

`StateManager` should own:

- the authoritative storage slot for each `StateSetId`
- the default resolver pointer for each `StateSetId`
- the active resolver pointer for each `StateSetId`
- the default resolver storage container

`StateManager` should not infer ownership from polymorphic interface shape. It should track ownership explicitly by state set.

Example shape:

```cpp
class StateManager final
{
public:
    StateManager();

    template <class System>
    void register_resolver(System& system) noexcept;

    void tick_defaults(RuntimeInvocation& invocation);

    template <StateSetId Id>
    [[nodiscard]] const typename state_traits<Id>::type& query() const noexcept;

    template <class System, StateSetId Id>
    [[nodiscard]] typename state_traits<Id>::type& state() noexcept;
};
```

The manager owns:

- one authoritative storage object per state set
- one default resolver per state set
- one active resolver pointer per state set

The manager does not centralize unrestricted gameplay authority.

## C. `RuntimeInvocation`

`RuntimeInvocation` should stay a context carrier for:

- transient tick inputs
- fixed-step timing
- runtime message submission
- game-message submission

It should not become the cross-owner state access API.

All cross-owner reads should go through `StateManager`.

## D. Owner systems

Owner systems keep responsibility for:

- authoritative state mutation
- subscribing to gameplay messages
- fixed-step updates
- lifecycle initialization and teardown
- registering themselves as resolvers for the state sets they own

## E. Default resolvers

Default resolvers should live inside `StateManager` as a complete baseline set.

They should be:

- allocated up front
- stored in `StateSetId` order
- tracked separately from active resolvers
- ticked only when they are still the active resolver for that state set

## Code Example

This example reflects the updated direction:

- `StateManager` is the read path
- systems register by `StateSetId`
- default tracking stays inside `StateManager`
- `IRuntimeSystem` does not expose `is_default_resolver()`

```cpp
class RuntimeInvocation;
class StateManager;

enum class StateSetId : std::uint8_t
{
    RawWeather = 0,
    ResolvedWeather,
    PlacementRules,
    Plants,
    Count
};

using PlantId = std::uint32_t;

struct RawWeatherState final
{
    float base_wind {0.0f};
};

struct ResolvedWeatherState final
{
    float wind {0.0f};
};

struct PlacementRulesState final
{
    float max_allowed_wind {40.0f};
};

struct PlantState final
{
    PlantId id {0};
    float growth {0.0f};
};

template <StateSetId>
struct state_traits;

template <>
struct state_traits<StateSetId::RawWeather>
{
    using type = RawWeatherState;
    using element_type = RawWeatherState;
    static constexpr bool k_is_container = false;
};

template <>
struct state_traits<StateSetId::ResolvedWeather>
{
    using type = ResolvedWeatherState;
    using element_type = ResolvedWeatherState;
    static constexpr bool k_is_container = false;
};

template <>
struct state_traits<StateSetId::PlacementRules>
{
    using type = PlacementRulesState;
    using element_type = PlacementRulesState;
    static constexpr bool k_is_container = false;
};

template <>
struct state_traits<StateSetId::Plants>
{
    using type = std::vector<PlantState>;
    using element_type = PlantState;
    static constexpr bool k_is_container = true;
};

template <StateSetId Id>
consteval bool state_contract_is_valid()
{
    using wrapper_type = typename state_traits<Id>::wrapper_type;
    using state_type = typename state_traits<Id>::type;
    using element_type = typename state_traits<Id>::element_type;

    if constexpr (state_traits<Id>::k_is_container)
    {
        return std::is_trivially_copyable_v<element_type> && std::is_standard_layout_v<element_type>;
    }

    return
        std::is_standard_layout_v<state_type> &&
        alignof(wrapper_type) >= 64U;
}

static_assert(state_contract_is_valid<StateSetId::RawWeather>());
static_assert(state_contract_is_valid<StateSetId::ResolvedWeather>());
static_assert(state_contract_is_valid<StateSetId::PlacementRules>());
static_assert(state_contract_is_valid<StateSetId::Plants>());

virtual void run(RuntimeInvocation& invocation, StateManager& state_manager) = 0;
// `RuntimeInvocation` stays the transient runtime context for timing, input, and message flow.
// `StateManager` is the shared read path and the owner of resolver registration/default tracking.

class DefaultRawWeatherResolver final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::RawWeather};
    void run(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class DefaultResolvedWeatherResolver final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::ResolvedWeather};
    void run(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class DefaultPlacementRulesResolver final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::PlacementRules};
    void run(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class DefaultPlantResolver final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::Plants};
    void run(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class WeatherResolverSystem final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::ResolvedWeather};
    void run(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class PlantResolverSystem final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::Plants};
    void run(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class PlacementValidationSystem final : public IRuntimeSystem
{
public:
    void run(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class StateManager final
{
public:
    StateManager()
    {
        // Rule: one StateSetId maps to exactly one storage object.
        // If a state set is container-backed, keep it to exactly one container.
        state_slots_[index(StateSetId::RawWeather)] = &raw_weather_state_;
        state_slots_[index(StateSetId::ResolvedWeather)] = &resolved_weather_state_;
        state_slots_[index(StateSetId::PlacementRules)] = &placement_rules_state_;
        state_slots_[index(StateSetId::Plants)] = &plant_state_;

        default_resolver_systems_.reserve(static_cast<std::size_t>(StateSetId::Count));
        default_resolver_systems_.resize(static_cast<std::size_t>(StateSetId::Count));

        assign_default_resolver<StateSetId::RawWeather, DefaultRawWeatherResolver>();
        assign_default_resolver<StateSetId::ResolvedWeather, DefaultResolvedWeatherResolver>();
        assign_default_resolver<StateSetId::PlacementRules, DefaultPlacementRulesResolver>();
        assign_default_resolver<StateSetId::Plants, DefaultPlantResolver>();
    }

    template <class System>
    void register_resolver(System& system) noexcept
    {
        for (const StateSetId state_set : System::k_owned_state_sets)
        {
            active_resolver_by_state_set_[index(state_set)] = &system;
        }
    }

    void tick_defaults(RuntimeInvocation& invocation)
    {
        for (std::size_t i = 0; i < static_cast<std::size_t>(StateSetId::Count); ++i)
        {
            IRuntimeSystem* const default_resolver = default_resolver_by_state_set_[i];
            if (default_resolver == nullptr)
            {
                continue;
            }

            if (active_resolver_by_state_set_[i] == default_resolver)
            {
                default_resolver->run(invocation, *this);
            }
        }
    }

    template <StateSetId Id>
    [[nodiscard]] const typename state_traits<Id>::type& query() const noexcept
    {
        return state_ref<Id>();
    }

    template <class System, StateSetId Id>
    [[nodiscard]] typename state_traits<Id>::type& state() noexcept
    {
        static_assert(system_owns_state_set<System, Id>(), "System cannot mutate a state set it does not own.");
        return state_ref_mut<Id>();
    }

    template <StateSetId Id>
    [[nodiscard]] std::size_t query_count() const noexcept
    {
        if constexpr (Id == StateSetId::Plants)
        {
            return plant_state_.size();
        }

        return 1U;
    }

    template <StateSetId Id>
    [[nodiscard]] const typename state_traits<Id>::element_type& query(std::uint32_t index_or_key) const
    {
        static_assert(Id == StateSetId::Plants, "keyed query is only valid for container state sets");
        return plant_state_.at(index_or_key);
    }

    template <class System, StateSetId Id>
    [[nodiscard]] typename state_traits<Id>::element_type& element_state(std::uint32_t index_or_key)
    {
        static_assert(Id == StateSetId::Plants, "keyed query is only valid for container state sets");
        static_assert(system_owns_state_set<System, Id>(), "System cannot mutate a state set it does not own.");
        return plant_state_.at(index_or_key);
    }

private:
    static constexpr std::size_t index(StateSetId state_set) noexcept
    {
        return static_cast<std::size_t>(state_set);
    }

    template <class System, StateSetId Id>
    static consteval bool system_owns_state_set()
    {
        for (const StateSetId owned_state_set : System::k_owned_state_sets)
        {
            if (owned_state_set == Id)
            {
                return true;
            }
        }

        return false;
    }

    template <StateSetId Id, class System, class... Args>
    System& assign_default_resolver(Args&&... args)
    {
        auto system = std::make_unique<System>(std::forward<Args>(args)...);
        IRuntimeSystem* const resolver = system.get();
        default_resolver_systems_[index(Id)] = std::move(system);
        default_resolver_by_state_set_[index(Id)] = resolver;
        active_resolver_by_state_set_[index(Id)] = resolver;
        return *static_cast<System*>(resolver);
    }

    template <StateSetId Id>
    [[nodiscard]] const typename state_traits<Id>::type& state_ref() const noexcept
    {
        return *static_cast<const typename state_traits<Id>::type*>(state_slots_[index(Id)]);
    }

    template <StateSetId Id>
    [[nodiscard]] typename state_traits<Id>::type& state_ref_mut() noexcept
    {
        return *static_cast<typename state_traits<Id>::type*>(state_slots_[index(Id)]);
    }

    RawWeatherState raw_weather_state_ {};
    ResolvedWeatherState resolved_weather_state_ {};
    PlacementRulesState placement_rules_state_ {};
    std::vector<PlantState> plant_state_ {};
    std::array<void*, static_cast<std::size_t>(StateSetId::Count)> state_slots_ {};

    std::array<IRuntimeSystem*, static_cast<std::size_t>(StateSetId::Count)> default_resolver_by_state_set_ {};
    std::array<IRuntimeSystem*, static_cast<std::size_t>(StateSetId::Count)> active_resolver_by_state_set_ {};
    std::vector<std::unique_ptr<IRuntimeSystem>> default_resolver_systems_ {};
};

inline void DefaultRawWeatherResolver::run(RuntimeInvocation&, StateManager& state_manager)
{
    state_manager.state<DefaultRawWeatherResolver, StateSetId::RawWeather>().base_wind = 0.0f;
}

inline void DefaultResolvedWeatherResolver::run(RuntimeInvocation&, StateManager& state_manager)
{
    const RawWeatherState& raw_weather = state_manager.query<StateSetId::RawWeather>();
    state_manager.state<DefaultResolvedWeatherResolver, StateSetId::ResolvedWeather>().wind = raw_weather.base_wind;
}

inline void DefaultPlacementRulesResolver::run(RuntimeInvocation&, StateManager& state_manager)
{
    state_manager.state<DefaultPlacementRulesResolver, StateSetId::PlacementRules>().max_allowed_wind = 40.0f;
}

inline void DefaultPlantResolver::run(RuntimeInvocation&, StateManager&)
{
}

inline void WeatherResolverSystem::run(RuntimeInvocation&, StateManager& state_manager)
{
    const RawWeatherState& raw_weather = state_manager.query<StateSetId::RawWeather>();
    state_manager.state<WeatherResolverSystem, StateSetId::ResolvedWeather>().wind = raw_weather.base_wind;
}

inline void PlantResolverSystem::run(RuntimeInvocation&, StateManager& state_manager)
{
    const ResolvedWeatherState& weather = state_manager.query<StateSetId::ResolvedWeather>();

    for (std::size_t i = 0; i < state_manager.query_count<StateSetId::Plants>(); ++i)
    {
        PlantState& plant =
            state_manager.element_state<PlantResolverSystem, StateSetId::Plants>(static_cast<std::uint32_t>(i));
        plant.growth += weather.wind < 20.0f ? 1.0f : 0.25f;
    }
}

inline void PlacementValidationSystem::run(RuntimeInvocation&, StateManager& state_manager)
{
    const ResolvedWeatherState& weather = state_manager.query<StateSetId::ResolvedWeather>();
    const PlacementRulesState& rules = state_manager.query<StateSetId::PlacementRules>();
    const bool can_place = weather.wind <= rules.max_allowed_wind;
    (void)can_place;
}

class GameRuntime
{
public:
    GameRuntime()
    {
        state_manager_.register_resolver(weather_system_);
        state_manager_.register_resolver(plant_system_);

        fixed_step_systems_.push_back(&weather_system_);
        fixed_step_systems_.push_back(&plant_system_);
        fixed_step_systems_.push_back(&placement_reader_);
    }

    void tick()
    {
        state_manager_.tick_defaults(runtime_invocation_);

        for (IRuntimeSystem* system : fixed_step_systems_)
        {
            system->run(runtime_invocation_, state_manager_);
        }
    }

private:
    RuntimeInvocation runtime_invocation_ {};
    StateManager state_manager_ {};
    WeatherResolverSystem weather_system_ {};
    PlantResolverSystem plant_system_ {};
    PlacementValidationSystem placement_reader_ {};
    std::vector<IRuntimeSystem*> fixed_step_systems_ {};
};
```

The important pattern here is:

- `StateManager` owns the default resolver set and the active resolver mapping
- default resolvers are stored and indexed in `StateSetId` order
- `StateManager` tracks which resolver is the default for each state set
- only one resolver may own a state set at a time
- `RuntimeInvocation` no longer acts as the cross-owner state-access surface
- internal readers use `StateManager::query<StateSetId::X>()`
- mutation stays inside the owning resolver through `StateManager::state<System, StateSetId::X>()`
- a non-owner system such as `PlacementValidationSystem` is not a resolver
- container-backed state sets still map to exactly one container

## Container Example

When a state set needs a container, keep that state set to exactly one container.

```cpp
struct PlantState final
{
    float growth {0.0f};
};

class PlantResolverSystem final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::Plants};

    void run(RuntimeInvocation&, StateManager& state_manager) override
    {
        for (std::size_t i = 0; i < state_manager.query_count<StateSetId::Plants>(); ++i)
        {
            PlantState& plant =
                state_manager.element_state<PlantResolverSystem, StateSetId::Plants>(static_cast<std::uint32_t>(i));
            plant.growth += 1.0f;
        }
    }
};

class PlantReaderSystem final : public IRuntimeSystem
{
public:
    void run(RuntimeInvocation&, StateManager& state_manager) override
    {
        const PlantState& plant = state_manager.query<StateSetId::Plants>(0);
        const float growth = plant.growth;
        (void)growth;
    }
};
```

This pattern means:

- one state set maps to one container
- query-by-index has one meaning
- the collection can grow and shrink without exposing multiple storage paths

## Adapter Example

The exported adapter API should keep the ABI surface trivial and query the real runtime state directly.

```cpp
extern "C" const void* gs1_query_state_view(
    Gs1RuntimeHandle* runtime,
    Gs1StateSetId state_set_id) GS1_NOEXCEPT
{
    if (runtime == nullptr)
    {
        return nullptr;
    }

    return runtime->runtime->query_state_view(state_set_id);
}

template <Gs1StateSetId StateSet>
[[nodiscard]] const auto* adapter_query_state(Gs1RuntimeHandle* runtime) noexcept
{
    using view_type = typename state_traits<StateSet>::type;
    return static_cast<const view_type*>(gs1_query_state_view(runtime, StateSet));
}
```

That keeps the exported structs trivial and lets the adapter read authoritative gameplay state without a cached query layer.

## Resolver Definition

A resolver is defined by ownership, not by interface type.

Definition:

- if an `IRuntimeSystem` owns one or more state sets and registers for them in `StateManager`, it is a resolver
- if an `IRuntimeSystem` owns no state sets, it is not a resolver

That means:

- there is no separate resolver class hierarchy requirement
- there is no resolver-category split
- there is no request-resolver subtype

The owning system itself is the resolver for the state sets it owns.

## Compatibility With Current GS1 Rules

This design fits the current repo rules if we preserve the following:

- systems still own their state slices
- direct system-to-system mutation stays forbidden
- `GameMessage` remains the cross-owner mutation path
- ECS ownership stays private to the owner system
- read access goes through `StateManager`

That makes this proposal an extension of the current ownership model, not a rejection of it.

## Benefits

### Plug-friendly composition

A runtime can install:

- default resolver only
- custom resolver system for one state set
- one custom resolver system that owns multiple state sets

as long as every state set still has a default resolver.

### Reduced structural coupling

A caller depends on:

- `StateManager::query<StateSetId::X>()`

instead of:

- direct knowledge of peer-owned mutation paths
- concrete peer-system wiring for read access

### Better ownership visibility

Ownership stays visible because:

- each state set has one active resolver
- resolver registration is explicit
- a non-owner does not become a resolver accidentally

## Risks

### 1. Hidden dependency sprawl

If systems read many state sets ad hoc, dependencies become less visible.

Mitigation:

- require each system to declare its consumed state sets beside its owned state sets
- keep dependency declarations near message subscriptions

### 2. Resolver bloat

If one system starts claiming too many unrelated state sets, ownership boundaries blur.

Mitigation:

- keep ownership grouped by real gameplay domain
- keep state-set ownership explicit and reviewable

### 3. Write-path leakage

If `StateManager::state(...)` becomes broadly callable, ownership collapses.

Mitigation:

- keep mutation on owner-only APIs
- keep cross-owner mutation on `GameMessage`

### 4. Registration bugs

If resolver registration does not match actual ownership, the wrong system may become active for a state set.

Mitigation:

- validate ownership declarations
- validate one active owner per state set
- add runtime assertions for duplicate resolver registration

## Migration Plan

### Phase 0. Ownership inventory

Document:

- current owned state slices
- candidate `StateSetId` values
- read-only peer access points
- cross-owner write paths

### Phase 1. `StateManager` scaffolding

Add:

- `StateSetId` inventory
- per-state-set storage slots
- default resolver storage ordered by `StateSetId`
- default resolver tracking
- active resolver tracking

Do not change gameplay behavior yet.

### Phase 2. Ownership declarations

Add per-system ownership declarations such as:

- owned state sets
- consumed state sets
- subscribed game messages
- subscribed host messages

This preserves architectural visibility.

### Phase 3. Resolver registration

Let owner systems register themselves for the state sets they own.

Examples:

- `WeatherEventSystem`
- `LocalWeatherResolveSystem`
- `TechnologySystem`
- `InventorySystem`

### Phase 4. Reader migration

Replace direct peer reads with `StateManager::query(...)` in selected systems.

Best early targets:

- weather-derived reads
- technology eligibility reads
- inventory availability reads

### Phase 5. Owner-only mutation enforcement

Tighten write access so that:

- only the owner resolver can mutate its state set
- non-owners cannot call the mutation path
- cross-owner change requests still use `GameMessage`

### Phase 6. Tests and validation

Add:

- duplicate-owner assertions
- default-resolver fallback tests
- custom-resolver override tests
- focused regressions for migrated read paths

## Recommended First Implementation Slice

The best first slice is weather, because:

- ownership is clear
- multiple systems only need read access
- default fallback behavior is easy to define

Recommended order:

1. add `StateManager` scaffolding
2. install default weather resolvers by `StateSetId`
3. register the real weather owner for the weather state sets it owns
4. migrate one dependent reader to `StateManager::query(...)`
5. add a focused fallback-versus-override test

## Resolved Design Choices

These points are no longer open for this refactor direction.

### 1. Granularity

Choose the second direction: group behavior by gameplay/domain capability rather than forcing one isolated resolver type per tiny storage detail.

In this refactor, that still means registration happens by `StateSetId`, but one resolver system may own and register multiple related state sets when that matches the real ownership boundary.

### 2. Default behavior strictness

Use the previously recommended behavior:

- safe neutral runtime behavior when a custom owner is absent
- debug-visible logging for unexpectedly missing non-optional capabilities

This keeps plug-out behavior safe without hiding mistakes during development.

### 3. Implementation shape

Follow the code example in this document.

That means:

- a resolver remains a normal `IRuntimeSystem`
- `StateManager` stores and tracks the default resolvers
- registration happens by owned `StateSetId`
- there is no separate resolver-interface hierarchy and no alternate resolver base model

### 4. Runtime context and access model

A resolver is also a system, so it should behave like one.

That means:

- it runs through the normal runtime/system flow
- it reads through `StateManager`
- it mutates only the state sets it owns
- cross-owner mutation still goes through `GameMessage`

## Final Recommendation

The right framing for this refactor is:

- central `StateManager`
- default resolvers installed and tracked by `StateSetId`
- owner systems register themselves as resolvers for the state sets they own
- cross-owner reads go through `StateManager`
- cross-owner writes stay on `GameMessage`

The main thing to avoid is treating resolver-ness as interface inheritance or service-type taxonomy. In this design, resolver-ness comes from state ownership plus registration, and `StateManager` remains the source of truth for both default tracking and cross-owner reads.
