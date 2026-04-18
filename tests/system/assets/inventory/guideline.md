# tests/system/assets/inventory/

Asset-driven inventory regressions for use-item and transfer stack behavior.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `invalid_item_use_rejected.gs1systemtest`: Verifies invalid item use requests are rejected.
- `transfer_merge_stack.gs1systemtest`: Verifies compatible transferred stacks merge correctly.
- `use_item_hydration.gs1systemtest`: Verifies the hydration item-use path updates the expected state.
