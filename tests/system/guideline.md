# tests/system/

Standalone system-test framework that mixes source-authored tests and asset-authored regression cases.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `README.md`: Overview of the system-test framework layout and entry points.
- `assets/`: Asset-authored `.gs1systemtest` regression packs discovered at runtime.
- `source/`: Source-authored system tests compiled directly into the standalone host, including harvest-action and harvested-output regressions plus faction-tech base/enhancement progression coverage.
- `system_test_host_main.cpp`: Standalone system-test host entry point that discovers and runs linked source tests plus asset-driven regressions without routing through the gameplay DLL.
