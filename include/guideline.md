# include/

Top-level public header folder for the exported gameplay API surface.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `gs1/`: Public GS1 headers shared by the gameplay DLL, hosts, and tests, including the exported runtime lifecycle and split-phase API, the read-only gameplay state-view and targeted site-tile query ABI, runtime profiling controls/snapshots, the standalone system-test discovery/run ABI, shared harvest-action and harvested-output transport contracts, active site-modifier and weather-timeline transport, excavation-depth and device-integrity tile projection fields, and the current host-to-runtime `Gs1HostMessage` plus runtime-to-host semantic `Gs1RuntimeMessage` transport families.
