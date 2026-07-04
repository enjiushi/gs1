# Shared Framework

Reusable framework code shared across GS-series game projects.

## Layout

```text
core/
  runtime/
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
