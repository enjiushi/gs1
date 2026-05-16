# System Test Assets

Place asset-driven system tests here with the `.gs1systemtest` extension.

Each file begins with metadata:

```text
system = inventory
name = use_item_hydration
runner = inventory_runtime
---
```

The optional payload after `---` is owned by the named runner.

Current asset-driven regression packs include:

- `inventory/`: item-use and transfer regression cases
- `economy_phone/`: listing purchase and insufficient-funds regression cases
- `task_board/`: board reset and task completion regression cases
- `placement_validation/`: reservation acceptance and rejection boundary cases
- `ecology/`: watering, burial, and passive growth boundary cases
