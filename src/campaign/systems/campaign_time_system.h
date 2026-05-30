#pragma once

#include "runtime/system_interface.h"
#include "runtime/runtime_clock.h"

namespace gs1
{
class CampaignTimeSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_TIME;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 0U;

    static constexpr std::array<StateSetId, 1> k_owned_state_sets {
        StateSetId::CampaignTime};
    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return k_owned_state_sets;
    }

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    void run(RuntimeInvocation& invocation) override;
};
}  // namespace gs1
