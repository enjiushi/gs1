# Shared Framework Upstream Version

- Upstream repository: `git@github.com:enjiushi/shared_framework.git`
- Vendored commit: `7457a87503abc5fd5c98ca7a02d0ffcda573407d`
- Snapshot notes:
  - Clean runtime layout now uses `foundation/`, `states/`, `messages/`, and `systems/`.
  - The flat compatibility include layer was intentionally removed from upstream.
  - Shared host bridge now lives under `core/host/` and is the source for the vendored `gs1_host_bridge` build.
  - First-pass Godot framework files now live under `engines/godot/`, with remaining GS1-prefixed surface types scheduled for later neutralization.
