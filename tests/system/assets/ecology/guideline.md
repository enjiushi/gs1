# tests/system/assets/ecology/

Asset-driven ecology regressions focused on density changes and passive growth boundaries.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `clear_burial_clamps_to_zero.gs1systemtest`: Verifies burial clearing does not underflow below zero.
- `run_growth_updates_progress.gs1systemtest`: Verifies ecology growth advancement updates progress as expected.
- `water_clamps_density_to_max.gs1systemtest`: Verifies watering cannot push density above its maximum.
- `water_without_occupant_no_change.gs1systemtest`: Verifies watering an empty target does not mutate ecology state.
