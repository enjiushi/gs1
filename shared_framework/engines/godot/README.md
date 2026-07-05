# Godot Adapter Framework

Godot-side helpers that are genuinely reusable across games.

Current shared contents:

- `debug_http_server.*`: a small loopback HTTP transport that accepts requests, exposes a health endpoint, and forwards accepted path/body payloads through a callback.
- `adapter_config_loader.*`: generic Godot-side adapter-config directory packing used to pass engine-owned config files through the neutral runtime bootstrap ABI.
- `runtime_bootstrap_dll_loader.*`: generic Godot-side runtime DLL bootstrap loader for the neutral `gs_runtime_*` ABI surface.
- `controller_context.h`: generic ancestor lookup helper for Godot-side controller trees.
- `scene_director_policy.h`: generic scene-switch decision helper for scene-hosting directors.
- `scene_director_framework.*`: reusable scene-hosting and async scene-load framework for game-owned director controls.

The earlier GS1-derived extraction was removed because it still carried GS1-specific gameplay state views, debug command meanings, DLL names, resource paths, and notification/projection contracts.

This folder may depend on platform and Godot-adjacent host concerns, but it should only contain helpers that remain reusable across multiple game repos without depending on one game's public gameplay ABI, debug command schema, or project-specific presentation semantics.
