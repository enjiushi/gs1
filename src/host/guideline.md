# src/host/

Shared engine-agnostic host bridge code used by runtime clients that load the gameplay DLL.

## Usage
- Read this file before scanning the folder in detail.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `adapter_config_loader.h` / `adapter_config_loader.cpp`: Shared host-side helper for packaging adapter-owned config roots into a runtime-startup payload so engine adapters can pass the staged runtime package root into gameplay startup alongside any engine-specific presentation/resource config.
- `adapter_metadata_catalog.h` / `adapter_metadata_catalog.cpp`: Shared adapter-side metadata catalog loader for presentation-only labels and descriptions such as modifiers plus progression entries, loaded from an `adapter_metadata/` root under the active project/runtime package so the gameplay DLL does not own that descriptive copy, with adapter metadata keyed directly by the same globally unique per-type ids used by gameplay content and references.
- `runtime_dll_loader.h`: Shared function-pointer table and Win32 DLL loader for the engine-free gameplay runtime ABI.
- `runtime_dll_loader.cpp`: Win32 implementation of the shared gameplay DLL loader.
- `runtime_message_pump.h`: Shared engine-message drain helper that feeds projection caches from the gameplay runtime and can also retain the authoritative drained message list for adapters that consume deltas directly.
- `runtime_message_pump.cpp`: Implementation of the shared engine-message drain helper.
- `runtime_projection_state.h`: Shared runtime-facing projection structs and cache logic for engine adapters, now covering app-state transitions, both legacy UI-setup batches and the newer panel-slot/list UI projection used by the authored Godot main-menu and regional-map shells, full regional-map snapshots, dense row-major site-tile storage, inventory/task/phone/craft/placement/protection/site-result projections, and recent one-shot cues alongside direct delta-message consumption.
- `runtime_projection_state.cpp`: Implementation of the expanded shared projection cache, including dense row-major site-tile indexing without adapter-owned dirty tracking and message-driven projection updates for the broader Godot/smoke client surface.
- `runtime_session.h`: Shared gameplay-runtime session owner that loads the gameplay DLL, creates the runtime from a host-supplied shared project/runtime root plus optional adapter-config payload and fixed-step timing, advances the two-phase update loop, and exposes host-event submission for engine adapters that need to drive gameplay UI/runtime flow directly.
- `runtime_session.cpp`: Implementation of the shared gameplay-runtime session owner, including shared host-event submission into the gameplay DLL runtime and startup wiring for both the shared project/runtime root path and adapter-config payload passed across the runtime ABI.
