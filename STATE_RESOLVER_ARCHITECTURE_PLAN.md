# State Resolver Architecture Plan

This document proposes a refactor path from the current direct tagged-state-access model toward a resolver-driven, plug-friendly runtime structure.

The goal is not to weaken ownership. The goal is to keep strict ownership of mutable state while replacing hard knowledge of peer systems with stable state-service contracts.

## Intent

Today the runtime already has a strong ownership rule:

- each system owns a specific mutable state slice
- other systems may read some slices
- cross-owner mutation should happen through `GameMessage`

That direction is good and should stay.

The current pain point is coupling:

- a system often needs to know that another specific state slice exists
- a game feature may feel like it needs a dependency tree of systems instead of one clean capability
- removing one system can force structural rewiring instead of graceful fallback

Your proposed direction is a good fit if we shape it carefully, and the refactor should follow this pattern:

- introduce a central state resolver manager
- define one state-set contract per owned state slice
- keep a full default resolver set inside `StateManager`
- initialize all default resolvers at game-runtime startup
- allow a gameplay system to override the default resolver for any state set it owns by implementing and registering that contract
- let `RuntimeInvocation` stay as runtime context only
- let systems read through `StateManager::query(...)` and write only through their owned resolver path

## Core Recommendation

The strongest version of the idea is:

- use a central `StateManager`
- keep the default resolver set inside that manager
- keep actual mutable state ownership inside the owning systems or owning runtime slices
- use `query(...)` for read access and resolver ownership for mutation
- keep cross-owner writes on `GameMessage` or owner-handled request flow

In short:

- centralize lookup
- centralize fallback behavior
- do not centralize unrestricted gameplay authority

That keeps the current ownership model intact while removing most compile-time and wiring-time coupling.

## Why This Is Better Than A Plain State Manager

If the new manager directly owns all state and gives everyone broad mutable access, it will create a larger coupling problem than the current one:

- dependencies become implicit
- ownership becomes blurry
- testing gets harder because every system can reach everything
- plug-in and plug-out becomes dangerous because behavior becomes hidden in global access

If the manager is instead only the registry and dispatch point for state-capability resolvers:

- systems can depend on capabilities instead of concrete peers
- missing systems can fall back to safe default behavior
- ownership stays explicit
- tests can swap resolvers cheaply
- feature modules become more optional

## Design Principles

### 1. Resolver interfaces are capability contracts

Each resolver interface should describe a stable gameplay capability, not a concrete subsystem.

Good examples:

- `IWeatherStateResolver`
- `ITechnologyStateResolver`
- `IInventoryQueryResolver`
- `IPlacementRulesResolver`
- `ITaskBoardQueryResolver`

Bad examples:

- `ILocalWeatherSystemBridge`
- `IFactionReputationSystemAccess`
- `IUseTaskBoardSystemInternals`

The interface should express what a caller needs, not who currently implements it.

### 2. Mutable ownership stays local

Only resolver systems may mutate a state set. Everyone else reads through `StateManager::query(...)`.

Resolvers may expose:

- read-only state views
- derived query answers
- capability checks
- request submission entry points that translate into `GameMessage`

Resolvers should not become backdoors for direct writes into another owner's state.

Rule:

- cross-owner reads may go through resolvers
- cross-owner writes should still go through messages, commands, or owner-handled requests

### 3. Every state set has a default resolver

Each state set should always have a registered default resolver inside `StateManager`.

Default resolvers should be explicit about which mode they represent:

- empty capability
- disabled feature
- unsupported action
- fallback authored/static behavior

Examples:

- no weather system installed -> default weather resolver returns neutral weather
- no technology system installed -> default technology resolver answers "nothing unlocked"
- no task board system installed -> default task queries return empty lists

This is what enables true plug-in and plug-out behavior.

### 4. System implementations may override defaults

If a system owns the authoritative state for a state set, it may implement that resolver contract and register itself as the active resolver.

Examples:

- `WeatherEventSystem` may implement `IWeatherStateResolver`
- `TechnologySystem` may implement `ITechnologyStateResolver`
- `InventorySystem` may implement `IInventoryQueryResolver`

The system then becomes the provider for that capability without callers knowing about the concrete system type.

### 5. Resolver lookup should be explicit and typed

Avoid stringly-typed service lookup.

Prefer compile-time typed registration such as:

- state-set enum
- interface type
- single active provider pointer/reference per state set
- guaranteed default fallback

That fits the codebase’s current tagged-state-access direction and keeps overhead low.

## Proposed Runtime Model

## A. State slices

Keep gameplay state grouped by ownership domain:

- app state
- campaign state
- active site-run state
- site ECS-owned sparse/dense domains
- runtime clocks and transient input snapshots

These remain the authoritative data containers.

## B. Resolver registry

Add a runtime-owned manager that stores:

- the full default resolver set
- the currently active custom owner for each state set
- read permission state for systems

Example shape:

```cpp
template <class ResolverTag>
struct resolver_traits;

class StateManager final
{
public:
    void initialize_defaults();
    void tick_defaults();

    template <class ResolverTag>
    const typename resolver_traits<ResolverTag>::interface_type& get() const noexcept;

    template <class ResolverTag>
    void register_override(const typename resolver_traits<ResolverTag>::interface_type& resolver) noexcept;

    template <class ResolverTag>
    void clear_override() noexcept;
};
```

The manager owns:

- a full default resolver set for every state set
- optional override pointer/reference per capability

The manager does not own gameplay state itself.

## C. Resolver context

`RuntimeInvocation` should stay as a context carrier for transient tick inputs, runtime timing, and message submission helpers.

It should not be the place where state access lives.

All state access should go through `StateManager`.

## D. Owner systems

Owner systems keep responsibility for:

- authoritative state mutation
- subscribing to gameplay messages
- fixed-step updates
- lifecycle initialization and teardown

If a system implements one or more resolver interfaces, it also becomes the active provider for those capabilities during registration.

## E. Default resolvers

Default resolvers should live inside `StateManager` as a complete baseline set.

They should be:

- initialized at game-runtime startup
- present for every state set
- tickable from `StateManager`
- safe when the custom resolver for that state set is absent
- skipped from default ticking when a custom resolver has replaced that state set

## Code Example

This is the pattern the refactor should follow.

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
    using state_type = typename state_traits<Id>::type;
    using element_type = typename state_traits<Id>::element_type;

    if constexpr (state_traits<Id>::k_is_container)
    {
        return std::is_trivially_copyable_v<element_type> && std::is_standard_layout_v<element_type>;
    }

    return std::is_trivially_copyable_v<state_type> && std::is_standard_layout_v<state_type>;
}

static_assert(state_contract_is_valid<StateSetId::RawWeather>());
static_assert(state_contract_is_valid<StateSetId::ResolvedWeather>());
static_assert(state_contract_is_valid<StateSetId::PlacementRules>());
static_assert(state_contract_is_valid<StateSetId::Plants>());

class IRuntimeSystem
{
public:
    virtual ~IRuntimeSystem() = default;
    [[nodiscard]] virtual const char* name() const noexcept = 0;
    [[nodiscard]] virtual bool is_default_resolver() const noexcept = 0;
    virtual void tick(RuntimeInvocation& invocation, StateManager& state_manager) = 0;
};

class DefaultRawWeatherResolver final : public IRuntimeSystem
{
public:
    [[nodiscard]] const char* name() const noexcept override { return "DefaultRawWeatherResolver"; }
    [[nodiscard]] bool is_default_resolver() const noexcept override { return true; }
    void tick(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class DefaultResolvedWeatherResolver final : public IRuntimeSystem
{
public:
    [[nodiscard]] const char* name() const noexcept override { return "DefaultResolvedWeatherResolver"; }
    [[nodiscard]] bool is_default_resolver() const noexcept override { return true; }
    void tick(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class DefaultPlacementRulesResolver final : public IRuntimeSystem
{
public:
    [[nodiscard]] const char* name() const noexcept override { return "DefaultPlacementRulesResolver"; }
    [[nodiscard]] bool is_default_resolver() const noexcept override { return true; }
    void tick(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class DefaultPlantResolver final : public IRuntimeSystem
{
public:
    [[nodiscard]] const char* name() const noexcept override { return "DefaultPlantResolver"; }
    [[nodiscard]] bool is_default_resolver() const noexcept override { return true; }
    void tick(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class WeatherResolverSystem final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 1> k_write_states {StateSetId::ResolvedWeather};

    [[nodiscard]] const char* name() const noexcept override { return "WeatherResolverSystem"; }
    [[nodiscard]] bool is_default_resolver() const noexcept override { return false; }
    void tick(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class PlantResolverSystem final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 1> k_write_states {StateSetId::Plants};

    [[nodiscard]] const char* name() const noexcept override { return "PlantResolverSystem"; }
    [[nodiscard]] bool is_default_resolver() const noexcept override { return false; }
    void tick(RuntimeInvocation& invocation, StateManager& state_manager) override;
};

class PlacementValidationSystem final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 0> k_write_states {};

    [[nodiscard]] const char* name() const noexcept override { return "PlacementValidationSystem"; }
    [[nodiscard]] bool is_default_resolver() const noexcept override { return false; }
    void tick(RuntimeInvocation& invocation, StateManager& state_manager) override;
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
        emplace_default_system<DefaultRawWeatherResolver>();
        emplace_default_system<DefaultResolvedWeatherResolver>();
        emplace_default_system<DefaultPlacementRulesResolver>();
        emplace_default_system<DefaultPlantResolver>();
    }

    void register_system(IRuntimeSystem& system)
    {
        resolver_systems_.push_back(&system);
    }

    template <class System>
    void register_resolver(System& system)
    {
        for (const StateSetId state_set : System::k_write_states)
        {
            resolver_owner_[index(state_set)] = &system;
        }
    }

    void tick_defaults(RuntimeInvocation& invocation)
    {
        for (IRuntimeSystem* resolver : resolver_systems_)
        {
            if (resolver->is_default_resolver())
            {
                resolver->tick(invocation, *this);
            }
        }
    }

    template <StateSetId Id>
    [[nodiscard]] const typename state_traits<Id>::type& query() const noexcept
    {
        return state_ref<Id>();
    }

    template <StateSetId Id>
    [[nodiscard]] typename state_traits<Id>::type& state() noexcept
    {
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
    [[nodiscard]] const typename state_traits<Id>::type& query(std::uint32_t index_or_key) const
    {
        static_assert(Id == StateSetId::Plants, "keyed query is only valid for container state sets");
        return plant_state_.at(index_or_key);
    }

    template <StateSetId Id>
    [[nodiscard]] typename state_traits<Id>::type& query(std::uint32_t index_or_key)
    {
        static_assert(Id == StateSetId::Plants, "keyed query is only valid for container state sets");
        return plant_state_.at(index_or_key);
    }

private:
    static constexpr std::size_t index(StateSetId state_set) noexcept
    {
        return static_cast<std::size_t>(state_set);
    }

    template <class System, class... Args>
    System& emplace_default_system(Args&&... args)
    {
        auto system = std::make_unique<System>(std::forward<Args>(args)...);
        System& ref = *system;
        resolver_systems_.push_back(&ref);
        default_systems_.push_back(std::move(system));
        return ref;
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

    std::array<IRuntimeSystem*, static_cast<std::size_t>(StateSetId::Count)> resolver_owner_ {};
    std::vector<std::unique_ptr<IRuntimeSystem>> default_systems_ {};
    std::vector<IRuntimeSystem*> resolver_systems_ {};
};

inline void DefaultRawWeatherResolver::tick(RuntimeInvocation&, StateManager& state_manager)
{
    state_manager.state<StateSetId::RawWeather>().base_wind = 0.0f;
}

inline void DefaultResolvedWeatherResolver::tick(RuntimeInvocation&, StateManager& state_manager)
{
    state_manager.state<StateSetId::ResolvedWeather>().wind = 0.0f;
}

inline void DefaultPlacementRulesResolver::tick(RuntimeInvocation&, StateManager& state_manager)
{
    state_manager.state<StateSetId::PlacementRules>().max_allowed_wind = 40.0f;
}

inline void DefaultPlantResolver::tick(RuntimeInvocation&, StateManager&)
{
}

inline void WeatherResolverSystem::tick(RuntimeInvocation&, StateManager& state_manager)
{
    const RawWeatherState& raw_weather = state_manager.query<StateSetId::RawWeather>();
    state_manager.state<StateSetId::ResolvedWeather>().wind = raw_weather.base_wind;
}

inline void PlantResolverSystem::tick(RuntimeInvocation&, StateManager& state_manager)
{
    const ResolvedWeatherState& weather = state_manager.query<StateSetId::ResolvedWeather>();
    std::vector<PlantState>& plants = state_manager.state<StateSetId::Plants>();
    for (std::size_t i = 0; i < state_manager.query_count<StateSetId::Plants>(); ++i)
    {
        plants.at(i).growth += weather.wind < 20.0f ? 1.0f : 0.25f;
    }
}

inline void PlacementValidationSystem::tick(RuntimeInvocation&, StateManager& state_manager)
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
        auto& weather_system = emplace_system<WeatherResolverSystem>();
        auto& plant_system = emplace_system<PlantResolverSystem>();
        auto& placement_reader = emplace_system<PlacementValidationSystem>();

        state_manager_.register_resolver(weather_system);
        state_manager_.register_resolver(plant_system);

        fixed_step_systems_.push_back(&weather_system);
        fixed_step_systems_.push_back(&plant_system);
        fixed_step_systems_.push_back(&placement_reader);
    }

    void tick()
    {
        state_manager_.tick_defaults(runtime_invocation_);
        for (IRuntimeSystem* system : fixed_step_systems_)
        {
            system->tick(runtime_invocation_, state_manager_);
        }
    }

private:
    template <class System, class... Args>
    System& emplace_system(Args&&... args)
    {
        auto system = std::make_unique<System>(std::forward<Args>(args)...);
        System& ref = *system;
        state_manager_.register_system(ref);
        systems_.push_back(std::move(system));
        return ref;
    }

    RuntimeInvocation runtime_invocation_ {};
    StateManager state_manager_ {};
    std::vector<std::unique_ptr<IRuntimeSystem>> systems_ {};
    std::vector<IRuntimeSystem*> fixed_step_systems_ {};
};
```

The important pattern here is:

- `StateManager` owns the default resolver set and the state-set access gates
- `StateManager` initializes the default resolver set at startup
- only one resolver may own a state set at a time
- `StateManager::tick_defaults()` iterates resolver systems and ticks only the default ones
- `RuntimeInvocation` no longer owns global game state access
- runtime systems are owned in `GameRuntime::systems_`, matching the current repo pattern
- `GameRuntime` creates only the custom systems it actually uses
- `StateManager` creates and owns the default resolver systems internally
- `StateManager` stores state as indexed opaque slots, with typed access resolved through `state_traits`
- each state set has exactly one authoritative storage object
- container state sets have exactly one container, so `query(index)` is never ambiguous
- query-by-index only applies to the one container owned by that state set
- internal systems and controllers read through `StateManager::query<StateSetId::X>()`
- mutation stays inside resolver systems through `StateManager::state<StateSetId::X>()`
- compile-time traits enforce the trivial-state rule, and the one-container rule is documented as a required code comment contract

## Container Example

When a state set needs a container, keep that state set to exactly one vector.

```cpp
#include <vector>
struct PlantState final
{
    float growth {0.0f};
};

class StateManager
{
public:
    std::size_t query_count() const noexcept
    {
        return plants_.size();
    }

    PlantState& query(std::uint32_t index)
    {
        return plants_.at(index);
    }

    const PlantState& query(std::uint32_t index) const
    {
        return plants_.at(index);
    }

    std::vector<PlantState>& plants() noexcept
    {
        return plants_;
    }

private:
    std::vector<PlantState> plants_ {};
};

class PlantResolverSystem
{
public:
    void tick(StateManager& state_manager)
    {
        for (std::size_t i = 0; i < state_manager.query_count(); ++i)
        {
            state_manager.query(static_cast<std::uint32_t>(i)).growth += 1.0f;
        }
    }
};

class PlantReaderSystem
{
public:
    void tick(const StateManager& state_manager, std::uint32_t index)
    {
        const PlantState& plant = state_manager.query(index);
        const float growth = plant.growth;
        (void)growth;
    }
};
```

This pattern means:

- one state set maps to one container
- `query(index)` has only one meaning
- the state set stays stable without extra query-shape branching
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

That keeps the exported structs trivial and lets the adapter read authoritative gameplay state without a cached query layer. The adapter should treat the returned pointer as transient and cast it according to the enum it passed in.

## Resolver Categories

Not every dependency should use the same resolver style.

### 1. Snapshot/query resolvers

Use these when a caller needs read-only facts.

Examples:

- read current weather
- read unlock status
- read site inventory summary
- read worker condition

### 2. Policy resolvers

Use these when a caller needs a decision or derived answer.

Examples:

- can this item be purchased
- can this structure be placed here
- is this recipe eligible
- what local-weather multiplier applies

### 3. Request resolvers

Use these when a caller needs to ask another owner to do something, but should not know who owns it.

Examples:

- request unlock purchase
- request task claim
- request inventory transfer

Important:

- request resolvers should usually enqueue `GameMessage` or another owner-handled command
- they should not directly mutate non-owned state

## What Should Not Become A Resolver

Do not turn everything into a resolver.

Keep these as they are:

- fixed-step execution order
- system lifecycle
- message subscriptions
- low-level ECS iteration helpers
- runtime create/destroy flow

Resolvers are for capability decoupling, not for replacing the whole runtime model.

## Compatibility With Current GS1 Rules

This design can fit the current repo rules if we preserve the following:

- systems still own their state slices
- direct system-to-system mutation still stays forbidden
- `GameMessage` remains the cross-owner mutation path
- ECS ownership stays private to the owner system
- resolver answers are read-only or request-based

That means this proposal is an extension of the current ownership model, not a rejection of it.

## Benefits

### Plug-friendly composition

A game can install:

- only inventory
- inventory plus technology
- technology without task board
- custom weather without the default weather owner

as long as defaults exist for omitted capabilities.

### Reduced compile-time coupling

A caller depends on:

- `IInventoryQueryResolver`

instead of:

- `InventorySystem`
- `InventoryState`
- helper functions scattered across site runtime files

### Better testing

A system test can provide:

- fake resolver
- default resolver
- real resolver

without spinning up unrelated systems.

### Cleaner feature replacement

A project can replace:

- default technology logic
- weather policy
- task generation rules

by swapping resolver providers rather than editing all dependents.

## Risks

### 1. Hidden dependency sprawl

If systems pull many resolvers ad hoc, dependencies become less visible.

Mitigation:

- require each system to declare its consumed resolver tags
- keep a compile-time dependency list beside message subscriptions

### 2. Resolver bloat

If interfaces become too large, they become mini god-APIs.

Mitigation:

- split resolvers by capability
- prefer narrow contracts
- keep query and request concerns separate

### 3. Write-path leakage

If a resolver starts returning mutable references into another owner's data, ownership collapses.

Mitigation:

- no cross-owner mutable references through resolver interfaces
- requests go through commands/messages only

### 4. Lifetime and registration bugs

If a system registers as resolver and then goes away, stale references can remain.

Mitigation:

- registration and unregistration belong to runtime system setup/teardown
- registry stores stable pointers only while the system lifetime is guaranteed

## Recommended Terminology

To keep this architecture understandable, I recommend these terms:

- `state slice`: authoritative owned mutable data
- `resolver`: typed capability interface for non-owned access
- `default resolver`: fallback implementation when no owner system overrides it
- `provider system`: a system that implements and registers a resolver
- `request`: cross-owner mutation intent, usually routed through message flow
- `query`: read-only access or derived answer

Avoid calling the new object just `StateManager` unless its role is narrowly defined, because that name tends to imply global authority and unrestricted access.

Better names:

- `StateResolverRegistry`
- `GameplayCapabilityRegistry`
- `StateServiceRegistry`

My recommendation is `StateResolverRegistry`.

## Proposed GS1 Refactor Shape

The current runtime already has:

- `RuntimeInvocation`
- tag-based read/write access
- `state_owner<Tag>`
- system interfaces and message subscriptions

That means the migration can be incremental.

### Step 1. Keep current state ownership and add resolver registry

Do not replace `GameState`, `SiteRunState`, or the ownership tags first.

First add:

- resolver interfaces
- resolver registry
- default resolver registration

### Step 2. Add a resolver-dependency declaration beside each system

Each system should declare:

- subscribed game messages
- subscribed host messages
- consumed resolver tags
- owned state tags

This preserves architectural visibility.

### Step 3. Convert read-only peer access to resolver calls

Replace cases where systems currently know too much about peer-owned state with typed resolver usage.

Best early targets:

- technology eligibility reads
- inventory availability reads
- weather-derived policy reads
- task-board lookup reads

### Step 4. Keep mutation on message flow

Do not let resolver adoption bypass `GameMessage`.

If a system wants another ownership domain to change:

- emit message
- let owner resolve it

### Step 5. Add project-specific override support

When the runtime builds systems, let a game/plugin install:

- no override
- custom override
- full provider system

This is the step that unlocks reusable composition.

## Suggested Runtime Contracts

### Resolver declaration

```cpp
struct WeatherResolverTag {};

class IWeatherStateResolver
{
public:
    virtual ~IWeatherStateResolver() = default;
    [[nodiscard]] virtual WeatherSnapshot current_weather(const ResolverReadContext& context) const = 0;
    [[nodiscard]] virtual LocalWeatherSnapshot local_weather_at(
        const ResolverReadContext& context,
        TileCoord tile) const = 0;
};
```

### Default resolver

```cpp
class DefaultWeatherStateResolver final : public IWeatherStateResolver
{
public:
    [[nodiscard]] WeatherSnapshot current_weather(const ResolverReadContext& context) const override;
    [[nodiscard]] LocalWeatherSnapshot local_weather_at(
        const ResolverReadContext& context,
        TileCoord tile) const override;
};
```

### Provider system

```cpp
class WeatherEventSystem final
    : public IRuntimeSystem
    , public IWeatherStateResolver
{
public:
    // system methods
    // resolver methods
};
```

### Registry usage

```cpp
const auto& weather = invocation.resolvers().get<WeatherResolverTag>();
const auto snapshot = weather.current_weather(read_context);
```

## Execution Order And Plugging Rules

Resolvers reduce structural dependency, but they do not remove all ordering concerns.

Rules should be:

- every capability has a default resolver
- registering a provider overrides the default
- systems may assume the resolver exists
- systems may not assume a specific concrete provider exists
- a provider system is still responsible for running in a valid fixed-step order for its own state updates

Important:

Plugging out a system means:

- the capability still exists
- behavior falls back to default semantics

It does not mean:

- all previous behaviors still happen identically

That distinction should be explicit in the docs and in tests.

## Open Design Choices

These need a decision before implementation:

### 1. Interface granularity

Do we want:

- one resolver per state slice

or:

- one resolver per gameplay capability

I recommend capability-first, because it is more stable than mirroring storage layout.

### 2. Default behavior strictness

Should missing capabilities:

- silently no-op
- return neutral values
- return "unsupported"
- assert in debug

I recommend:

- safe neutral runtime behavior
- plus debug-visible logging for unexpectedly missing non-optional capabilities

### 3. Resolver implementation base

Should resolvers be:

- pure interfaces with virtual dispatch
- template traits with static dispatch
- function-table structs

I recommend:

- typed interface plus registry first

because it is the fastest to adopt incrementally in the current codebase.

If performance later matters in hot paths, specific high-frequency resolvers can be flattened into function-table or trait-based forms.

### 4. Read context shape

Should resolvers receive:

- full `RuntimeInvocation`
- dedicated narrow `ResolverReadContext`

I recommend:

- narrow context

because otherwise the new architecture will keep too much accidental reach.

## Migration Plan

### Phase 0. Architecture inventory

Document:

- current owned state slices
- read-only peer access points
- cross-owner write paths
- candidate resolver capabilities

Deliverable:

- dependency inventory appendix added to this document or a follow-up inventory doc

### Phase 1. Runtime scaffolding

Add:

- `StateResolverRegistry`
- resolver tag traits
- default resolver registration
- resolver access from `RuntimeInvocation`

Do not change gameplay behavior yet.

### Phase 2. Resolver declarations

Introduce the first resolver interfaces for the highest-coupling read paths.

Suggested first batch:

- weather
- technology/unlock queries
- inventory queries
- task-board queries

### Phase 3. Provider registration

Let existing owner systems implement and register those resolvers.

Examples:

- `WeatherEventSystem`
- `TechnologySystem`
- `InventorySystem`
- `TaskBoardSystem`

### Phase 4. Caller migration

Replace direct peer-state reads with resolver usage in selected systems.

Do this one vertical slice at a time.

### Phase 5. Request-path cleanup

Where needed, add request resolvers that only translate intent into owner-handled messages.

Do not convert owner writes into direct calls.

### Phase 6. Plugin composition layer

Add runtime/system-build configuration that allows:

- default resolver only
- custom provider system
- custom non-system provider

This is where game-specific composition becomes first-class.

### Phase 7. Documentation and tests

Add:

- resolver contract docs
- default-behavior docs
- tests for missing-provider fallback
- tests for provider override behavior

## Recommended First Implementation Slice

The best first slice is not inventory or task board.

The best first slice is weather or technology, because:

- each is clearly owned
- many other systems only need read/query access
- default fallback behavior is easy to define

My preferred order:

1. add registry core
2. add `IWeatherStateResolver`
3. add default neutral weather resolver
4. let the real weather owner override it
5. migrate one dependent system to use the resolver
6. prove plug-out behavior with a focused test

After that, repeat for technology and inventory.

## Final Recommendation

I think your idea is strong if we phrase it as:

- central resolver registry
- default capability resolvers
- owner systems override defaults by implementing resolver interfaces
- reads and derived decisions go through resolvers
- writes still go through owner-controlled message flow

That gives you:

- decoupled systems
- plug-friendly game composition
- preserved ownership
- fewer hard dependency trees

The main thing I would avoid is turning this into a single central mutable state manager with unrestricted access. That would solve dependency wiring in the short term, but it would weaken the architecture you already built around owned state and message-driven coordination.

## Concrete Next Steps

1. Approve the resolver-registry direction and the rule that resolvers are read/query/request contracts, not cross-owner mutable access.
2. Decide the first resolver batch: I recommend weather, technology, and inventory.
3. Add runtime scaffolding for the registry without changing behavior.
4. Migrate one vertical slice and test the default-versus-provider override behavior.
5. Expand capability by capability instead of trying to convert the whole runtime in one pass.
