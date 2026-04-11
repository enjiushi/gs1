#pragma once

#include "site/systems/site_system_context.h"

namespace gs1
{
class LocalWeatherResolveSystem final
{
public:
    static void run(SiteSystemContext& context);
};
}  // namespace gs1
