# Flecs Upstream

This directory vendors the minimal Flecs distribution needed by `gs1_game`.

- Upstream project: https://github.com/SanderMertens/flecs
- Version: `v4.1.3`
- Commit: `8f63134f738f0676103460610c1345ff185566c0`
- Vendored files:
  - `flecs.h`
  - `flecs.c`
  - `LICENSE`

Integration policy:

- Flecs is an internal implementation dependency.
- Do not expose Flecs types through public headers in `include/gs1`.
- Prefer wrapping Flecs behind project-owned ECS types such as `SiteWorld`.
