# Shared Framework Upstream Version

- Upstream repository: `git@github.com:enjiushi/shared_framework.git`
- Vendored commit: `2279a060c446bbb46cd3e43c837dacfbb9d35695`
- Snapshot notes:
  - Clean runtime layout now uses `foundation/`, `states/`, `messages/`, and `systems/`.
  - The flat compatibility include layer was intentionally removed from upstream.
  - The neutral runtime ABI still lives at `core/runtime/include/shared_framework/runtime/api/runtime_api.h`.
  - `core/` now contains only engine-agnostic runtime framework code, with `core/runtime/` as the sole shared core module.
  - Shared Godot-side bootstrap/config/debug helpers now live under `engines/godot/`, alongside the neutral controller/director helpers; game-specific command semantics, app-state routing, preload semantics, and any non-Godot host bootstrap remain game-owned.
