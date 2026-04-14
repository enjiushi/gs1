#pragma once

#include "site/systems/site_system_context.h"

namespace gs1
{
class FailureRecoverySystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "FailureRecoverySystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::WorkerNeeds),
            0U};
    }

    static void run(SiteSystemContext<FailureRecoverySystem>& context);
};
}  // namespace gs1
