# shared_framework/

Vendored snapshot of the shared upstream runtime/engine framework used across GS-series projects.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- Keep this folder as a copied snapshot of the upstream `shared_framework` repo rather than treating it as an independently designed local framework tree.
- When refreshing the vendored snapshot, update `UPSTREAM_VERSION.md` in the same change.

## Contents
- `UPSTREAM_VERSION.md`: Records the upstream repository revision that this vendored snapshot was copied from.
- `README.md`: Upstream framework overview and dependency rules.
- `core/`: Engine-agnostic runtime framework modules only, with the neutral runtime ABI and shared execution/state/message plumbing under `core/runtime/`.
- `engines/`: Engine-specific framework area now limited to genuinely reusable Godot-side helpers such as adapter-config loading, runtime DLL bootstrap, the loopback debug HTTP transport, and the neutral controller/director scaffolding, while game-specific adapters stay in each game repo.
