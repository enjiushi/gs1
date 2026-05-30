#pragma once

#include "runtime/game_state.h"
#include "runtime/runtime_split_state_compat.h"
#include "runtime/state_manager.h"
#include "runtime/type_list.h"
#include "gs1/status.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <optional>
#include <span>
#include <type_traits>

namespace gs1
{
class GameRuntime;
struct SiteWorldHandle;
struct SiteMoveDirectionInput;

struct RuntimeAppStateTag
{
};

struct RuntimeCampaignFactionProgressTag
{
};

struct RuntimeCampaignProgressionTag
{
};

struct RuntimeCampaignTimeTag
{
};

struct RuntimeCampaignTechnologyTag
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

class CampaignFlowSystem;
class CampaignProgressionSystem;
class CampaignTimeSystem;
class FactionReputationSystem;
class TechnologySystem;
class SiteFlowSystem;

template <>
struct state_owner<RuntimeAppStateTag>
{
    using type = CampaignFlowSystem;
};

template <>
struct state_owner<RuntimeCampaignTimeTag>
{
    using type = CampaignTimeSystem;
};

template <>
struct state_owner<RuntimeCampaignFactionProgressTag>
{
    using type = CampaignProgressionSystem;
};

template <>
struct state_owner<RuntimeCampaignProgressionTag>
{
    using type = CampaignProgressionSystem;
};

template <>
struct state_owner<RuntimeCampaignTechnologyTag>
{
    using type = TechnologySystem;
};

template <>
struct state_owner<RuntimeMoveDirectionTag>
{
    using type = SiteFlowSystem;
};

class RuntimeInvocation final
{
public:
    explicit RuntimeInvocation(GameRuntime& runtime) noexcept;
    explicit RuntimeInvocation(
        GameState& state,
        GameMessageQueue* emitted_game_messages = nullptr) noexcept;
    RuntimeInvocation(
        GameState& state,
        StateManager& state_manager,
        SiteWorldHandle site_world = {},
        GameMessageQueue* emitted_game_messages = nullptr) noexcept;
    RuntimeInvocation(
        GameState& state,
        StateManager& state_manager,
        float move_direction_x,
        float move_direction_y,
        float move_direction_z,
        bool move_direction_present,
        SiteWorldHandle site_world = {},
        GameMessageQueue* emitted_game_messages = nullptr) noexcept;
    ~RuntimeInvocation() = default;

    RuntimeInvocation(const RuntimeInvocation&) = delete;
    RuntimeInvocation& operator=(const RuntimeInvocation&) = delete;

    [[nodiscard]] GameRuntime* runtime() noexcept { return runtime_; }
    [[nodiscard]] const GameRuntime* runtime() const noexcept { return runtime_; }
    [[nodiscard]] GameState* owned_state() noexcept { return owned_state_; }
    [[nodiscard]] const GameState* owned_state() const noexcept { return owned_state_; }
    [[nodiscard]] StateManager* state_manager() noexcept { return state_manager_; }
    [[nodiscard]] const StateManager* state_manager() const noexcept { return state_manager_; }
    void set_site_world(const SiteWorldHandle& site_world) noexcept { site_world_ = site_world; }
    void set_site_world(std::nullptr_t) noexcept { site_world_ = nullptr; }
    [[nodiscard]] SiteWorld* site_world() noexcept { return site_world_.get(); }
    [[nodiscard]] const SiteWorld* site_world() const noexcept { return site_world_.get(); }
    [[nodiscard]] const SiteWorldHandle& site_world_handle() const noexcept { return site_world_; }
    [[nodiscard]] std::deque<Gs1RuntimeMessage>& runtime_message_queue() noexcept
    {
        return *runtime_messages_;
    }
    [[nodiscard]] const std::deque<Gs1RuntimeMessage>& runtime_message_queue() const noexcept
    {
        return *runtime_messages_;
    }

    void install_campaign_state(const CampaignState& campaign);
    void install_site_run_state(const SiteRunState& site_run);
    void clear_site_run_state();
    template <typename Message>
    void emit_game_message(const Message& message);
    void push_runtime_message(const Gs1RuntimeMessage& message);
    void push_log_message(Gs1LogLevel level, const char* text);

private:
    template <class Tag>
    friend decltype(auto) runtime_invocation_state_ref(RuntimeInvocation& invocation);
    template <class Tag>
    friend decltype(auto) runtime_invocation_state_cref(const RuntimeInvocation& invocation);

    GameRuntime* runtime_ {nullptr};
    GameState* owned_state_ {nullptr};
    StateManager* state_manager_ {nullptr};
    Gs1AppState* app_state_ {nullptr};
    double* fixed_step_seconds_ {nullptr};
    RuntimeMoveDirectionSnapshot move_direction_ {};
    SiteWorldHandle site_world_ {};
    GameMessageQueue* emitted_game_messages_ {nullptr};
    std::deque<Gs1RuntimeMessage>* runtime_messages_ {nullptr};
};

template <class Tag>
decltype(auto) runtime_invocation_state_ref(RuntimeInvocation& invocation);

template <class Tag>
decltype(auto) runtime_invocation_state_cref(const RuntimeInvocation& invocation);

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
inline decltype(auto) runtime_invocation_state_cref<RuntimeAppStateTag>(
    const RuntimeInvocation& invocation)
{
    if (invocation.app_state_ != nullptr)
    {
        return std::as_const(*invocation.app_state_);
    }

    return invocation.state_manager_->query<StateSetId::AppState>(*invocation.owned_state_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignTimeTag>(RuntimeInvocation& invocation)
{
    return invocation.state_manager_->state<StateSetId::CampaignTime>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_cref<RuntimeCampaignTimeTag>(
    const RuntimeInvocation& invocation)
{
    return invocation.state_manager_->query<StateSetId::CampaignTime>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignFactionProgressTag>(
    RuntimeInvocation& invocation)
{
    return invocation.state_manager_->state<StateSetId::CampaignFactionProgress>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_cref<RuntimeCampaignFactionProgressTag>(
    const RuntimeInvocation& invocation)
{
    return invocation.state_manager_->query<StateSetId::CampaignFactionProgress>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignProgressionTag>(
    RuntimeInvocation& invocation)
{
    return invocation.state_manager_->state<StateSetId::CampaignProgression>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_cref<RuntimeCampaignProgressionTag>(
    const RuntimeInvocation& invocation)
{
    return invocation.state_manager_->query<StateSetId::CampaignProgression>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignTechnologyTag>(RuntimeInvocation& invocation)
{
    return invocation.state_manager_->state<StateSetId::CampaignTechnology>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_cref<RuntimeCampaignTechnologyTag>(
    const RuntimeInvocation& invocation)
{
    return invocation.state_manager_->query<StateSetId::CampaignTechnology>(*invocation.owned_state_).value();
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
inline decltype(auto) runtime_invocation_state_cref<RuntimeFixedStepSecondsTag>(
    const RuntimeInvocation& invocation)
{
    if (invocation.fixed_step_seconds_ != nullptr)
    {
        return std::as_const(*invocation.fixed_step_seconds_);
    }

    return invocation.state_manager_->query<StateSetId::FixedStepSeconds>(*invocation.owned_state_);
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

template <>
inline decltype(auto) runtime_invocation_state_cref<RuntimeMoveDirectionTag>(
    const RuntimeInvocation& invocation)
{
    if (invocation.move_direction_.present)
    {
        return std::as_const(invocation.move_direction_);
    }

    if (invocation.state_manager_ != nullptr && invocation.owned_state_ != nullptr)
    {
        return invocation.state_manager_->query<StateSetId::MoveDirection>(*invocation.owned_state_);
    }

    return std::as_const(invocation.move_direction_);
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
        return runtime_invocation_state_cref<Tag>(*invocation_);
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
