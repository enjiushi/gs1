# GS1 System Test Manual

This document describes the GS1 per-system test framework, how to run it, and how to add, change, or remove tests.

## Goals

The framework is designed around these goals:

- Every gameplay system can be tested without adding direct system-to-system shortcuts.
- Tests run through the gameplay DLL, not through a separate copy of the gameplay code.
- Test cases can be authored in source code or in asset files.
- Developers can run all system tests or only one or more target systems while iterating.
- Adding new tests should be low-friction and should not require inventing a new host per system.

## Framework Layout

Key files:

- [include/gs1/system_test_api.h](E:/gs1/include/gs1/system_test_api.h): C ABI exported by the gameplay DLL for system-test discovery and execution.
- [src/testing/system_test_registry.h](E:/gs1/src/testing/system_test_registry.h): source-test registration macros, assertion helpers, and asset-runner registration.
- [src/testing/system_test_registry.cpp](E:/gs1/src/testing/system_test_registry.cpp): registry storage, execution, timing, and result packaging.
- [src/testing/system_test_asset.h](E:/gs1/src/testing/system_test_asset.h): asset-file contract and discovery helpers.
- [src/testing/system_test_asset.cpp](E:/gs1/src/testing/system_test_asset.cpp): asset metadata parsing and recursive discovery.
- [tests/system/system_test_host_main.cpp](E:/gs1/tests/system/system_test_host_main.cpp): standalone executable that loads the gameplay DLL, lists tests, filters them, and runs them.
- [tests/system/source](E:/gs1/tests/system/source): source-authored system test `.cpp` files compiled into the DLL when `GS1_BUILD_TESTS=ON`.
- [tests/system/assets](E:/gs1/tests/system/assets): asset-authored test files using the `.gs1systemtest` extension.
- [scripts/build_system_test_host.ps1](E:/gs1/scripts/build_system_test_host.ps1): builds the DLL and the system-test host.
- [scripts/run_system_tests.ps1](E:/gs1/scripts/run_system_tests.ps1): runs all tests, filtered systems, or specific asset files.
- [CMakeLists.txt](E:/gs1/CMakeLists.txt): build and CTest wiring, including configure-time system filters.

## Design Summary

The system-test framework has four layers:

1. Gameplay DLL test API
   The DLL exports functions to:
   - report the system-test API version
   - enumerate registered source test cases
   - run a source test case by index
   - run an asset test file by path

2. In-DLL registry
   Source tests and asset runners are registered statically inside the DLL. This keeps test execution close to the same gameplay code and internal state that production runtime paths use.

3. External test host executable
   `gs1_system_test_host` is a separate executable. It loads `gs1_game.dll`, asks the DLL what source tests exist, discovers asset tests from disk, applies filters, runs the requested tests, and prints results.

4. Build and run integration
   CMake and PowerShell scripts let you:
   - build the framework
   - run all tests
   - run only selected systems
   - set a configure-time filter for CTest or CI use

## Why This Design

This project already uses a DLL-loading host pattern for smoke tests. The system-test framework follows that same runtime shape so tests use the real gameplay DLL boundary instead of a special direct-link-only path.

This design also avoids mixing test coordination into gameplay systems:

- Systems still own their own state.
- Cross-system behavior should still flow through `GameCommand`.
- Tests can observe behavior through public state, command flow, or runtime outputs without breaking ownership rules.

## Supported Test Authoring Styles

Two authoring styles are supported.

### 1. Source-authored tests

Use source tests when:

- the test needs C++ helpers
- the test needs direct access to internal gameplay types
- the setup is easier to express as code
- you want fast compile-time feedback while changing a system

Source test files live under [tests/system/source](E:/gs1/tests/system/source). They are picked up automatically by CMake when `GS1_BUILD_TESTS=ON`.

### 2. Asset-authored tests

Use asset tests when:

- you want test data to live in files
- you want many cases to share one runner implementation
- designers or content authors may need to add or tweak cases later
- you want to separate the test payload from the test executor

Asset test files live under [tests/system/assets](E:/gs1/tests/system/assets) and use the `.gs1systemtest` extension.

## Current Asset Contract

Each asset file starts with a metadata header, then an optional payload section after `---`.

Required metadata keys:

- `system`
- `name`
- `runner`

Optional metadata keys:

- `description`
- any runner-specific custom keys

Template:

```text
system = inventory
name = use_item_hydration
runner = inventory_runtime
description = Optional description.
---
# optional runner-specific payload starts here
```

Important:

- The framework only defines the common metadata contract.
- The payload schema is owned by the named runner.
- No real asset runners or cases are required yet. The framework is ready for them.

## How Test Discovery Works

When the host starts:

1. It loads `gs1_game.dll`.
2. It queries the DLL for all registered source tests.
3. It scans [tests/system/assets](E:/gs1/tests/system/assets) for `.gs1systemtest` files.
4. It optionally loads any explicitly requested asset files.
5. It filters the final list by `system` name if requested.
6. It runs each selected case and prints pass/fail/error results.

## How To Build

### Build the host and DLL

```powershell
scripts/build_system_test_host.ps1
```

### Build with a configure-time system filter

This is useful for CI or when you want the generated `ctest` entry to always run only certain systems.

```powershell
scripts/build_system_test_host.ps1 -System inventory,weather_event
```

Equivalent direct CMake form:

```powershell
cmake -S . -B build -DGS1_BUILD_TESTS=ON -DGS1_SYSTEM_TEST_SYSTEMS=inventory;weather_event
cmake --build build --config Debug --target gs1_system_test_host
```

### Relevant CMake switches

- `GS1_BUILD_TESTS`
  Enables test executables and source test compilation.
- `GS1_SYSTEM_TEST_SYSTEMS`
  Semicolon-separated list of system names to pass to `gs1_system_test_host` from CTest.

## How To Run Tests

### Run all discovered system tests

```powershell
scripts/run_system_tests.ps1
```

### List discovered tests without running them

```powershell
scripts/run_system_tests.ps1 -List
```

### Run only one or more systems

```powershell
scripts/run_system_tests.ps1 -System inventory
scripts/run_system_tests.ps1 -System inventory,weather_event
```

### Run a specific asset file

```powershell
scripts/run_system_tests.ps1 -Asset tests/system/assets/inventory/use_item_hydration.gs1systemtest
```

### Build first, then run

```powershell
scripts/run_system_tests.ps1 -BuildFirst
scripts/run_system_tests.ps1 -BuildFirst -System inventory
```

### Run through CTest

```powershell
ctest -C Debug -R gs1_system_test_host --output-on-failure
```

If the build was configured with `GS1_SYSTEM_TEST_SYSTEMS`, the CTest entry will pass those system filters automatically.

## How To Add A Source Test Case

### Step 1: create or update a source file

Add a `.cpp` file under [tests/system/source](E:/gs1/tests/system/source). You can group multiple cases for one system in one file.

Suggested naming:

- `tests/system/source/inventory_system_tests.cpp`
- `tests/system/source/weather_event_system_tests.cpp`

### Step 2: include the registry header

```cpp
#include "testing/system_test_registry.h"
```

Add any gameplay headers needed for the system under test.

### Step 3: write a test function

The function signature is:

```cpp
void my_test(gs1::testing::SystemTestExecutionContext& context)
```

Inside the function use:

- `GS1_SYSTEM_TEST_REQUIRE(context, expr)`
  Fails immediately and returns.
- `GS1_SYSTEM_TEST_CHECK(context, expr)`
  Records a failure but keeps going.
- `GS1_SYSTEM_TEST_FAIL(context, "message")`
  Forces failure with a custom message and returns.

Template:

```cpp
#include "testing/system_test_registry.h"

namespace
{
void inventory_example_test(gs1::testing::SystemTestExecutionContext& context)
{
    GS1_SYSTEM_TEST_REQUIRE(context, true);
    GS1_SYSTEM_TEST_CHECK(context, true);
}
}

GS1_REGISTER_SOURCE_SYSTEM_TEST("inventory", "example_test", inventory_example_test);
```

### Step 4: build and run

```powershell
scripts/run_system_tests.ps1 -BuildFirst -System inventory
```

### Notes for source tests

- Put the `system` string on the registration macro in lowercase for consistency.
- Name tests for gameplay behavior, not implementation detail.
- Prefer setting up only the owned state slice the tested system needs.
- For cross-system interactions, drive the scenario through `GameCommand` flow rather than direct peer mutation.

## How To Add An Asset Test Case

Asset tests require two pieces:

- an asset runner implemented in source code
- one or more `.gs1systemtest` files that point at that runner

### Step 1: add or reuse an asset runner

Create a runner in a source test file under [tests/system/source](E:/gs1/tests/system/source), or add it to an existing file for that system.

Runner signature:

```cpp
void my_runner(
    gs1::testing::SystemTestExecutionContext& context,
    const gs1::testing::SystemTestAssetDocument& document)
```

Register it with:

```cpp
GS1_REGISTER_SYSTEM_TEST_ASSET_RUNNER("inventory_runtime", my_runner);
```

Template:

```cpp
#include "testing/system_test_registry.h"

namespace
{
void inventory_runtime_runner(
    gs1::testing::SystemTestExecutionContext& context,
    const gs1::testing::SystemTestAssetDocument& document)
{
    GS1_SYSTEM_TEST_REQUIRE(context, document.system_name == "inventory");
    GS1_SYSTEM_TEST_REQUIRE(context, !document.test_name.empty());
}
}

GS1_REGISTER_SYSTEM_TEST_ASSET_RUNNER("inventory_runtime", inventory_runtime_runner);
```

### Step 2: add the asset file

Create a file under [tests/system/assets](E:/gs1/tests/system/assets), for example:

- `tests/system/assets/inventory/use_item_hydration.gs1systemtest`

Template:

```text
system = inventory
name = use_item_hydration
runner = inventory_runtime
description = Verifies hydration item behavior.
---
# runner-owned payload goes here later
```

### Step 3: run it

```powershell
scripts/run_system_tests.ps1 -BuildFirst -System inventory
scripts/run_system_tests.ps1 -Asset tests/system/assets/inventory/use_item_hydration.gs1systemtest
```

### Notes for asset tests

- The asset `system` value is what filtering uses.
- The asset `runner` value must match a registered runner name exactly.
- One runner can power many asset files.
- Prefer asset tests when the main variation is data, not control flow.

## How To Modify A Test Case

### Modify a source test

1. Edit the `.cpp` file under [tests/system/source](E:/gs1/tests/system/source).
2. Rebuild and run the relevant system:

```powershell
scripts/run_system_tests.ps1 -BuildFirst -System inventory
```

If you renamed the file, CMake will pick it up automatically on the next configure/build because `tests/system/source/*.cpp` is auto-discovered.

### Modify an asset test

1. Edit the `.gs1systemtest` file under [tests/system/assets](E:/gs1/tests/system/assets).
2. Re-run either the owning system or the specific asset:

```powershell
scripts/run_system_tests.ps1 -System inventory
scripts/run_system_tests.ps1 -Asset tests/system/assets/inventory/use_item_hydration.gs1systemtest
```

Asset-only changes do not require a rebuild unless you also changed the runner code.

## How To Remove A Test Case

### Remove a source test

Two common cases:

- Delete the registration macro for that case if the file still contains other tests.
- Delete the whole `.cpp` file if it is only used for that test or that system's tests.

Then rebuild:

```powershell
scripts/build_system_test_host.ps1
```

### Remove an asset test

Delete the `.gs1systemtest` file. No code or CMake change is required.

If a runner is no longer used by any assets, you may also remove its source registration and function.

## Recommended Workflow During System Changes

When changing one system:

1. Add or update the system test case.
2. Run only that system's tests.
3. Repeat until the system behavior is correct.
4. Run the broader test set before merging.

Typical commands:

```powershell
scripts/run_system_tests.ps1 -BuildFirst -System inventory
scripts/run_system_tests.ps1 -BuildFirst -System weather_event
scripts/run_system_tests.ps1 -BuildFirst
```

## Result Semantics

The host prints one of these outcomes per test:

- `PASS`
  The test ran and recorded zero failures.
- `FAIL`
  The test ran, but one or more assertions failed.
- `ERR`
  The framework could not run the case, usually because of invalid arguments, missing runner registration, or another framework-level issue.

At the end, the host prints a summary with counts for passed, failed, and error cases.

## Guidelines For Good System Tests

- Keep each test focused on one gameplay behavior.
- Prefer behavior names such as `accept_task_adds_task_to_accepted_list` over internal names.
- Keep setup small and explicit.
- Respect system ownership boundaries.
- Drive cross-owner effects through commands, not direct peer mutation.
- Use asset tests when many cases share the same execution logic.
- Use source tests when the logic is code-heavy or needs internal helper setup.

## Current State

The framework is implemented and ready, but the repository does not need real system cases yet.

That means:

- the host builds and runs
- source discovery works
- asset discovery works
- filtering works
- CTest integration works
- real source test cases and asset runners can be added incrementally, system by system

## Quick Reference

Build:

```powershell
scripts/build_system_test_host.ps1
```

Run all:

```powershell
scripts/run_system_tests.ps1
```

Run one system:

```powershell
scripts/run_system_tests.ps1 -System inventory
```

Run multiple systems:

```powershell
scripts/run_system_tests.ps1 -System inventory,weather_event
```

List tests:

```powershell
scripts/run_system_tests.ps1 -List
```

Run one asset:

```powershell
scripts/run_system_tests.ps1 -Asset tests/system/assets/inventory/use_item_hydration.gs1systemtest
```
