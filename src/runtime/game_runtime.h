#pragma once

#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_time_system.h"
#include "messages/game_message.h"
#include "runtime/gameplay_message_traits.h"
#include "runtime/game_state.h"
#include "runtime/state_manager.h"
#include "runtime/game_state_view.h"
#include "runtime/runtime_clock.h"
#include "runtime/system_interface.h"
#include "runtime/typed_gameplay_dispatch_traits.h"
#include "site/site_world.h"
#include "gs1/status.h"
#include "gs1/types.h"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <utility>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gs1
{
inline constexpr std::uint32_t k_api_version = 7;

class RuntimeSemanticGameMessageScope;

struct RuntimeGameMessageSubscriberEntry final
{
    IRuntimeSystem* system {nullptr};
    std::optional<Gs1RuntimeProfileSystemId> profile_id {};
};

using RuntimeGameMessageSubscriberEntryArray =
    std::array<
        std::vector<RuntimeGameMessageSubscriberEntry>,
        k_game_message_type_count>;

template <typename Message>
[[nodiscard]] constexpr std::string_view runtime_debug_type_name() noexcept
{
#if defined(_MSC_VER)
    constexpr std::string_view signature = __FUNCSIG__;
    constexpr std::string_view prefix = "runtime_debug_type_name<";
    constexpr std::string_view suffix = ">(void)";
#elif defined(__clang__)
    constexpr std::string_view signature = __PRETTY_FUNCTION__;
    constexpr std::string_view prefix = "runtime_debug_type_name() [Message = ";
    constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
    constexpr std::string_view signature = __PRETTY_FUNCTION__;
    constexpr std::string_view prefix =
        "constexpr std::string_view gs1::runtime_debug_type_name() [with Message = ";
    constexpr std::string_view suffix = "; std::string_view = std::basic_string_view<char>]";
#else
    return "unknown";
#endif

    const std::size_t start = signature.find(prefix);
    const std::size_t end = signature.rfind(suffix);
    if (start == std::string_view::npos || end == std::string_view::npos || end <= start + prefix.size())
    {
        return "unknown";
    }

    auto name = signature.substr(start + prefix.size(), end - (start + prefix.size()));
    if (name.starts_with("struct "))
    {
        name.remove_prefix(7U);
    }
    else if (name.starts_with("class "))
    {
        name.remove_prefix(6U);
    }
    else if (name.starts_with("enum "))
    {
        name.remove_prefix(5U);
    }

    if (name.starts_with("gs1::"))
    {
        name.remove_prefix(5U);
    }

    return name;
}

class GameRuntime final
{
public:
    explicit GameRuntime(Gs1RuntimeCreateDesc create_desc);

    [[nodiscard]] Gs1Status submit_host_messages(
        const Gs1HostMessage* messages,
        std::uint32_t message_count);
    [[nodiscard]] Gs1Status run_phase1(const Gs1Phase1Request& request, Gs1Phase1Result& out_result);
    [[nodiscard]] Gs1Status run_phase2(const Gs1Phase2Request& request, Gs1Phase2Result& out_result);
    [[nodiscard]] Gs1Status pop_runtime_message(Gs1RuntimeMessage& out_message);
    [[nodiscard]] Gs1Status get_game_state_view(Gs1GameStateView& out_view);
    [[nodiscard]] Gs1Status query_site_tile_view(std::uint32_t tile_index, Gs1SiteTileView& out_tile) const;
    [[nodiscard]] Gs1Status get_profiling_snapshot(Gs1RuntimeProfilingSnapshot& out_snapshot) const noexcept;
    void reset_profiling() noexcept;
    [[nodiscard]] Gs1Status set_profiled_system_enabled(
        Gs1RuntimeProfileSystemId system_id,
        bool enabled) noexcept;
    [[nodiscard]] bool profiled_system_enabled(Gs1RuntimeProfileSystemId system_id) const noexcept;
#ifndef NDEBUG
    [[nodiscard]] std::span<const std::string_view> debug_semantic_game_message_stack() const noexcept
    {
        return debug_semantic_game_message_stack_;
    }
    [[nodiscard]] std::span<const std::string_view> debug_last_semantic_game_message_stack() const noexcept
    {
        return debug_last_semantic_game_message_stack_;
    }
#endif

    [[nodiscard]] GameState& state() noexcept { return state_manager_.game_state(); }
    [[nodiscard]] const GameState& state() const noexcept { return state_manager_.game_state(); }
    [[nodiscard]] StateManager& state_manager() noexcept { return state_manager_; }
    [[nodiscard]] const StateManager& state_manager() const noexcept { return state_manager_; }
    [[nodiscard]] GameMessageQueue& message_queue() noexcept { return state().message_queue; }
    [[nodiscard]] const GameMessageQueue& message_queue() const noexcept { return state().message_queue; }
    [[nodiscard]] std::deque<Gs1RuntimeMessage>& runtime_messages() noexcept { return state().runtime_messages; }
    [[nodiscard]] const std::deque<Gs1RuntimeMessage>& runtime_messages() const noexcept
    {
        return state().runtime_messages;
    }
    void set_site_world(const SiteWorldHandle& site_world) noexcept { site_world_ = site_world; }
    void set_site_world(std::nullptr_t) noexcept { site_world_ = nullptr; }
    [[nodiscard]] SiteWorld* site_world() noexcept { return site_world_.get(); }
    [[nodiscard]] const SiteWorld* site_world() const noexcept { return site_world_.get(); }
    [[nodiscard]] Gs1Status handle_message(const GameMessage& message);
    template <typename Message>
    [[nodiscard]] Gs1Status handle_message(const Message& message);
    friend struct GameRuntimeProjectionTestAccess;
    friend class RuntimeInvocation;
    friend class RuntimeInlineGameMessageScope;
    friend class RuntimeSemanticGameMessageScope;

private:
    struct TimingAccumulator final
    {
        std::uint64_t sample_count {0U};
        double last_elapsed_ms {0.0};
        double total_elapsed_ms {0.0};
        double max_elapsed_ms {0.0};
    };

    struct ProfiledSystemState final
    {
        bool enabled {true};
        TimingAccumulator run_timing {};
        TimingAccumulator message_timing {};
    };

    struct FixedStepSystemEntry final
    {
        IRuntimeSystem* system {nullptr};
        std::optional<Gs1RuntimeProfileSystemId> profile_id {};
        std::uint32_t order {0U};
    };

    void initialize_system_registry();
    [[nodiscard]] Gs1Status dispatch_host_messages(std::uint32_t& out_processed_count);
    [[nodiscard]] Gs1Status dispatch_queued_messages();
    [[nodiscard]] Gs1Status dispatch_subscribed_host_message(const Gs1HostMessage& message);
    [[nodiscard]] Gs1Status dispatch_subscribed_message(const GameMessage& message);
    void install_campaign_state(const CampaignState& campaign);
    void install_site_run_state(const SiteRunState& site_run);
    void clear_site_run_state();
    void run_fixed_step();
    [[nodiscard]] Gs1Status dispatch_game_message_inline(const GameMessage& message);
    template <typename Message>
    [[nodiscard]] Gs1Status dispatch_game_message_inline(const Message& message);
    template <typename Message>
    [[nodiscard]] Gs1Status dispatch_typed_game_message_to_subscribers(const Message& message);
    template <typename System>
    [[nodiscard]] System* find_system() noexcept;
    void copy_timing_snapshot(
        const TimingAccumulator& source,
        Gs1RuntimeTimingStats& destination) const noexcept;
#ifndef NDEBUG
    void push_debug_semantic_game_message(std::string_view message_name);
    void pop_debug_semantic_game_message() noexcept;
    void print_debug_semantic_game_message_stack() const;
#endif

private:
    Gs1RuntimeCreateDesc create_desc_ {};
    std::string adapter_config_json_utf8_ {};
    SiteWorldHandle site_world_ {};
    StateManager state_manager_ {};
    GameState& state_;
    RuntimeGameStateViewCache state_view_cache_ {};
    std::deque<Gs1HostMessage> host_messages_ {};
    std::vector<std::unique_ptr<IRuntimeSystem>> systems_ {};
    std::vector<FixedStepSystemEntry> fixed_step_systems_ {};
    RuntimeHostMessageSubscribers host_message_subscribers_ {};
    RuntimeGameMessageSubscriberEntryArray message_subscribers_ {};
    std::array<IRuntimeSystem*, GameSystems::size> systems_by_pack_index_ {};
    TimingAccumulator phase1_timing_ {};
    TimingAccumulator phase2_timing_ {};
    TimingAccumulator fixed_step_timing_ {};
    std::uint32_t inline_game_message_depth_ {0U};
#ifndef NDEBUG
    std::vector<std::string_view> debug_semantic_game_message_stack_ {};
    std::vector<std::string_view> debug_last_semantic_game_message_stack_ {};
#endif
    std::array<ProfiledSystemState, static_cast<std::size_t>(GS1_RUNTIME_PROFILE_SYSTEM_COUNT)>
        profiled_systems_ {};
    bool boot_initialized_ {false};
};

class RuntimeSemanticGameMessageScope final
{
public:
    RuntimeSemanticGameMessageScope(GameRuntime& runtime, std::string_view message_name) noexcept
        : runtime_(&runtime)
    {
#ifndef NDEBUG
        runtime_->push_debug_semantic_game_message(message_name);
#else
        (void)message_name;
#endif
    }

    ~RuntimeSemanticGameMessageScope()
    {
#ifndef NDEBUG
        if (runtime_ != nullptr)
        {
            runtime_->pop_debug_semantic_game_message();
        }
#endif
    }

    RuntimeSemanticGameMessageScope(const RuntimeSemanticGameMessageScope&) = delete;
    RuntimeSemanticGameMessageScope& operator=(const RuntimeSemanticGameMessageScope&) = delete;

private:
    GameRuntime* runtime_ {nullptr};
};

template <typename Message>
inline Gs1Status GameRuntime::handle_message(const Message& message)
{
    return dispatch_game_message_inline(message);
}

template <typename Message>
inline Gs1Status GameRuntime::dispatch_game_message_inline(const Message& message)
{
    if constexpr (!typed_gameplay_dispatch_traits<Message>::enabled)
    {
        return dispatch_game_message_inline(make_game_message(message));
    }
    else
    {
        struct InlineDepthScope final
        {
            explicit InlineDepthScope(GameRuntime& runtime) noexcept
                : runtime_(&runtime)
            {
                ++runtime_->inline_game_message_depth_;
            }

            ~InlineDepthScope()
            {
                if (runtime_ != nullptr && runtime_->inline_game_message_depth_ > 0U)
                {
                    --runtime_->inline_game_message_depth_;
                }
            }

            GameRuntime* runtime_ {nullptr};
        };

        struct MutationScope final
        {
            MutationScope(StateManager& state_manager, IRuntimeSystem& system)
                : state_manager_(&state_manager)
            {
                state_manager_->push_current_mutating_system(&system);
            }

            ~MutationScope()
            {
                if (state_manager_ != nullptr)
                {
                    state_manager_->pop_current_mutating_system();
                }
            }

            StateManager* state_manager_ {nullptr};
        };

        InlineDepthScope depth_scope {*this};
        RuntimeSemanticGameMessageScope semantic_scope {*this, runtime_debug_type_name<Message>()};
        constexpr std::uint32_t warn_depth = 4U;

        if (inline_game_message_depth_ == warn_depth)
        {
            std::fprintf(
                stderr,
                "Warning: internal typed gameplay message inline dispatch depth reached %u.\n",
                inline_game_message_depth_);
#ifndef NDEBUG
            print_debug_semantic_game_message_stack();
#endif
        }

        assert(
            inline_game_message_depth_ <= warn_depth &&
            "Internal typed gameplay message inline dispatch depth exceeded limit 4.");

        return dispatch_typed_game_message_to_subscribers(message);
    }
}

template <typename Message>
inline Gs1Status GameRuntime::dispatch_typed_game_message_to_subscribers(const Message& message)
{
    static_assert(
        typed_gameplay_dispatch_traits<Message>::enabled,
        "Typed subscriber dispatch requires an enabled typed gameplay dispatch trait.");

    struct MutationScope final
    {
        MutationScope(StateManager& state_manager, IRuntimeSystem& system)
            : state_manager_(&state_manager)
        {
            state_manager_->push_current_mutating_system(&system);
        }

        ~MutationScope()
        {
            if (state_manager_ != nullptr)
            {
                state_manager_->pop_current_mutating_system();
            }
        }

        StateManager* state_manager_ {nullptr};
    };

    RuntimeInvocation invocation {*this};
    using subscriber_list = typename typed_gameplay_dispatch_traits<Message>::subscribers;
    Gs1Status status = GS1_STATUS_OK;

    for_each_type<subscriber_list>(
        [&]<typename System>()
        {
            if (status != GS1_STATUS_OK)
            {
                return;
            }

            auto* system = find_system<System>();
            if (system == nullptr)
            {
                status = GS1_STATUS_INVALID_STATE;
                return;
            }

            MutationScope mutation_scope {state_manager_, *system};
            const auto profile_id = runtime_profile_system_id_for<System>(*system);
            if (!profile_id.has_value() || !profiled_system_enabled(*profile_id))
            {
                status = system->handle(invocation, message);
                return;
            }

            const auto started_at = std::chrono::steady_clock::now();
            status = system->handle(invocation, message);
            const double elapsed_ms =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - started_at)
                    .count();
            auto& accumulator =
                profiled_systems_[static_cast<std::size_t>(*profile_id)].message_timing;
            accumulator.sample_count += 1U;
            accumulator.last_elapsed_ms = elapsed_ms;
            accumulator.total_elapsed_ms += elapsed_ms;
            if (elapsed_ms > accumulator.max_elapsed_ms)
            {
                accumulator.max_elapsed_ms = elapsed_ms;
            }
        });

    return status;
}

template <typename System>
inline System* GameRuntime::find_system() noexcept
{
    constexpr std::size_t system_index = system_pack_index_v<System, GameSystems>;
    static_assert(system_index < GameSystems::size, "Requested system is not part of GameSystems.");
    return static_cast<System*>(systems_by_pack_index_[system_index]);
}

template <typename Message>
inline void RuntimeInvocation::emit_game_message(const Message& message)
{
    if (runtime_ != nullptr)
    {
        const auto status = runtime_->dispatch_game_message_inline(message);
        assert(status == GS1_STATUS_OK);
        (void)status;
        return;
    }

    push_game_message(make_game_message(message));
}
}  // namespace gs1

