# Godot Adapter Framework

Reusable Godot-native adapter framework code.

## Dependency Policy

- This folder may depend on Godot headers and `godot-cpp`.
- The shared framework repo does not vendor `godot-cpp` by default.
- Each game repo should provide its own `third_party/godot-cpp/` or an equivalent configured dependency path.

## Expected Integration

A game repo should vendor this framework under:

```text
shared_framework/
  engines/
    godot/
```

Then the game build should point the Godot adapter target at the game's own `godot-cpp` checkout.

## Current First-Pass Contents

- `include/shared_framework/godot/adapter_service.h` and `src/adapter_service.cpp`: shared Godot-side runtime-session service extracted from GS1.
- `include/shared_framework/godot/debug_http_protocol.h` and `src/debug_http_protocol.cpp`: optional semantic debug HTTP command parser.
- `include/shared_framework/godot/debug_http_server.h` and `src/debug_http_server.cpp`: optional Windows loopback debug HTTP server.
- `include/shared_framework/godot/notification_types.h`: shared Godot adapter notification payload/types contract.
- `include/shared_framework/godot/notification_policy.h`: shared notification subscriber policy helper.
- `include/shared_framework/godot/projection_types.h`: shared Godot-side projection helper structs.
- `include/shared_framework/godot/director_scene_policy.h`: shared scene-switch policy helper.

## Neutralization Status

This first extraction pass moves the files and shared include paths into the framework repo, but some type names still carry GS1-era prefixes.

The intended follow-up is:

- rename the remaining `Gs1Godot...` surface types to framework-neutral names
- separate stable framework contracts from game-specific controller logic
- keep the consuming game responsible for providing compatible public gameplay ABI headers and its own `godot-cpp` dependency
