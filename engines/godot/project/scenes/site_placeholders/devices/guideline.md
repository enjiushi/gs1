# engines/godot/project/scenes/site_placeholders/devices/

Script-free `Node3D` placeholder device scenes for site-session visuals.

## Usage
- Keep roots as `Node3D`; the native `Gs1GodotSiteSceneController` positions the root by gameplay visual ID.
- Keep scenes lightweight and primitive-based until final art replaces them.
- Update `engines/godot/project/config/content_resources.toml` when adding, removing, or renaming a scene path.
- When files in this directory change, update this file in the same change.

## Contents
- `device_201_field_kitchen.tscn`: Camp stove and pot placeholder for Field Kitchen.
- `device_202_workbench.tscn`: Bench and tool placeholder for Workbench.
- `device_203_storage_crate.tscn`: Crate placeholder for Storage Crate.
- `device_204_wind_fence.tscn`: Slatted fence placeholder for Wind Fence.
- `device_205_chemistry_station.tscn`: Table and flask placeholder for Chemistry Station.
