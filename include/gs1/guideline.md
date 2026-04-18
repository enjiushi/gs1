# include/gs1/

Public C ABI and shared runtime type declarations exposed outside the gameplay DLL.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `export.h`: DLL import/export macro definitions used by the public API headers.
- `game_api.h`: Main exported gameplay entry points and host-facing function declarations.
- `status.h`: Public result/status codes returned across the DLL boundary.
- `system_test_api.h`: Public hooks for the standalone system-test host and test asset runners.
- `types.h`: Shared API structs, payloads, handles, and enums used by runtime callers.
