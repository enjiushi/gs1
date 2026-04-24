# tests/system/source/

Source-authored system tests and supporting fixtures/runner registration code.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `README.md`: Notes on how source-authored system tests in this folder are discovered.
- `action_ecology_placement_system_tests.cpp`: Cross-system regression tests for actions, ecology, placement, harvest-output insertion plus density setbacks, persistent item-based plant-mode rearming, checkerboard `2x2` plant occupancy, previous-frame work-efficiency-driven action timing/cost plus current-local-weather-scaled worker meter costs including recipe-authored craft timing and worker meter costs, multi-tile plant wind-exposure behavior, summed owner-specific weather-contribution-driven terrain updates, and highway-target sand-cover handling.
- `asset_regression_runners.cpp`: Registers and implements runtime runners for asset-authored system tests, including compatibility parsing for legacy normalized-versus-raw ecology meter inputs in regression payloads and the remaining task-board reset regression flow.
- `campaign_progress_system_tests.cpp`: System tests for campaign progression behavior, including dedicated campaign/site time-system coverage for the 30-real-minute in-game day contract, selected-site support assembly, objective-mode completion/failure messaging, non-decreasing faction-reputation rewards, total-reputation plant-tier unlocks, neutral base-tech tier gating, faction enhancement gating, persistent-campaign-cash tech purchases, and tech-tree open/tab flow from both the regional map and active site sessions.
- `craft_system_tests.cpp`: System tests covering crafting behavior, including worker-pack distance cache membership regressions that use test-injected inventory instead of authored startup loadout assumptions.
- `device_interaction_system_tests.cpp`: System tests for device interaction flows.
- `environment_worker_device_system_tests.cpp`: System tests across environment, worker, and device interactions, including prototype Site 1 bootstrap layout for the harvestable starter plant patches near camp, authored hostile site-one background weather, Site 1 starter-plant density setbacks plus density-change logging under baseline weather, cumulative small-step density-loss reporting for Site 1 starter plants, owner-specific plant/device weather contribution accumulation, directional wind-shadow coverage for plants, plant-tile projection dirties on local wind changes, derived worker energy-cap/work-efficiency behavior, calibrated worker passive weather-decay timing and heat-wind-dust ratio coverage, site-time-system-aligned weather timeline interpolation with baseline recovery, windbreak devices, and one-sided highway-protection weather waves.
- `inventory_economy_task_system_tests.cpp`: System tests spanning inventory, economy, site-start delivery-crate loadout seeding, immediate delivery-crate insertion plus overflow queuing, empty production task-board startup on Site 1, explicit test-side placeholder task seeding for zero-reward completion, resolved buy/transfer/build targets, owner-snapshot-driven task tracking, worker-plus-plant duration-task progress, duration/build-task progress, and phone sell/cart regressions.
- `phone_panel_system_tests.cpp`: System tests for authoritative phone home/app-panel snapshot state, expanded task-count projection, section switching, invalid section rejection, and sell-list refresh after immediate purchase delivery routing.
- `system_test_fixtures.h`: Shared campaign/site fixtures and helpers, including default worker-pack setup, used across the source-authored system tests.
