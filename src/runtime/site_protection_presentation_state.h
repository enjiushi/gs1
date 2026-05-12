#pragma once

#include "gs1/types.h"

namespace gs1
{
struct SiteProtectionPresentationState final
{
    bool selector_open {false};
    Gs1SiteProtectionOverlayMode overlay_mode {GS1_SITE_PROTECTION_OVERLAY_NONE};
};
}  // namespace gs1
