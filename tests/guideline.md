# tests/

Automated test coverage split into runtime tests, headless game-process journeys, and broader system tests.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `runtime/`: Runtime-focused unit/integration-style tests and probes covering projection/state-view output, split message flow, `StateManager`, timed modifiers, performance probes, smoke-host threading, and the pure C++ Godot policy/debug-protocol helpers, including the site-message-flow and local-weather probes that now refresh split-state fixtures in place instead of rebuilding temporary aggregates and the runtime-facing projection tests that are moving off the removed production compatibility mirrors onto explicit split-state snapshot assembly/writeback.
- `process/`: Headless semantic player-journey automation that runs real DLL gameplay sessions through reusable game-process scenarios, currently starting with a dedicated Site 1 onboarding case.
- `system/`: Standalone system-test framework, source tests, and asset-driven regressions, including harvest-action and harvest-output coverage, total-reputation unlock and linear faction-tech regressions, Village shovel/excavation/recipe/buff-slot coverage, with the host now linking source-authored tests directly instead of discovering them through the gameplay DLL and the shared split-state fixture helpers now exposing in-place apply/write paths for test round-trips plus the inventory/craft tests now using explicit split inventory refs where the last bridge cleanup needed them.
- `system/source/split_state_test_helpers.h`: Test-only aggregate bridge helpers for split-state round-trips used by runtime and system test harnesses.
