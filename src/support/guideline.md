# src/support/

Shared lightweight support types reused across runtime and system code.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `currency.h`: Shared cash-point conversion constants and helpers that keep internal valuation aligned with player-facing cash, including integer cash-point normalization and float cash-display conversion helpers.
- `excavation_types.h`: Shared excavation depth and loot-tier enums used by content, site runtime, and projection code.
- `id_types.h`: Strongly typed ID wrappers and helper aliases used across gameplay state, including the shared `ModifierId` used by nearby auras, permanent run modifiers, and timed item-granted modifiers.
- `site_objective_types.h`: Shared enums for site objective modes and target edges used by content, site setup, and objective evaluation.
