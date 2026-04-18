# tests/system/source/

Source-authored system tests and supporting fixtures/runner registration code.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `README.md`: Notes on how source-authored system tests in this folder are discovered.
- `action_ecology_placement_system_tests.cpp`: Cross-system regression tests for actions, ecology, and placement.
- `asset_regression_runners.cpp`: Registers and implements runtime runners for asset-authored system tests.
- `campaign_progress_system_tests.cpp`: System tests for campaign progression behavior.
- `craft_system_tests.cpp`: System tests covering crafting behavior.
- `device_interaction_system_tests.cpp`: System tests for device interaction flows.
- `environment_worker_device_system_tests.cpp`: System tests across environment, worker, and device interactions.
- `inventory_economy_task_system_tests.cpp`: System tests spanning inventory, economy, and task board behavior.
- `system_test_fixtures.h`: Shared fixtures and helpers used across the source-authored system tests.
