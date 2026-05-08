# tests/process/

Headless game-process automation that drives real gameplay journeys through semantic player actions and host-side state assertions.

## Usage
- Read this file before scanning the folder in detail.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `game_process_registry.h` / `game_process_registry.cpp`: Registry of named game-process scenarios plus shared run options, currently exposing the reusable `site1_onboarding` scenario by adapting the existing silent-onboarding bot.
- `game_process_test_host_main.cpp`: Dedicated process-test host that loads the gameplay DLL, lists or filters registered scenarios, creates a fresh runtime per scenario, and runs the selected game-process journeys headlessly without the browser viewer.
