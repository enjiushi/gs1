#pragma once

#include "messages/game_message.h"
#include "runtime/system_interface.h"
#include "gs1/status.h"

namespace gs1
{
class FactionReputationSystem final : public IRuntimeSystem
{
public:
    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::CampaignFactionProgress};
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    void run(RuntimeInvocation& invocation) override;
};
}  // namespace gs1
