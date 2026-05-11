#pragma once

#include "messages/game_message.h"
#include "gs1/status.h"
#include "gs1/types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace gs1
{
class GameRuntime;
struct SiteMoveDirectionInput;

class GameRuntimeTempBridge final
{
public:
    explicit GameRuntimeTempBridge(GameRuntime& runtime) noexcept
        : runtime_(runtime)
    {
    }

    [[nodiscard]] SiteMoveDirectionInput current_move_direction() const noexcept;

    template <typename Fn>
    [[nodiscard]] Gs1Status with_campaign_flow_message_context(Fn&& fn);

    template <typename Fn>
    [[nodiscard]] Gs1Status with_campaign_context(Fn&& fn);

    template <typename Fn>
    void with_campaign_fixed_step_context(Fn&& fn);

    template <typename SystemTag, typename Fn>
    [[nodiscard]] Gs1Status with_site_context(
        Fn&& fn,
        SiteMoveDirectionInput move_direction = {},
        bool missing_context_is_ok = false);

    template <typename Fn>
    [[nodiscard]] Gs1Status with_presentation_context(Fn&& fn);

private:
    GameRuntime& runtime_;
};

inline constexpr std::size_t k_runtime_host_message_type_count =
    static_cast<std::size_t>(GS1_HOST_EVENT_SITE_SCENE_READY) + 1U;
inline constexpr std::size_t k_runtime_feedback_event_type_count = 4U;

class IRuntimeSystem
{
public:
    virtual ~IRuntimeSystem() = default;

    [[nodiscard]] virtual const char* name() const noexcept = 0;
    virtual void register_game_message_subscriptions(class RuntimeSystemRegistration& registration) = 0;
    virtual void register_host_message_subscriptions(class RuntimeSystemRegistration& registration) = 0;
    virtual void register_feedback_event_subscriptions(class RuntimeSystemRegistration& registration) = 0;
    [[nodiscard]] virtual std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::uint32_t> fixed_step_order() const noexcept = 0;
    [[nodiscard]] virtual Gs1Status process_game_message(
        GameRuntimeTempBridge& bridge,
        const GameMessage& message) = 0;
    [[nodiscard]] virtual Gs1Status process_host_message(
        GameRuntimeTempBridge& bridge,
        const Gs1HostMessage& message) = 0;
    virtual void run(GameRuntimeTempBridge& bridge) = 0;
};

class RuntimeSystemRegistration final
{
public:
    using GameMessageSubscribers = std::array<std::vector<IRuntimeSystem*>, k_game_message_type_count>;
    using HostMessageSubscribers = std::array<std::vector<IRuntimeSystem*>, k_runtime_host_message_type_count>;
    using FeedbackEventSubscribers =
        std::array<std::vector<IRuntimeSystem*>, k_runtime_feedback_event_type_count>;

    RuntimeSystemRegistration(
        GameMessageSubscribers& game_message_subscribers,
        HostMessageSubscribers& host_message_subscribers,
        FeedbackEventSubscribers& feedback_event_subscribers) noexcept
        : game_message_subscribers_(game_message_subscribers),
          host_message_subscribers_(host_message_subscribers),
          feedback_event_subscribers_(feedback_event_subscribers)
    {
    }

    void subscribe_to(GameMessageType type, IRuntimeSystem& system)
    {
        const auto index = static_cast<std::size_t>(type);
        if (index < game_message_subscribers_.size())
        {
            game_message_subscribers_[index].push_back(&system);
        }
    }

    void subscribe_to_host_message(Gs1HostMessageType type, IRuntimeSystem& system)
    {
        const auto index = static_cast<std::size_t>(type);
        if (index < host_message_subscribers_.size())
        {
            host_message_subscribers_[index].push_back(&system);
        }
    }

    void subscribe_to_feedback_event(Gs1FeedbackEventType type, IRuntimeSystem& system)
    {
        const auto index = static_cast<std::size_t>(type);
        if (index < feedback_event_subscribers_.size())
        {
            feedback_event_subscribers_[index].push_back(&system);
        }
    }

private:
    GameMessageSubscribers& game_message_subscribers_;
    HostMessageSubscribers& host_message_subscribers_;
    FeedbackEventSubscribers& feedback_event_subscribers_;
};
}  // namespace gs1

#define GS1_IMPLEMENT_RUNTIME_CAMPAIGN_FLOW_SYSTEM(ClassName)                                                     \
    const char* ClassName::name() const noexcept                                                                  \
    {                                                                                                             \
        return #ClassName;                                                                                        \
    }                                                                                                             \
    void ClassName::register_game_message_subscriptions(gs1::RuntimeSystemRegistration& registration)             \
    {                                                                                                             \
        for (std::size_t index = 0; index < gs1::k_game_message_type_count; ++index)                             \
        {                                                                                                         \
            const auto type = static_cast<gs1::GameMessageType>(index);                                           \
            if (ClassName::subscribes_to(type))                                                                   \
            {                                                                                                     \
                registration.subscribe_to(type, *this);                                                           \
            }                                                                                                     \
        }                                                                                                         \
    }                                                                                                             \
    void ClassName::register_host_message_subscriptions(gs1::RuntimeSystemRegistration& registration)            \
    {                                                                                                             \
        for (std::size_t index = 0; index < gs1::k_runtime_host_message_type_count; ++index)                     \
        {                                                                                                         \
            const auto type = static_cast<Gs1HostMessageType>(index);                                             \
            if (ClassName::subscribes_to_host_message(type))                                                      \
            {                                                                                                     \
                registration.subscribe_to_host_message(type, *this);                                              \
            }                                                                                                     \
        }                                                                                                         \
    }                                                                                                             \
    void ClassName::register_feedback_event_subscriptions(gs1::RuntimeSystemRegistration&)                       \
    {                                                                                                             \
    }                                                                                                             \
    std::optional<Gs1RuntimeProfileSystemId> ClassName::profile_system_id() const noexcept                       \
    {                                                                                                             \
        return GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_FLOW;                                                          \
    }                                                                                                             \
    std::optional<std::uint32_t> ClassName::fixed_step_order() const noexcept                                    \
    {                                                                                                             \
        return std::nullopt;                                                                                      \
    }                                                                                                             \
    Gs1Status ClassName::process_game_message(gs1::GameRuntimeTempBridge& bridge, const gs1::GameMessage& message) \
    {                                                                                                             \
        return bridge.with_campaign_flow_message_context(                                                         \
            [&](gs1::CampaignFlowMessageContext& context) -> Gs1Status                                            \
            {                                                                                                     \
                return ClassName::process_message(context, message);                                              \
            });                                                                                                   \
    }                                                                                                             \
    Gs1Status ClassName::process_host_message(gs1::GameRuntimeTempBridge& bridge, const Gs1HostMessage& message) \
    {                                                                                                             \
        return bridge.with_campaign_flow_message_context(                                                         \
            [&](gs1::CampaignFlowMessageContext& context) -> Gs1Status                                            \
            {                                                                                                     \
                return ClassName::process_host_message(context, message);                                         \
            });                                                                                                   \
    }                                                                                                             \
    void ClassName::run(gs1::GameRuntimeTempBridge&)                                                              \
    {                                                                                                             \
    }

#define GS1_IMPLEMENT_RUNTIME_CAMPAIGN_MESSAGE_SYSTEM(ClassName, ProfileId)                                       \
    const char* ClassName::name() const noexcept                                                                  \
    {                                                                                                             \
        return #ClassName;                                                                                        \
    }                                                                                                             \
    void ClassName::register_game_message_subscriptions(gs1::RuntimeSystemRegistration& registration)             \
    {                                                                                                             \
        for (std::size_t index = 0; index < gs1::k_game_message_type_count; ++index)                             \
        {                                                                                                         \
            const auto type = static_cast<gs1::GameMessageType>(index);                                           \
            if (ClassName::subscribes_to(type))                                                                   \
            {                                                                                                     \
                registration.subscribe_to(type, *this);                                                           \
            }                                                                                                     \
        }                                                                                                         \
    }                                                                                                             \
    void ClassName::register_host_message_subscriptions(gs1::RuntimeSystemRegistration&)                         \
    {                                                                                                             \
    }                                                                                                             \
    void ClassName::register_feedback_event_subscriptions(gs1::RuntimeSystemRegistration&)                       \
    {                                                                                                             \
    }                                                                                                             \
    std::optional<Gs1RuntimeProfileSystemId> ClassName::profile_system_id() const noexcept                       \
    {                                                                                                             \
        return ProfileId;                                                                                         \
    }                                                                                                             \
    std::optional<std::uint32_t> ClassName::fixed_step_order() const noexcept                                    \
    {                                                                                                             \
        return std::nullopt;                                                                                      \
    }                                                                                                             \
    Gs1Status ClassName::process_game_message(gs1::GameRuntimeTempBridge& bridge, const gs1::GameMessage& message) \
    {                                                                                                             \
        return bridge.with_campaign_context(                                                                      \
            [&](gs1::CampaignSystemContext& context) -> Gs1Status                                                 \
            {                                                                                                     \
                return ClassName::process_message(context, message);                                              \
            });                                                                                                   \
    }                                                                                                             \
    Gs1Status ClassName::process_host_message(gs1::GameRuntimeTempBridge&, const Gs1HostMessage&)                \
    {                                                                                                             \
        return GS1_STATUS_OK;                                                                                     \
    }                                                                                                             \
    void ClassName::run(gs1::GameRuntimeTempBridge&)                                                              \
    {                                                                                                             \
    }

#define GS1_IMPLEMENT_RUNTIME_CAMPAIGN_FIXED_STEP_SYSTEM(ClassName, ProfileId, FixedStepOrderValue, ContextType)  \
    const char* ClassName::name() const noexcept                                                                  \
    {                                                                                                             \
        return #ClassName;                                                                                        \
    }                                                                                                             \
    void ClassName::register_game_message_subscriptions(gs1::RuntimeSystemRegistration&)                         \
    {                                                                                                             \
    }                                                                                                             \
    void ClassName::register_host_message_subscriptions(gs1::RuntimeSystemRegistration&)                         \
    {                                                                                                             \
    }                                                                                                             \
    void ClassName::register_feedback_event_subscriptions(gs1::RuntimeSystemRegistration&)                       \
    {                                                                                                             \
    }                                                                                                             \
    std::optional<Gs1RuntimeProfileSystemId> ClassName::profile_system_id() const noexcept                       \
    {                                                                                                             \
        return ProfileId;                                                                                         \
    }                                                                                                             \
    std::optional<std::uint32_t> ClassName::fixed_step_order() const noexcept                                    \
    {                                                                                                             \
        return FixedStepOrderValue;                                                                               \
    }                                                                                                             \
    Gs1Status ClassName::process_game_message(gs1::GameRuntimeTempBridge&, const gs1::GameMessage&)              \
    {                                                                                                             \
        return GS1_STATUS_OK;                                                                                     \
    }                                                                                                             \
    Gs1Status ClassName::process_host_message(gs1::GameRuntimeTempBridge&, const Gs1HostMessage&)                \
    {                                                                                                             \
        return GS1_STATUS_OK;                                                                                     \
    }                                                                                                             \
    void ClassName::run(gs1::GameRuntimeTempBridge& bridge)                                                       \
    {                                                                                                             \
        bridge.with_campaign_fixed_step_context(                                                                  \
            [&](ContextType& context)                                                                             \
            {                                                                                                     \
                ClassName::run(context);                                                                          \
            });                                                                                                   \
    }

#define GS1_IMPLEMENT_RUNTIME_SITE_MESSAGE_SYSTEM(ClassName, ProfileId, FixedStepOrderValue)                      \
    const char* ClassName::name() const noexcept                                                                  \
    {                                                                                                             \
        return ClassName::access().system_name.data();                                                            \
    }                                                                                                             \
    void ClassName::register_game_message_subscriptions(gs1::RuntimeSystemRegistration& registration)             \
    {                                                                                                             \
        for (std::size_t index = 0; index < gs1::k_game_message_type_count; ++index)                             \
        {                                                                                                         \
            const auto type = static_cast<gs1::GameMessageType>(index);                                           \
            if (ClassName::subscribes_to(type))                                                                   \
            {                                                                                                     \
                registration.subscribe_to(type, *this);                                                           \
            }                                                                                                     \
        }                                                                                                         \
    }                                                                                                             \
    void ClassName::register_host_message_subscriptions(gs1::RuntimeSystemRegistration&)                         \
    {                                                                                                             \
    }                                                                                                             \
    void ClassName::register_feedback_event_subscriptions(gs1::RuntimeSystemRegistration&)                       \
    {                                                                                                             \
    }                                                                                                             \
    std::optional<Gs1RuntimeProfileSystemId> ClassName::profile_system_id() const noexcept                       \
    {                                                                                                             \
        return ProfileId;                                                                                         \
    }                                                                                                             \
    std::optional<std::uint32_t> ClassName::fixed_step_order() const noexcept                                    \
    {                                                                                                             \
        return FixedStepOrderValue;                                                                               \
    }                                                                                                             \
    Gs1Status ClassName::process_game_message(gs1::GameRuntimeTempBridge& bridge, const gs1::GameMessage& message) \
    {                                                                                                             \
        return bridge.with_site_context<ClassName>(                                                               \
            [&](gs1::SiteSystemContext<ClassName>& context) -> Gs1Status                                          \
            {                                                                                                     \
                return ClassName::process_message(context, message);                                              \
            });                                                                                                   \
    }                                                                                                             \
    Gs1Status ClassName::process_host_message(gs1::GameRuntimeTempBridge&, const Gs1HostMessage&)                \
    {                                                                                                             \
        return GS1_STATUS_OK;                                                                                     \
    }                                                                                                             \
    void ClassName::run(gs1::GameRuntimeTempBridge& bridge)                                                       \
    {                                                                                                             \
        (void)bridge.with_site_context<ClassName>(                                                                \
            [&](gs1::SiteSystemContext<ClassName>& context) -> Gs1Status                                          \
            {                                                                                                     \
                ClassName::run(context);                                                                          \
                return GS1_STATUS_OK;                                                                             \
            },                                                                                                    \
            bridge.current_move_direction());                                                                     \
    }

#define GS1_IMPLEMENT_RUNTIME_SITE_HOST_AND_MESSAGE_SYSTEM(ClassName, ProfileId, FixedStepOrderValue)             \
    const char* ClassName::name() const noexcept                                                                  \
    {                                                                                                             \
        return ClassName::access().system_name.data();                                                            \
    }                                                                                                             \
    void ClassName::register_game_message_subscriptions(gs1::RuntimeSystemRegistration& registration)             \
    {                                                                                                             \
        for (std::size_t index = 0; index < gs1::k_game_message_type_count; ++index)                             \
        {                                                                                                         \
            const auto type = static_cast<gs1::GameMessageType>(index);                                           \
            if (ClassName::subscribes_to(type))                                                                   \
            {                                                                                                     \
                registration.subscribe_to(type, *this);                                                           \
            }                                                                                                     \
        }                                                                                                         \
    }                                                                                                             \
    void ClassName::register_host_message_subscriptions(gs1::RuntimeSystemRegistration& registration)            \
    {                                                                                                             \
        for (std::size_t index = 0; index < gs1::k_runtime_host_message_type_count; ++index)                     \
        {                                                                                                         \
            const auto type = static_cast<Gs1HostMessageType>(index);                                             \
            if (ClassName::subscribes_to_host_message(type))                                                      \
            {                                                                                                     \
                registration.subscribe_to_host_message(type, *this);                                              \
            }                                                                                                     \
        }                                                                                                         \
    }                                                                                                             \
    void ClassName::register_feedback_event_subscriptions(gs1::RuntimeSystemRegistration&)                       \
    {                                                                                                             \
    }                                                                                                             \
    std::optional<Gs1RuntimeProfileSystemId> ClassName::profile_system_id() const noexcept                       \
    {                                                                                                             \
        return ProfileId;                                                                                         \
    }                                                                                                             \
    std::optional<std::uint32_t> ClassName::fixed_step_order() const noexcept                                    \
    {                                                                                                             \
        return FixedStepOrderValue;                                                                               \
    }                                                                                                             \
    Gs1Status ClassName::process_game_message(gs1::GameRuntimeTempBridge& bridge, const gs1::GameMessage& message) \
    {                                                                                                             \
        return bridge.with_site_context<ClassName>(                                                               \
            [&](gs1::SiteSystemContext<ClassName>& context) -> Gs1Status                                          \
            {                                                                                                     \
                return ClassName::process_message(context, message);                                              \
            });                                                                                                   \
    }                                                                                                             \
    Gs1Status ClassName::process_host_message(gs1::GameRuntimeTempBridge& bridge, const Gs1HostMessage& message) \
    {                                                                                                             \
        return bridge.with_site_context<ClassName>(                                                               \
            [&](gs1::SiteSystemContext<ClassName>& context) -> Gs1Status                                          \
            {                                                                                                     \
                return ClassName::process_host_message(context, message);                                         \
            },                                                                                                    \
            gs1::SiteMoveDirectionInput {},                                                                       \
            true);                                                                                                \
    }                                                                                                             \
    void ClassName::run(gs1::GameRuntimeTempBridge& bridge)                                                       \
    {                                                                                                             \
        (void)bridge.with_site_context<ClassName>(                                                                \
            [&](gs1::SiteSystemContext<ClassName>& context) -> Gs1Status                                          \
            {                                                                                                     \
                ClassName::run(context);                                                                          \
                return GS1_STATUS_OK;                                                                             \
            },                                                                                                    \
            bridge.current_move_direction());                                                                     \
    }

#define GS1_IMPLEMENT_RUNTIME_SITE_RUN_ONLY_SYSTEM(ClassName, ProfileId, FixedStepOrderValue)                     \
    const char* ClassName::name() const noexcept                                                                  \
    {                                                                                                             \
        return ClassName::access().system_name.data();                                                            \
    }                                                                                                             \
    void ClassName::register_game_message_subscriptions(gs1::RuntimeSystemRegistration&)                         \
    {                                                                                                             \
    }                                                                                                             \
    void ClassName::register_host_message_subscriptions(gs1::RuntimeSystemRegistration&)                         \
    {                                                                                                             \
    }                                                                                                             \
    void ClassName::register_feedback_event_subscriptions(gs1::RuntimeSystemRegistration&)                       \
    {                                                                                                             \
    }                                                                                                             \
    std::optional<Gs1RuntimeProfileSystemId> ClassName::profile_system_id() const noexcept                       \
    {                                                                                                             \
        return ProfileId;                                                                                         \
    }                                                                                                             \
    std::optional<std::uint32_t> ClassName::fixed_step_order() const noexcept                                    \
    {                                                                                                             \
        return FixedStepOrderValue;                                                                               \
    }                                                                                                             \
    Gs1Status ClassName::process_game_message(gs1::GameRuntimeTempBridge&, const gs1::GameMessage&)              \
    {                                                                                                             \
        return GS1_STATUS_OK;                                                                                     \
    }                                                                                                             \
    Gs1Status ClassName::process_host_message(gs1::GameRuntimeTempBridge&, const Gs1HostMessage&)                \
    {                                                                                                             \
        return GS1_STATUS_OK;                                                                                     \
    }                                                                                                             \
    void ClassName::run(gs1::GameRuntimeTempBridge& bridge)                                                       \
    {                                                                                                             \
        (void)bridge.with_site_context<ClassName>(                                                                \
            [&](gs1::SiteSystemContext<ClassName>& context) -> Gs1Status                                          \
            {                                                                                                     \
                ClassName::run(context);                                                                          \
                return GS1_STATUS_OK;                                                                             \
            },                                                                                                    \
            bridge.current_move_direction());                                                                     \
    }

#define GS1_IMPLEMENT_RUNTIME_SITE_HOST_ONLY_RUN_SYSTEM(ClassName, ProfileId, FixedStepOrderValue)                \
    const char* ClassName::name() const noexcept                                                                  \
    {                                                                                                             \
        return ClassName::access().system_name.data();                                                            \
    }                                                                                                             \
    void ClassName::register_game_message_subscriptions(gs1::RuntimeSystemRegistration&)                         \
    {                                                                                                             \
    }                                                                                                             \
    void ClassName::register_host_message_subscriptions(gs1::RuntimeSystemRegistration& registration)            \
    {                                                                                                             \
        for (std::size_t index = 0; index < gs1::k_runtime_host_message_type_count; ++index)                     \
        {                                                                                                         \
            const auto type = static_cast<Gs1HostMessageType>(index);                                             \
            if (ClassName::subscribes_to_host_message(type))                                                      \
            {                                                                                                     \
                registration.subscribe_to_host_message(type, *this);                                              \
            }                                                                                                     \
        }                                                                                                         \
    }                                                                                                             \
    void ClassName::register_feedback_event_subscriptions(gs1::RuntimeSystemRegistration&)                       \
    {                                                                                                             \
    }                                                                                                             \
    std::optional<Gs1RuntimeProfileSystemId> ClassName::profile_system_id() const noexcept                       \
    {                                                                                                             \
        return ProfileId;                                                                                         \
    }                                                                                                             \
    std::optional<std::uint32_t> ClassName::fixed_step_order() const noexcept                                    \
    {                                                                                                             \
        return FixedStepOrderValue;                                                                               \
    }                                                                                                             \
    Gs1Status ClassName::process_game_message(gs1::GameRuntimeTempBridge&, const gs1::GameMessage&)              \
    {                                                                                                             \
        return GS1_STATUS_OK;                                                                                     \
    }                                                                                                             \
    Gs1Status ClassName::process_host_message(gs1::GameRuntimeTempBridge& bridge, const Gs1HostMessage& message) \
    {                                                                                                             \
        return bridge.with_site_context<ClassName>(                                                               \
            [&](gs1::SiteSystemContext<ClassName>& context) -> Gs1Status                                          \
            {                                                                                                     \
                return ClassName::process_host_message(context, message);                                         \
            },                                                                                                    \
            gs1::SiteMoveDirectionInput {},                                                                       \
            true);                                                                                                \
    }                                                                                                             \
    void ClassName::run(gs1::GameRuntimeTempBridge& bridge)                                                       \
    {                                                                                                             \
        (void)bridge.with_site_context<ClassName>(                                                                \
            [&](gs1::SiteSystemContext<ClassName>& context) -> Gs1Status                                          \
            {                                                                                                     \
                ClassName::run(context);                                                                          \
                return GS1_STATUS_OK;                                                                             \
            },                                                                                                    \
            bridge.current_move_direction());                                                                     \
    }
