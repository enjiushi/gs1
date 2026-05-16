# tests/

Automated test coverage split into runtime tests, headless game-process journeys, smoke flows, and broader system tests.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `runtime/`: Runtime-focused unit/integration-style tests and probes covering projection/state-view output, split message flow, `StateManager`, timed modifiers, performance probes, smoke-host threading, and the pure C++ Godot policy/debug-protocol helpers.
- `process/`: Headless semantic player-journey automation that runs real DLL gameplay sessions through reusable game-process scenarios, currently starting with a dedicated Site 1 onboarding case.
- `smoke/`: DLL-loading smoke host, scripted smoke flows, and visual smoke assets, including the browser-side site renderer, timed-modifier remaining-game-hour badge projection, the adapter FPS plus web/host/DLL frame-cost readout, RMB harvest context actions, tuned low-profile straw checkerboard presentation, the fixed-width left-HUD site vitals/task stack with shorter tracked-task summary copy, the expanded Village shovel/food item catalog metadata used by the viewer, the new headless visual-host Site 1 onboarding path used for silent smoke runs without the browser, and the phase-1 site-scene-ready acknowledgement path that auto-confirms `SITE_LOADING` for smoke hosts that do not perform real async scene loads.
- `system/`: Standalone system-test framework, source tests, and asset-driven regressions, including harvest-action and harvest-output coverage, total-reputation unlock and linear faction-tech regressions, Village shovel/excavation/recipe/buff-slot coverage, with the host now linking source-authored tests directly instead of discovering them through the gameplay DLL.
- `system/source/split_state_test_helpers.h`: Test-only aggregate bridge helpers for split-state round-trips used by runtime and system test harnesses.
