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
- `runtime_session.h`: Shared gameplay-runtime session owner that loads the gameplay DLL, creates the runtime from a host-supplied shared project/runtime root plus optional adapter-config payload and fixed-step timing, exposes separate `run_phase1` and `run_phase2` entry points alongside the older combined helper, and exposes host-event submission for engine adapters that need to drive gameplay UI/runtime flow directly.
- `runtime_session.cpp`: Implementation of the shared gameplay-runtime session owner, including shared host-event submission into the gameplay DLL runtime, startup wiring for both the shared project/runtime root path and adapter-config payload passed across the runtime ABI, and the split host-side phase entry points now used by the Godot adapter's pipelined frame loop.
