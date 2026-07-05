# Shared Framework Process

This document defines the standard process for extracting, maintaining, and reusing the shared framework across GS-series game projects.

## Goals

- Keep reusable engine-agnostic runtime infrastructure in a shared upstream repository.
- Keep reusable Godot-side adapter/bootstrap infrastructure in the same shared upstream repository under `engines/godot/`.
- Keep game-specific runtime state, content, ABI payloads, and adapter semantics in each game repo.
- Let each game repo vendor a copy of the shared framework instead of depending on a submodule.
- Allow each game repo to choose and pin its own `godot-cpp` version.
- Promote reusable improvements back into the shared framework in small stable batches during development instead of waiting until a game is finished.

## Shared Framework Repository Layout

The shared framework upstream repo should use this shape:

```text
shared_framework/
  core/
    runtime/
  engines/
    godot/
```

### `core/runtime/`

Reusable engine-agnostic runtime framework:

- runtime loop and fixed-step execution
- system registration and lifecycle scaffolding
- global/runtime state ownership framework
- state-set claim and resolver registration
- internal message routing infrastructure
- runtime ABI foundation that is not tied to one game's content, engine, or adapter

This layer must not depend on Godot headers or any game-owned headers.

### `engines/godot/`

Reusable Godot-native framework:

- GDExtension bootstrap helpers
- Godot-side runtime DLL bootstrap/config loading helpers
- reusable scene/director/controller scaffolding
- optional Godot-only debug tools such as the loopback HTTP server
- other adapter helpers that are reusable across multiple games but still intrinsically Godot-specific

This layer may depend on Godot headers and `godot-cpp`, but it must not depend on `gs1` or `gs2` headers.

## Dependency Policy

The shared framework repo owns framework code only. It should not own the concrete `godot-cpp` dependency by default.

### Rules

- `core/runtime/` must remain engine-agnostic.
- `engines/godot/` may require `godot-cpp` and Godot headers.
- Shared framework code must not include `gs1`, `gs2`, or any other game-owned headers.
- If a helper is only reusable inside Godot, keep it under `engines/godot/` rather than forcing it into `core/`.
- If a helper is not actually reusable across games, keep it in the owning game repo even if it looks infrastructural.
- Each game repo is responsible for choosing and pinning its own compatible `godot-cpp` version.

### Recommended `godot-cpp` Layout Per Game Repo

Each game repo should provide its own dependency path, for example:

```text
gs1/
  third_party/
    godot-cpp/

gs2/
  third_party/
    godot-cpp/
```

This avoids making the shared framework repo own duplicated Godot headers while still allowing different games to move at different engine versions when necessary.

## New Godot Project Workflow

When starting a new Godot-based game project:

1. Create the new game repo.
2. Copy a snapshot of the shared framework upstream repo into the new repo under `shared_framework/`.
3. Record the upstream source revision in a small tracking file such as `shared_framework/UPSTREAM_VERSION.md`.
4. Add the game's own `third_party/godot-cpp/` dependency or configure an equivalent external dependency path.
5. Wire the game build so:
   - the gameplay/runtime DLL uses `shared_framework/core/runtime/`
   - the Godot adapter/plugin uses `shared_framework/engines/godot/`
6. Keep any non-Godot host/bootstrap code in the game repo unless it becomes truly cross-game and engine-agnostic.
7. Add the game's own systems, state, content contracts, state-view shaping, and game-specific Godot scenes/controllers on top of the framework.
8. Keep reusable framework changes local only until they are proven stable.
9. Promote stable reusable improvements back into the shared framework upstream repo in small focused commits.

## Vendored Copy Policy

Games use a copied snapshot of the shared framework, not a submodule.

### Reasoning

- each game can ship independently
- each game can make local framework adjustments safely
- framework updates can be promoted upstream intentionally instead of forcing immediate synchronization

### Upstreaming Rule

Do not wait until a game is complete before upstreaming reusable framework improvements. Prefer:

1. stabilize the change inside the game repo
2. extract the reusable part cleanly
3. commit it to the shared framework upstream repo
4. carry that improved snapshot into future projects

This keeps divergence manageable and avoids large end-of-project framework merges.

## Extraction Order For Existing Projects

When applying this process to an existing game such as `gs1`, use this order:

1. Write and keep this process document current.
2. Create the shared framework upstream repo with the target folder layout.
3. Extract the framework-only code into:
   - `core/runtime/`
   - `engines/godot/`
4. Remove anything from the shared framework that still depends on a game-owned header, symbol family, project path, or DLL name.
5. Keep game-specific systems, content rules, state views, non-reusable host bootstrap, and game-specific Godot controllers in `gs1`.
6. Vendor the resulting shared framework snapshot back into `gs1`.
7. Verify `gs1` still builds and runs against the vendored framework.
8. Create `gs2` from a fresh copy of the shared framework plus its own game-specific code.

## Extraction Guidance

### Extract First

- runtime loop and execution shell
- state claim / resolver / ownership framework
- generic runtime ABI and transport foundations
- Godot extension bootstrap and generic adapter scaffolding
- shared Godot-side config/bootstrap/debug helpers
- stable object reconciliation and scene-director patterns

### Leave Game-Specific

- game rules and domain systems
- content tables and validation specific to one game
- game-specific runtime messages and action enums
- game-specific state-view shaping
- game-specific Godot scene layout and panel/controller logic
- non-reusable host bootstrap or project-path conventions

The goal is to share real framework scaffolding without dragging game-owned policy into the shared repo.

## Future Engine Support

Engine-specific framework code should stay under `engines/`.

Example future shape:

```text
shared_framework/
  core/
    runtime/
  engines/
    godot/
    ue/
```

Common code should be pushed down into `core/` only when it no longer requires engine headers, engine-owned types, or game-owned policy.
