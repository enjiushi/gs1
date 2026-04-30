#pragma once

#include "runtime_dll_loader.h"
#include "smoke_engine_host.h"

#include <cstdint>

struct VisualSmokeSilentOnboardingOptions final
{
    std::uint32_t max_frames {18000U};
    double frame_delta_seconds {0.25};
};

[[nodiscard]] int run_visual_smoke_silent_onboarding(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime,
    SmokeEngineHost::LogMode log_mode,
    const VisualSmokeSilentOnboardingOptions& options);
