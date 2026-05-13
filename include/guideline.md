# include/

Top-level public header folder for the exported gameplay API surface.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `gs1/`: Public GS1 headers shared by the gameplay DLL, hosts, and tests, including the new read-only gameplay state-view ABI in `gs1/state_view.h` with direct craft-context and placement-preview readback for host-owned site craft panels, shared harvest-action and harvested-output transport contracts, active site-modifier projection plus timed-modifier remaining-game-hour and end-action transport, excavate-action and excavation-depth tile projection transport fields, occupied-tile device-integrity transport used by the smoke viewer's density/integrity overlay, and `gs1/types.h` now carrying the host-to-runtime `Gs1HostMessage` transport contract plus the reduced runtime-to-host semantic `Gs1RuntimeMessage` families used by the active Godot path after the gameplay ABI stopped carrying host-owned phone/protection/tech-tree panel session declarations.
