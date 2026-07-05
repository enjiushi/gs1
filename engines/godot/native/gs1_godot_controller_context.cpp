#include "gs1_godot_controller_context.h"

#include "gs1_godot_director_control.h"
#include "shared_framework/godot/controller_context.h"

using namespace godot;

Gs1GodotDirectorControl* gs1_find_director_control(Node* start)
{
    return shared_framework::godot::find_ancestor_of_type<Gs1GodotDirectorControl>(start);
}

Gs1GodotAdapterService* gs1_resolve_adapter_service(Node* start)
{
    if (Gs1GodotDirectorControl* director = gs1_find_director_control(start))
    {
        return &director->adapter_service();
    }
    return nullptr;
}
