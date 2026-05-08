# scripts/

PowerShell entry points for building targets and running smoke, process, and system test workflows.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `build_gameplay_dll.ps1`: Builds the gameplay DLL target.
- `build_process_test_host.ps1`: Builds the dedicated headless game-process test host.
- `setup_godot_build.ps1`: Verifies the expected sibling Godot source tree plus the `third_party/godot-cpp` submodule checkout, reminds new machines to run `git submodule update --init --recursive` when needed, configures a Godot-enabled CMake build directory with the required adapter variables, and can optionally build `gs1_godot_adapter` in the same step.
- `build_smoke_host.ps1`: Builds the smoke-test host executable.
- `build_system_test_host.ps1`: Builds the standalone system-test host executable.
- `build_visual_smoke_host.ps1`: Builds the visual smoke host used by the live viewer.
- `common.ps1`: Shared helper functions imported by the other PowerShell scripts.
- `run_process_tests.ps1`: Runs the dedicated headless game-process host with scenario listing/filtering plus frame-budget overrides for semantic player-journey automation.
- `run_smoke.ps1`: Runs the scripted smoke-test flow and forwards PowerShell `-Verbose` to the smoke host as `--verbose`.
- `run_system_tests.ps1`: Runs the standalone system-test host and its authored test packs without requiring a gameplay DLL path argument.
- `run_visual_smoke.ps1`: Launches the visual smoke workflow and live viewer support, forwarding PowerShell `-Verbose` to the visual host as `--verbose`, and now also exposes a foreground `-SilentOnboarding` mode that runs the visual host headlessly through the full Site 1 onboarding chain without opening the browser.
