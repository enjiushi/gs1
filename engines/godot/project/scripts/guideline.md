# engines/godot/project/scripts/

Godot project-script area kept only for cases where the native plugin path is not practical.

## Usage
- Read this file before scanning the folder in detail.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `menu_backdrop_character_retarget.gd`: Small menu-only presentation script that copies a mapped subset of local bone poses from the imported `basic_animation_free` driver skeleton onto the visible PSX killer rig so the authored main-menu backdrop can show a looping character animation without introducing gameplay behavior into GDScript.
