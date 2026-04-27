# tests/

Automated test coverage split into runtime tests, smoke flows, and broader system tests.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `runtime/`: Runtime-focused unit/integration-style tests and probes.
- `smoke/`: DLL-loading smoke host, scripted smoke flows, and visual smoke assets, including the browser-side site renderer, the adapter FPS plus web/host/DLL frame-cost readout, RMB harvest context actions, and tuned low-profile straw checkerboard presentation.
- `system/`: Standalone system-test framework, source tests, and asset-driven regressions, including harvest-action and harvest-output coverage.
