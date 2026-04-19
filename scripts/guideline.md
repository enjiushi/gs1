# scripts/

PowerShell entry points for building targets and running smoke/system test workflows.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `build_gameplay_dll.ps1`: Builds the gameplay DLL target.
- `build_smoke_host.ps1`: Builds the smoke-test host executable.
- `build_system_test_host.ps1`: Builds the standalone system-test host executable.
- `build_visual_smoke_host.ps1`: Builds the visual smoke host used by the live viewer.
- `common.ps1`: Shared helper functions imported by the other PowerShell scripts.
- `run_smoke.ps1`: Runs the scripted smoke-test flow and forwards PowerShell `-Verbose` to the smoke host as `--verbose`.
- `run_system_tests.ps1`: Runs the system-test host and its authored test packs.
- `run_visual_smoke.ps1`: Launches the visual smoke workflow and live viewer support, forwarding PowerShell `-Verbose` to the visual host as `--verbose`.
