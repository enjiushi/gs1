#include "gs1_godot_controller_context.h"

#include "gs1_godot_director_control.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/object.hpp>

using namespace godot;

Gs1GodotDirectorControl* gs1_find_director_control(Node* start)
{
    for (Node* node = start; node != nullptr; node = node->get_parent())
    {
        if (auto* director = Object::cast_to<Gs1GodotDirectorControl>(node))
        {
            return director;
        }
    }
    return nullptr;
}

Gs1GodotAdapterService* gs1_resolve_adapter_service(Node* start)
{
    if (Gs1GodotDirectorControl* director = gs1_find_director_control(start))
    {
        return &director->adapter_service();
    }
    return nullptr;
}
