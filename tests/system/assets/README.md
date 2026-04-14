# System Test Assets

Place asset-driven system tests here with the `.gs1systemtest` extension.

See the full framework manual here:

- [SYSTEM_TEST_MANUAL.md](E:/gs1/SYSTEM_TEST_MANUAL.md)

Each file begins with metadata:

```text
system = inventory
name = use_item_hydration
runner = inventory_runtime
---
```

The optional payload after `---` is owned by the named runner.
