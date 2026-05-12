#pragma once

#include "gs1/types.h"

#include <cstdint>
#include <map>
#include <optional>
#include <unordered_set>

namespace gs1
{
struct UiPresentationState final
{
    std::map<Gs1UiSetupId, Gs1UiSetupPresentationType> active_ui_setups {};
    std::unordered_set<Gs1UiPanelId> active_ui_panels {};
    std::optional<Gs1UiSetupId> active_normal_ui_setup {};
    std::optional<Gs1AppState> last_emitted_app_state {};
    bool regional_map_tech_tree_view_open {false};
};
}  // namespace gs1
