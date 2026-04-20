# tests/system/source/

Source-authored system tests and supporting fixtures/runner registration code.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `README.md`: Notes on how source-authored system tests in this folder are discovered.
- `action_ecology_placement_system_tests.cpp`: Cross-system regression tests for actions, ecology, placement, multi-tile plant wind-exposure behavior, and highway-target sand-cover handling.
- `asset_regression_runners.cpp`: Registers and implements runtime runners for asset-authored system tests.
- `campaign_progress_system_tests.cpp`: System tests for campaign progression behavior, including selected-site support assembly, pure-survival and green-wall objective-mode completion/failure messaging, faction-reputation rewards, occupied-reputation tech claims, tier unlock rules, amplification lockout, and regional-map tech-tree tab/open flow.
- `craft_system_tests.cpp`: System tests covering crafting behavior.
- `device_interaction_system_tests.cpp`: System tests for device interaction flows.
- `environment_worker_device_system_tests.cpp`: System tests across environment, worker, and device interactions, including directional wind-shadow coverage for plants, windbreak devices, one-sided highway-protection weather waves, and modifier import from unlocked assistants plus purchased technology.
- `inventory_economy_task_system_tests.cpp`: System tests spanning inventory, economy, task board behavior, and phone sell/cart regressions.
- `phone_panel_system_tests.cpp`: System tests for authoritative phone panel snapshot state, section switching, and sell-list refresh after delivery arrivals.
- `system_test_fixtures.h`: Shared campaign/site fixtures and helpers, including default worker-pack setup, used across the source-authored system tests.
