#pragma once

namespace godot
{
class Node;
}

class Gs1GodotAdapterService;
class Gs1GodotDirectorControl;

Gs1GodotDirectorControl* gs1_find_director_control(godot::Node* start);
Gs1GodotAdapterService* gs1_resolve_adapter_service(godot::Node* start);
