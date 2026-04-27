# include/

Top-level public header folder for the exported gameplay API surface.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `gs1/`: Public GS1 headers shared by the gameplay DLL, hosts, and tests, including shared harvest-action and harvested-output transport contracts, active site-modifier projection plus timed-modifier remaining-game-hour and end-action transport, plus excavate-action and excavation-depth tile projection transport fields.
