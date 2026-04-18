# src/testing/

Runtime-side helpers that register, locate, and load system-test assets.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `system_test_asset.h`: System-test asset data structures and load helpers.
- `system_test_asset.cpp`: System-test asset parsing/loading implementation.
- `system_test_registry.h`: Registry declarations for source-authored and asset-driven tests.
- `system_test_registry.cpp`: Registry implementation and test registration wiring.
