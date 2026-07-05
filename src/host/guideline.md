# src/host/

GS1-owned host bridge implementation and headers used by the process host, runtime projection test host-facing helpers, and the Godot adapter service.

## Usage
- Read this file before scanning the folder in detail.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `adapter_metadata_catalog.cpp` / `include/gs1/host/adapter_metadata_catalog.h`: Loads GS1 adapter metadata TOML catalogs for Godot-facing labels and descriptions.
- `runtime_dll_loader.cpp` / `include/gs1/host/runtime_dll_loader.h`: GS1 gameplay DLL symbol loader for the current exported runtime ABI.
- `runtime_session.cpp` / `include/gs1/host/runtime_session.h`: Thin GS1 host-side runtime session wrapper around the loaded gameplay DLL API.
