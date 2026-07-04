# shared_framework/

Vendored snapshot of the shared upstream runtime/host/engine framework used across GS-series projects.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- Keep this folder as a copied snapshot of the upstream `shared_framework` repo rather than treating it as an independently designed local framework tree.
- When refreshing the vendored snapshot, update `UPSTREAM_VERSION.md` in the same change.

## Contents
- `UPSTREAM_VERSION.md`: Records the upstream repository revision that this vendored snapshot was copied from.
- `README.md`: Upstream framework overview and dependency rules.
- `core/`: Engine-free framework modules split into `runtime/` and `host/`.
- `engines/`: Engine-specific framework modules, currently including Godot scaffolding notes.
