# third_party/

Vendored external code kept in-repo for builds and tests.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `flecs/`: Vendored Flecs ECS library sources and license metadata.
- `godot-cpp/`: Vendored `godot-cpp` bindings checkout used by the Godot runtime-extension build to register higher-level native Godot classes against the local engine API dump.
- `tomlplusplus/`: Vendored `toml++` single-header parser and license files used by the TOML-backed content loader.
