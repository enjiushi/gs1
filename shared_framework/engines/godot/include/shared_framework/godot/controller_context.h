#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/object.hpp>

namespace shared_framework::godot {
template <typename T>
T* find_ancestor_of_type(::godot::Node* start)
{
    for (::godot::Node* node = start; node != nullptr; node = node->get_parent())
    {
        if (auto* typed = ::godot::Object::cast_to<T>(node))
        {
            return typed;
        }
    }

    return nullptr;
}
}  // namespace shared_framework::godot
