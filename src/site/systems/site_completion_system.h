#pragma once

#include "site/systems/site_system_context.h"

namespace gs1
{
class SiteCompletionSystem final
{
public:
    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "SiteCompletionSystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::Counters),
            0U};
    }

    static void run(SiteSystemContext<SiteCompletionSystem>& context);
};
}  // namespace gs1
