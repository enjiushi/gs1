# tests/system/assets/placement_validation/

Asset-driven placement validation regressions for boundary and reservation checks.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `out_of_bounds_rejected.gs1systemtest`: Verifies out-of-bounds placement requests are rejected.
- `reserved_tile_rejected.gs1systemtest`: Verifies placement on reserved tiles is rejected.
- `structure_tile_blocked_rejected.gs1systemtest`: Verifies blocked/occupied structure tiles reject placement.
