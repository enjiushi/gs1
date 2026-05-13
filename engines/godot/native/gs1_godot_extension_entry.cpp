#include "gs1_godot_craft_panel_controller.h"
#include "gs1_godot_director_control.h"
#include "gs1_godot_inventory_panel_controller.h"
#include "gs1_godot_main_menu_ui_controller.h"
#include "gs1_godot_overlay_panel_controller.h"
#include "gs1_godot_phone_panel_controller.h"
#include "gs1_godot_regional_map_hud_controller.h"
#include "gs1_godot_regional_map_scene_controller.h"
#include "gs1_godot_regional_map_ui_controller.h"
#include "gs1_godot_regional_selection_panel_controller.h"
#include "gs1_godot_regional_summary_panel_controller.h"
#include "gs1_godot_regional_tech_tree_panel_controller.h"
#include "gs1_godot_adapter_service.h"
#include "gs1_godot_site_hud_controller.h"
#include "gs1_godot_site_scene_controller.h"
#include "gs1_godot_site_session_ui_controller.h"
#include "gs1_godot_site_summary_panel_controller.h"
#include "gs1_godot_status_panel_controller.h"
#include "gs1_godot_task_panel_controller.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

namespace
{
void initialize_gs1_godot_adapter(ModuleInitializationLevel level)
{
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        return;
    }

    GDREGISTER_CLASS(Gs1GodotCraftPanelController);
    GDREGISTER_CLASS(Gs1GodotInventoryPanelController);
    GDREGISTER_CLASS(Gs1GodotMainMenuUiController);
    GDREGISTER_CLASS(Gs1GodotOverlayPanelController);
    GDREGISTER_CLASS(Gs1GodotPhonePanelController);
    GDREGISTER_CLASS(Gs1GodotRegionalMapHudController);
    GDREGISTER_CLASS(Gs1GodotRegionalMapSceneController);
    GDREGISTER_CLASS(Gs1GodotRegionalMapUiController);
    GDREGISTER_CLASS(Gs1GodotRegionalSelectionPanelController);
    GDREGISTER_CLASS(Gs1GodotRegionalSummaryPanelController);
    GDREGISTER_CLASS(Gs1GodotRegionalTechTreePanelController);
    GDREGISTER_CLASS(Gs1GodotSiteHudController);
    GDREGISTER_CLASS(Gs1GodotSiteSceneController);
    GDREGISTER_CLASS(Gs1GodotSiteSessionUiController);
    GDREGISTER_CLASS(Gs1GodotSiteSummaryPanelController);
    GDREGISTER_CLASS(Gs1GodotStatusPanelController);
    GDREGISTER_CLASS(Gs1GodotTaskPanelController);
    GDREGISTER_CLASS(Gs1GodotDirectorControl);
}

void uninitialize_gs1_godot_adapter(ModuleInitializationLevel level)
{
    (void)level;
}
}

#if defined(_WIN32) || defined(__CYGWIN__)
#define GS1_GODOT_ADAPTER_API __declspec(dllexport)
#else
#define GS1_GODOT_ADAPTER_API __attribute__((visibility("default")))
#endif

extern "C" GS1_GODOT_ADAPTER_API GDExtensionBool gs1_godot_adapter_init(
    GDExtensionInterfaceGetProcAddress get_proc_address,
    GDExtensionClassLibraryPtr library,
    GDExtensionInitialization* out_initialization)
{
    GDExtensionBinding::InitObject init_object(get_proc_address, library, out_initialization);
    init_object.register_initializer(initialize_gs1_godot_adapter);
    init_object.register_terminator(uninitialize_gs1_godot_adapter);
    init_object.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_object.init();
}

#undef GS1_GODOT_ADAPTER_API
