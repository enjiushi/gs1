#pragma once

#include "runtime/game_state.h"
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

struct RuntimeMoveDirectionSnapshot final
{
    float world_move_x {0.0f};
    float world_move_y {0.0f};
    float world_move_z {0.0f};
    bool present {false};
};

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

struct RuntimeCampaignTag
{
};

struct RuntimeActiveSiteRunTag
{
};

struct RuntimeFixedStepSecondsTag
{
};

struct RuntimeMoveDirectionTag
{
};

struct RuntimeSiteProtectionPresentationTag
{
};

struct RuntimeUiPresentationTag
{
};

template <class System>
struct system_state_tags;

template <class Tag>
struct state_owner;

template <class Tag>
struct state_traits;

class RuntimeInvocation final
{
public:
    explicit RuntimeInvocation(GameRuntime& runtime) noexcept;
    explicit RuntimeInvocation(GameState& state) noexcept;
    RuntimeInvocation(
        GameState& state,
        float move_direction_x,
        float move_direction_y,
        float move_direction_z,
        bool move_direction_present) noexcept;
    RuntimeInvocation(
        Gs1AppState& app_state,
        std::optional<CampaignState>& campaign,
        std::optional<SiteRunState>& active_site_run,
        std::deque<Gs1RuntimeMessage>& runtime_messages,
        GameMessageQueue& game_messages,
        double fixed_step_seconds,
        float move_direction_x = 0.0f,
        float move_direction_y = 0.0f,
        float move_direction_z = 0.0f,
        bool move_direction_present = false) noexcept;

    [[nodiscard]] GameRuntime* runtime() noexcept { return runtime_; }
    [[nodiscard]] const GameRuntime* runtime() const noexcept { return runtime_; }
    [[nodiscard]] GameState* owned_state() noexcept { return owned_state_; }
    [[nodiscard]] const GameState* owned_state() const noexcept { return owned_state_; }
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
    template <class Tag>
    friend decltype(auto) runtime_invocation_state_ref(const RuntimeInvocation& invocation);

    GameRuntime* runtime_ {nullptr};
    GameState* owned_state_ {nullptr};
    Gs1AppState* app_state_ {nullptr};
    std::optional<CampaignState>* campaign_ {nullptr};
    std::optional<SiteRunState>* active_site_run_ {nullptr};
    std::deque<Gs1RuntimeMessage>* runtime_messages_ {nullptr};
    GameMessageQueue* game_messages_ {nullptr};
    double* fixed_step_seconds_ {nullptr};
    RuntimeMoveDirectionSnapshot move_direction_ {};
};

template <class Tag>
decltype(auto) runtime_invocation_state_ref(RuntimeInvocation& invocation);

template <class Tag>
decltype(auto) runtime_invocation_state_ref(const RuntimeInvocation& invocation);

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeAppStateTag>(RuntimeInvocation& invocation)
{
    return (*invocation.app_state_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignTag>(RuntimeInvocation& invocation)
{
    return (*invocation.campaign_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeActiveSiteRunTag>(RuntimeInvocation& invocation)
{
    return (*invocation.active_site_run_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeFixedStepSecondsTag>(RuntimeInvocation& invocation)
{
    return (*invocation.fixed_step_seconds_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeMoveDirectionTag>(RuntimeInvocation& invocation)
{
    if (invocation.active_site_run_ == nullptr || !invocation.active_site_run_->has_value())
    {
        invocation.move_direction_ = {};
        return (invocation.move_direction_);
    }

    if (invocation.runtime_ != nullptr)
    {
        const auto& move_direction = invocation.active_site_run_->value().host_move_direction;
        invocation.move_direction_ = RuntimeMoveDirectionSnapshot {
            move_direction.world_move_x,
            move_direction.world_move_y,
            move_direction.world_move_z,
            move_direction.present};
    }

    return (invocation.move_direction_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeSiteProtectionPresentationTag>(
    RuntimeInvocation& invocation)
{
    return (invocation.owned_state_->site_protection_presentation);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeUiPresentationTag>(RuntimeInvocation& invocation)
{
    return (invocation.owned_state_->ui_presentation);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeAppStateTag>(const RuntimeInvocation& invocation)
{
    return (*invocation.app_state_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignTag>(const RuntimeInvocation& invocation)
{
    return (*invocation.campaign_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeActiveSiteRunTag>(const RuntimeInvocation& invocation)
{
    return (*invocation.active_site_run_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeFixedStepSecondsTag>(const RuntimeInvocation& invocation)
{
    return (*invocation.fixed_step_seconds_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeMoveDirectionTag>(const RuntimeInvocation& invocation)
{
    return runtime_invocation_state_ref<RuntimeMoveDirectionTag>(const_cast<RuntimeInvocation&>(invocation));
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeSiteProtectionPresentationTag>(
    const RuntimeInvocation& invocation)
{
    return (invocation.owned_state_->site_protection_presentation);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeUiPresentationTag>(
    const RuntimeInvocation& invocation)
{
    return (invocation.owned_state_->ui_presentation);
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
        return runtime_invocation_state_ref<Tag>(std::as_const(*invocation_));
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
