#include "gs1_godot_main_screen_control.h"
#include "gs1_godot_runtime_node.h"
#include "gs1_godot_scene_router_control.h"
#include "gs1_godot_site_view_node.h"

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

    GDREGISTER_CLASS(Gs1RuntimeNode);
    GDREGISTER_CLASS(Gs1SiteViewNode);
    GDREGISTER_CLASS(Gs1GodotMainScreenControl);
    GDREGISTER_CLASS(Gs1GodotSceneRouterControl);
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
