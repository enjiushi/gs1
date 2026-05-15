#pragma once

#include "runtime/game_state.h"
#include "runtime/state_manager.h"
#include "gs1/status.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <type_traits>

namespace gs1
{
class GameRuntime;
struct SiteMoveDirectionInput;

template <class... Ts>
struct type_list
{
};

template <class T, class List>
struct type_list_contains;

template <class T, class... Ts>
struct type_list_contains<T, type_list<Ts...>> : std::bool_constant<(std::same_as<T, Ts> || ...)>
{
};

template <class T, class List>
inline constexpr bool type_list_contains_v = type_list_contains<T, List>::value;

struct RuntimeAppStateTag
{
};

struct RuntimeCampaignRegionalMapTag
{
};

struct RuntimeCampaignFactionProgressTag
{
};

struct RuntimeCampaignTechnologyTag
{
};

struct RuntimeCampaignLoadoutPlannerTag
{
};

struct RuntimeCampaignSitesTag
{
};

struct RuntimeFixedStepSecondsTag
{
};

struct RuntimeMoveDirectionTag
{
};

template <class System>
struct system_state_tags;

template <class Tag>
struct state_owner;

class RuntimeInvocation final
{
public:
    explicit RuntimeInvocation(GameRuntime& runtime) noexcept;
    explicit RuntimeInvocation(GameState& state) noexcept;
    RuntimeInvocation(GameState& state, StateManager& state_manager) noexcept;
    RuntimeInvocation(
        GameState& state,
        StateManager& state_manager,
        float move_direction_x,
        float move_direction_y,
        float move_direction_z,
        bool move_direction_present) noexcept;
    ~RuntimeInvocation() = default;

    RuntimeInvocation(const RuntimeInvocation&) = delete;
    RuntimeInvocation& operator=(const RuntimeInvocation&) = delete;

    [[nodiscard]] GameRuntime* runtime() noexcept { return runtime_; }
    [[nodiscard]] const GameRuntime* runtime() const noexcept { return runtime_; }
    [[nodiscard]] GameState* owned_state() noexcept { return owned_state_; }
    [[nodiscard]] const GameState* owned_state() const noexcept { return owned_state_; }
    [[nodiscard]] StateManager* state_manager() noexcept { return state_manager_; }
    [[nodiscard]] const StateManager* state_manager() const noexcept { return state_manager_; }
    [[nodiscard]] GameMessageQueue& game_message_queue() noexcept { return *game_messages_; }
    [[nodiscard]] const GameMessageQueue& game_message_queue() const noexcept { return *game_messages_; }
    [[nodiscard]] std::deque<Gs1RuntimeMessage>& runtime_message_queue() noexcept
    {
        return *runtime_messages_;
    }
    [[nodiscard]] const std::deque<Gs1RuntimeMessage>& runtime_message_queue() const noexcept
    {
        return *runtime_messages_;
    }

    void push_game_message(const GameMessage& message);
    void push_runtime_message(const Gs1RuntimeMessage& message);

private:
    template <class Tag>
    friend decltype(auto) runtime_invocation_state_ref(RuntimeInvocation& invocation);

    GameRuntime* runtime_ {nullptr};
    GameState* owned_state_ {nullptr};
    StateManager* state_manager_ {nullptr};
    Gs1AppState* app_state_ {nullptr};
    double* fixed_step_seconds_ {nullptr};
    RuntimeMoveDirectionSnapshot move_direction_ {};
    std::deque<Gs1RuntimeMessage>* runtime_messages_ {nullptr};
    GameMessageQueue* game_messages_ {nullptr};
};

template <class Tag>
decltype(auto) runtime_invocation_state_ref(RuntimeInvocation& invocation);

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeAppStateTag>(RuntimeInvocation& invocation)
{
    if (invocation.app_state_ != nullptr)
    {
        return (*invocation.app_state_);
    }

    return invocation.state_manager_->state<StateSetId::AppState>(*invocation.owned_state_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignRegionalMapTag>(RuntimeInvocation& invocation)
{
    return invocation.state_manager_->state<StateSetId::CampaignRegionalMap>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignFactionProgressTag>(
    RuntimeInvocation& invocation)
{
    return invocation.state_manager_->state<StateSetId::CampaignFactionProgress>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignTechnologyTag>(RuntimeInvocation& invocation)
{
    return invocation.state_manager_->state<StateSetId::CampaignTechnology>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignLoadoutPlannerTag>(
    RuntimeInvocation& invocation)
{
    return invocation.state_manager_->state<StateSetId::CampaignLoadoutPlanner>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignSitesTag>(RuntimeInvocation& invocation)
{
    return invocation.state_manager_->state<StateSetId::CampaignSites>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeFixedStepSecondsTag>(RuntimeInvocation& invocation)
{
    if (invocation.fixed_step_seconds_ != nullptr)
    {
        return (*invocation.fixed_step_seconds_);
    }

    return invocation.state_manager_->state<StateSetId::FixedStepSeconds>(*invocation.owned_state_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeMoveDirectionTag>(RuntimeInvocation& invocation)
{
    if (invocation.move_direction_.present)
    {
        return (invocation.move_direction_);
    }

    if (invocation.state_manager_ != nullptr && invocation.owned_state_ != nullptr)
    {
        return invocation.state_manager_->state<StateSetId::MoveDirection>(*invocation.owned_state_);
    }

    return (invocation.move_direction_);
}

[[nodiscard]] inline bool runtime_invocation_uses_split_state_sets(
    const RuntimeInvocation& invocation) noexcept
{
    return invocation.owned_state() != nullptr && invocation.state_manager() != nullptr;
}

[[nodiscard]] inline bool runtime_invocation_has_active_site_run(RuntimeInvocation& invocation) noexcept
{
    return invocation.state_manager()
        ->query<StateSetId::SiteRunMeta>(*invocation.owned_state())
        .has_value();
}

[[nodiscard]] inline bool runtime_invocation_has_campaign(RuntimeInvocation& invocation) noexcept
{
    return invocation.state_manager()
        ->query<StateSetId::CampaignCore>(*invocation.owned_state())
        .has_value();
}

template <class System>
class GameStateAccess final
{
public:
    explicit GameStateAccess(RuntimeInvocation& invocation) noexcept
        : invocation_(&invocation)
    {
    }

    template <class Tag>
    [[nodiscard]] decltype(auto) read() const
    {
        using tags = typename system_state_tags<System>::type;
        static_assert(type_list_contains_v<Tag, tags>, "System cannot read this state tag.");
        return runtime_invocation_state_ref<Tag>(*invocation_);
    }

    template <class Tag>
    [[nodiscard]] decltype(auto) write()
        requires std::same_as<System, typename state_owner<Tag>::type>
    {
        using tags = typename system_state_tags<System>::type;
        static_assert(type_list_contains_v<Tag, tags>, "System cannot access this state tag.");
        return runtime_invocation_state_ref<Tag>(*invocation_);
    }

private:
    RuntimeInvocation* invocation_ {nullptr};
};

template <class System>
[[nodiscard]] constexpr auto make_game_state_access(RuntimeInvocation& invocation) -> GameStateAccess<System>
{
    return GameStateAccess<System> {invocation};
}
}  // namespace gs1
