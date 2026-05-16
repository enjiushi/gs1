# GS1 System Test Framework

This folder contains the standalone system-test host plus the source-authored and asset-authored test suites used by the runtime/system harness.

This folder contains:

- `system_test_host_main.cpp`: the standalone DLL-loading system-test host
- `source/`: source-authored test files compiled into the DLL when tests are enabled
- `assets/`: asset-authored `.gs1systemtest` files discovered at runtime
