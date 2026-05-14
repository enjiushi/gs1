#pragma once

#include "campaign/campaign_state.h"
#include "runtime/game_state.h"
#include "runtime/game_state_view.h"
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

struct RuntimeCampaignTag
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

struct RuntimeActiveSiteRunTag
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
    RuntimeInvocation(
        Gs1AppState& app_state,
        std::optional<CampaignState>& campaign,
        std::optional<SiteRunState>& active_site_run,
        std::deque<Gs1RuntimeMessage>& runtime_messages,
        GameMessageQueue& game_messages,
        StateManager& state_manager,
        double fixed_step_seconds,
        float move_direction_x = 0.0f,
        float move_direction_y = 0.0f,
        float move_direction_z = 0.0f,
        bool move_direction_present = false) noexcept;
    ~RuntimeInvocation();

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

    void hydrate_campaign_cache_from_owned_state();
    void flush_campaign_cache_to_owned_state();
    void hydrate_site_cache_from_owned_state();
    void flush_site_cache_to_owned_state();

    GameRuntime* runtime_ {nullptr};
    GameState* owned_state_ {nullptr};
    StateManager* state_manager_ {nullptr};
    Gs1AppState* app_state_ {nullptr};
    std::optional<CampaignState>* campaign_ {nullptr};
    std::optional<SiteRunState>* active_site_run_ {nullptr};
    double* fixed_step_seconds_ {nullptr};
    RuntimeMoveDirectionSnapshot move_direction_ {};
    std::deque<Gs1RuntimeMessage>* runtime_messages_ {nullptr};
    GameMessageQueue* game_messages_ {nullptr};

    bool owns_campaign_cache_ {false};
    bool owns_site_cache_ {false};
    std::optional<CampaignState> campaign_cache_ {};
    std::optional<SiteRunState> site_run_cache_ {};
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
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignTag>(RuntimeInvocation& invocation)
{
    if (invocation.campaign_ != nullptr)
    {
        return (*invocation.campaign_);
    }

    invocation.hydrate_campaign_cache_from_owned_state();
    return (invocation.campaign_cache_);
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignRegionalMapTag>(RuntimeInvocation& invocation)
{
    if (invocation.campaign_ != nullptr)
    {
        return (invocation.campaign_->value().regional_map_state);
    }

    return invocation.state_manager_->state<StateSetId::CampaignRegionalMap>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignFactionProgressTag>(
    RuntimeInvocation& invocation)
{
    if (invocation.campaign_ != nullptr)
    {
        return (invocation.campaign_->value().faction_progress);
    }

    return invocation.state_manager_->state<StateSetId::CampaignFactionProgress>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignTechnologyTag>(RuntimeInvocation& invocation)
{
    if (invocation.campaign_ != nullptr)
    {
        return (invocation.campaign_->value().technology_state);
    }

    return invocation.state_manager_->state<StateSetId::CampaignTechnology>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignLoadoutPlannerTag>(
    RuntimeInvocation& invocation)
{
    if (invocation.campaign_ != nullptr)
    {
        return (invocation.campaign_->value().loadout_planner_state);
    }

    return invocation.state_manager_->state<StateSetId::CampaignLoadoutPlanner>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeCampaignSitesTag>(RuntimeInvocation& invocation)
{
    if (invocation.campaign_ != nullptr)
    {
        return (invocation.campaign_->value().sites);
    }

    return invocation.state_manager_->state<StateSetId::CampaignSites>(*invocation.owned_state_).value();
}

template <>
inline decltype(auto) runtime_invocation_state_ref<RuntimeActiveSiteRunTag>(RuntimeInvocation& invocation)
{
    if (invocation.active_site_run_ != nullptr)
    {
        return (*invocation.active_site_run_);
    }

    invocation.hydrate_site_cache_from_owned_state();
    return (invocation.site_run_cache_);
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
    if (invocation.active_site_run_ != nullptr && invocation.active_site_run_->has_value())
    {
        const auto& move_direction = invocation.active_site_run_->value().host_move_direction;
        invocation.move_direction_ = RuntimeMoveDirectionSnapshot {
            move_direction.world_move_x,
            move_direction.world_move_y,
            move_direction.world_move_z,
            move_direction.present};
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

[[nodiscard]] inline SiteRunState runtime_assemble_site_run_state_from_sets(
    const GameState& state,
    const StateManager& state_manager)
{
    SiteRunState site_run {};

    const auto& meta = state_manager.query<StateSetId::SiteRunMeta>(state);
    const auto& world = state_manager.query<StateSetId::SiteWorld>(state);
    const auto& clock = state_manager.query<StateSetId::SiteClock>(state);
    const auto& camp = state_manager.query<StateSetId::SiteCamp>(state);
    const auto& inventory = state_manager.query<StateSetId::SiteInventory>(state);
    const auto& contractor = state_manager.query<StateSetId::SiteContractor>(state);
    const auto& weather = state_manager.query<StateSetId::SiteWeather>(state);
    const auto& event = state_manager.query<StateSetId::SiteEvent>(state);
    const auto& task_board = state_manager.query<StateSetId::SiteTaskBoard>(state);
    const auto& modifier = state_manager.query<StateSetId::SiteModifier>(state);
    const auto& economy = state_manager.query<StateSetId::SiteEconomy>(state);
    const auto& craft = state_manager.query<StateSetId::SiteCraft>(state);
    const auto& action = state_manager.query<StateSetId::SiteAction>(state);
    const auto& counters = state_manager.query<StateSetId::SiteCounters>(state);
    const auto& objective = state_manager.query<StateSetId::SiteObjective>(state);
    const auto& local_weather = state_manager.query<StateSetId::SiteLocalWeatherResolve>(state);
    const auto& plant_weather = state_manager.query<StateSetId::SitePlantWeatherContribution>(state);
    const auto& device_weather = state_manager.query<StateSetId::SiteDeviceWeatherContribution>(state);
    const auto& move_direction = state_manager.query<StateSetId::MoveDirection>(state);

    if (!meta.has_value())
    {
        return site_run;
    }

    site_run.site_run_id = meta->site_run_id;
    site_run.site_id = meta->site_id;
    site_run.site_archetype_id = meta->site_archetype_id;
    site_run.attempt_index = meta->attempt_index;
    site_run.site_attempt_seed = meta->site_attempt_seed;
    site_run.run_status = meta->run_status;
    site_run.result_newly_revealed_site_count = meta->result_newly_revealed_site_count;
    if (world.has_value())
    {
        site_run.site_world = world->site_world;
    }
    if (clock.has_value())
    {
        site_run.clock = *clock;
    }
    if (camp.has_value())
    {
        site_run.camp = *camp;
    }
    if (inventory.has_value())
    {
        site_run.inventory = *inventory;
    }
    if (contractor.has_value())
    {
        site_run.contractor = *contractor;
    }
    if (weather.has_value())
    {
        site_run.weather = *weather;
    }
    if (event.has_value())
    {
        site_run.event = *event;
    }
    if (task_board.has_value())
    {
        site_run.task_board = *task_board;
    }
    if (modifier.has_value())
    {
        site_run.modifier = *modifier;
    }
    if (economy.has_value())
    {
        site_run.economy = *economy;
    }
    if (craft.has_value())
    {
        site_run.craft = *craft;
    }
    if (action.has_value())
    {
        site_run.site_action = *action;
    }
    if (counters.has_value())
    {
        site_run.counters = *counters;
    }
    if (objective.has_value())
    {
        site_run.objective = *objective;
    }
    if (local_weather.has_value())
    {
        site_run.local_weather_resolve = *local_weather;
    }
    if (plant_weather.has_value())
    {
        site_run.plant_weather_contribution = *plant_weather;
    }
    if (device_weather.has_value())
    {
        site_run.device_weather_contribution = *device_weather;
    }

    site_run.host_move_direction = SiteHostMoveDirectionState {
        move_direction.world_move_x,
        move_direction.world_move_y,
        move_direction.world_move_z,
        move_direction.present};
    return site_run;
}

inline void runtime_write_site_run_state_to_sets(
    const std::optional<SiteRunState>& site_run,
    GameState& state,
    StateManager& state_manager)
{
    if (!site_run.has_value())
    {
        state_manager.state<StateSetId::SiteRunMeta>(state).reset();
        state_manager.state<StateSetId::SiteWorld>(state).reset();
        state_manager.state<StateSetId::SiteClock>(state).reset();
        state_manager.state<StateSetId::SiteCamp>(state).reset();
        state_manager.state<StateSetId::SiteInventory>(state).reset();
        state_manager.state<StateSetId::SiteContractor>(state).reset();
        state_manager.state<StateSetId::SiteWeather>(state).reset();
        state_manager.state<StateSetId::SiteEvent>(state).reset();
        state_manager.state<StateSetId::SiteTaskBoard>(state).reset();
        state_manager.state<StateSetId::SiteModifier>(state).reset();
        state_manager.state<StateSetId::SiteEconomy>(state).reset();
        state_manager.state<StateSetId::SiteCraft>(state).reset();
        state_manager.state<StateSetId::SiteAction>(state).reset();
        state_manager.state<StateSetId::SiteCounters>(state).reset();
        state_manager.state<StateSetId::SiteObjective>(state).reset();
        state_manager.state<StateSetId::SiteLocalWeatherResolve>(state).reset();
        state_manager.state<StateSetId::SitePlantWeatherContribution>(state).reset();
        state_manager.state<StateSetId::SiteDeviceWeatherContribution>(state).reset();
        state_manager.state<StateSetId::MoveDirection>(state) = RuntimeMoveDirectionSnapshot {};
        return;
    }

    state_manager.state<StateSetId::SiteRunMeta>(state) = SiteRunMetaState {
        site_run->site_run_id,
        site_run->site_id,
        site_run->site_archetype_id,
        site_run->attempt_index,
        site_run->site_attempt_seed,
        site_run->run_status,
        site_run->result_newly_revealed_site_count};
    state_manager.state<StateSetId::SiteWorld>(state) = SiteWorldState {site_run->site_world};
    state_manager.state<StateSetId::SiteClock>(state) = site_run->clock;
    state_manager.state<StateSetId::SiteCamp>(state) = site_run->camp;
    state_manager.state<StateSetId::SiteInventory>(state) = site_run->inventory;
    state_manager.state<StateSetId::SiteContractor>(state) = site_run->contractor;
    state_manager.state<StateSetId::SiteWeather>(state) = site_run->weather;
    state_manager.state<StateSetId::SiteEvent>(state) = site_run->event;
    state_manager.state<StateSetId::SiteTaskBoard>(state) = site_run->task_board;
    state_manager.state<StateSetId::SiteModifier>(state) = site_run->modifier;
    state_manager.state<StateSetId::SiteEconomy>(state) = site_run->economy;
    state_manager.state<StateSetId::SiteCraft>(state) = site_run->craft;
    state_manager.state<StateSetId::SiteAction>(state) = site_run->site_action;
    state_manager.state<StateSetId::SiteCounters>(state) = site_run->counters;
    state_manager.state<StateSetId::SiteObjective>(state) = site_run->objective;
    state_manager.state<StateSetId::SiteLocalWeatherResolve>(state) = site_run->local_weather_resolve;
    state_manager.state<StateSetId::SitePlantWeatherContribution>(state) = site_run->plant_weather_contribution;
    state_manager.state<StateSetId::SiteDeviceWeatherContribution>(state) =
        site_run->device_weather_contribution;
    state_manager.state<StateSetId::MoveDirection>(state) = RuntimeMoveDirectionSnapshot {
        site_run->host_move_direction.world_move_x,
        site_run->host_move_direction.world_move_y,
        site_run->host_move_direction.world_move_z,
        site_run->host_move_direction.present};
}

[[nodiscard]] inline bool runtime_invocation_has_active_site_run(RuntimeInvocation& invocation) noexcept
{
    if (runtime_invocation_uses_split_state_sets(invocation))
    {
        return invocation.state_manager()
            ->query<StateSetId::SiteRunMeta>(*invocation.owned_state())
            .has_value();
    }

    return runtime_invocation_state_ref<RuntimeActiveSiteRunTag>(invocation).has_value();
}

[[nodiscard]] inline SiteRunState runtime_invocation_active_site_run_copy(RuntimeInvocation& invocation)
{
    if (runtime_invocation_uses_split_state_sets(invocation))
    {
        return runtime_assemble_site_run_state_from_sets(
            *invocation.owned_state(),
            *invocation.state_manager());
    }

    const auto& active_site_run = runtime_invocation_state_ref<RuntimeActiveSiteRunTag>(invocation);
    return active_site_run.has_value() ? *active_site_run : SiteRunState {};
}

inline void runtime_invocation_assign_active_site_run(
    RuntimeInvocation& invocation,
    const SiteRunState& site_run)
{
    if (runtime_invocation_uses_split_state_sets(invocation))
    {
        runtime_write_site_run_state_to_sets(
            std::optional<SiteRunState> {site_run},
            *invocation.owned_state(),
            *invocation.state_manager());
        return;
    }

    runtime_invocation_state_ref<RuntimeActiveSiteRunTag>(invocation) = site_run;
}

inline void runtime_invocation_clear_active_site_run(RuntimeInvocation& invocation)
{
    if (runtime_invocation_uses_split_state_sets(invocation))
    {
        runtime_write_site_run_state_to_sets(
            std::nullopt,
            *invocation.owned_state(),
            *invocation.state_manager());
        return;
    }

    runtime_invocation_state_ref<RuntimeActiveSiteRunTag>(invocation).reset();
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
