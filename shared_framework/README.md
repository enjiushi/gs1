# Shared Framework

Reusable framework code shared across GS-series game projects.

## Layout

```text
core/
  runtime/
    foundation/
    states/
    messages/
    systems/
  host/
engines/
  godot/
```

## Dependency Rules

- `core/runtime/` must remain engine-free.
- `core/host/` must remain engine-free.
- `engines/godot/` may depend on Godot headers and `godot-cpp`.
- The framework repo does not own `godot-cpp` by default.
- Each game repo chooses and pins its own `godot-cpp` dependency version.

## Vendoring Model

Game repos copy a snapshot of this repo into a local `shared_framework/` folder instead of using a submodule.

Each vendored copy should record the upstream source revision in `shared_framework/UPSTREAM_VERSION.md`.

## Runtime Direction

The intended long-term runtime framework shape is:

- compile-time `system_pack` composition
- compile-time message subscription discovery from declared system traits
- compile-time state catalog discovery from declared owned-state descriptors
- compile-time accessed-state validation against the owned-state catalog
- runtime state ownership enforcement layered on top of that catalog

The shared framework should own those mechanisms, while each game repo still defines its own:

- concrete systems
- concrete message payload catalog
- concrete state descriptor catalog

When a reusable gameplay system is promoted into the shared framework, it should live under:

- `core/runtime/systems/`

and depend on the unified shared contract headers under:

- `core/runtime/states/`
- `core/runtime/messages/`
