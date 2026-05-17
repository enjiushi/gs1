# Godot Authored Prewarm Plan

## Purpose

Design a fully authored Godot prewarm system so the adapter does not guess what to warm.

The current prototype fix prewarms one whole scene resource:

- `res://scenes/regional_map_backdrop.tscn`

That worked because it pulled the expensive regional Terrain3D cold-start work earlier, while the main menu was already visible. The next step should generalize this into an authored system with explicit forward and back transition coverage.

## Core Direction

- Keep prewarm policy fully authored.
- Do not auto-retain or auto-infer previous/next scene warmups.
- Author both forward and back transitions explicitly.
- Support warming at resource granularity, not only whole-scene granularity.
- Keep implementation in the native Godot adapter, not project scripts.

## Recommended Model

Use transition-based authored config, keyed by logical screen/app-state names rather than raw scene file names.

Examples:

- `main_menu -> regional_map`
- `regional_map -> main_menu`
- `regional_map -> site_session`
- `site_session -> regional_map`

This is better than a pure per-scene list because the required warmup set may differ by destination and by return path.

## Config Proposal

Future authored file:

- `project/adapter_metadata/godot_prewarm.toml`

Suggested shape:

```toml
[[group]]
id = "regional_backdrop_core"

  [[group.targets]]
  kind = "scene"
  path = "res://scenes/regional_map_backdrop.tscn"

  [[group.targets]]
  kind = "resource"
  path = "res://materials/regional_terrain.material"

  [[group.targets]]
  kind = "resource"
  path = "res://textures/regional_splatmap.png"

[[transition]]
from = "main_menu"
to = "regional_map"
groups = ["regional_backdrop_core"]

[[transition]]
from = "site_session"
to = "regional_map"
groups = ["regional_backdrop_core"]

[[transition]]
from = "regional_map"
to = "site_session"

  [[transition.targets]]
  kind = "scene"
  path = "res://scenes/site_session.tscn"
```

## Supported Target Kinds

### `scene`

Preload a whole `PackedScene` resource.

Use when:

- the expensive cold path is easiest to capture by warming a known sub-scene
- the scene is a stable asset bundle and we want a simple authored entry

### `resource`

Preload a specific Godot resource such as:

- `.material`
- `.tres`
- `.res`
- texture
- mesh
- shader

Use when:

- we know the exact expensive material/texture/resource
- warming the whole scene is broader than needed

### `instantiate_once`

Optional second-phase target type.

Meaning:

- preload the resource
- instantiate or attach it once in a controlled hidden context
- keep only the warmed resource/object references needed afterward

Use when plain resource loading does not eliminate the hitch because the expensive work happens on first real engine/render use instead of file/resource loading.

This should be opt-in and authored only for known stubborn cases such as Terrain3D or shader-heavy assets.

## Runtime Behavior

### Ownership

- `Gs1GodotDirectorControl` remains the high-level owner of screen/app-state transition timing.
- A dedicated native `PrewarmManager` should own authored config, active requests, and retained resources.

### Triggering

The director should request prewarms based on authored transitions, for example:

- while on `main_menu`, request the authored targets for `main_menu -> regional_map`
- while on `regional_map`, request the authored targets for `regional_map -> site_session`
- if return is important, also request the authored targets for `regional_map -> main_menu` or `site_session -> regional_map` only when that path is intentionally authored

### Retention

Retention should also be authored rather than inferred.

Possible retention fields:

- `until_transition_consumed`
- `until_screen_exit`
- `manual`

I do not recommend implicit TTL policy unless we explicitly decide we want it. For now, authored release policy is safer and clearer.

## Resource Cache Shape

The native adapter will likely need a small retained-resource cache:

- key: resource path
- value: retained Godot `Ref<Resource>` or `Ref<PackedScene>`
- bookkeeping: which active authored requests currently depend on it

Desired behavior:

- if two transitions reference the same target, load once and share
- if a target is no longer needed by any active authored request, release it
- avoid duplicate threaded-load requests for the same path

## Initial Implementation Plan

### Phase 1

Build the minimum authored system:

- add `godot_prewarm.toml`
- add native TOML loader for prewarm config
- add `PrewarmManager`
- support `scene`
- support `resource`
- support explicit retain/release policy
- integrate with `Gs1GodotDirectorControl`

This phase should be enough to replace the current hardcoded regional backdrop prewarm.

### Phase 2

Add higher-fidelity control:

- shared groups
- request deduplication across transitions
- debug visibility into active prewarm state
- optional `instantiate_once`

### Phase 3

Tune authored data:

- move known heavy resources out of scene-level warming when practical
- keep scene-level warmups only where they remain the simplest stable answer

## How To Choose What To Prewarm

Author the smallest target set that reliably removes the observed hitch.

Preferred order:

1. specific material/texture/resource if the culprit is known
2. a focused sub-scene if that is the smallest practical stable bundle
3. `instantiate_once` only if load-only is not enough

Avoid warming:

- assets with no measured transition impact
- broad bundles just because they are nearby
- everything in both directions by default

## Recommended First Usage In This Repo

Start with these explicit authored transitions:

1. `main_menu -> regional_map`
2. `regional_map -> main_menu`
3. `regional_map -> site_session`
4. `site_session -> regional_map`

And begin with these likely target categories:

- regional backdrop scene
- regional Terrain3D material resources
- regional Terrain3D textures
- site-session root scene if it proves heavy

## Questions To Resolve

### 1. Which logical key should the config use?

Recommendation:

- use adapter screen/app-state ids such as `main_menu`, `regional_map`, `site_session`

Reason:

- this matches director behavior better than raw scene file paths

### 2. Should one transition be allowed to request multiple independent target groups?

Recommendation:

- yes

Reason:

- this keeps reusable bundles possible without duplicating asset lists

### 3. Should retention live on transitions, groups, or individual targets?

Recommendation:

- allow target-level override
- otherwise inherit from transition-level default

Reason:

- some resources may need longer retention than others within the same transition

### 4. Do we need `instantiate_once` in v1?

Recommendation:

- no, unless we confirm resource preload alone is insufficient for the next known hitch

Reason:

- this keeps v1 much simpler

### 5. Should the config support priorities?

Recommendation:

- optional, low priority

Reason:

- useful only if we later cap parallel prewarm work or want partial fallback under load

## Suggestions To Make The System Better

### Suggestion 1

Add a small debug dump or UI-visible report of:

- currently requested transitions
- loaded targets
- retained targets
- failed targets

This will make authored tuning much easier.

### Suggestion 2

Log a short transition timing summary around:

- prewarm requested
- prewarm finished
- scene switch started
- scene switch finished

This is useful even if we remove the heavier profiling code, because it helps validate whether authored warmups actually completed before transition time.

### Suggestion 3

Keep the config Godot-only and adapter-owned under `project/adapter_metadata/`.

Reason:

- prewarm policy is a presentation/runtime concern, not gameplay simulation content

### Suggestion 4

Prefer a few named shared groups for large repeated bundles such as regional backdrop resources, but do not over-abstract the first version.

Reason:

- too much indirection will make the authored file harder to tune

## Proposed Deliverable Breakdown

1. Add `project/adapter_metadata/godot_prewarm.toml`.
2. Add native adapter config structs and TOML loader.
3. Add `PrewarmManager` with threaded resource loading and retained-resource bookkeeping.
4. Replace the current hardcoded regional-backdrop prewarm with config-driven transition requests.
5. Add a small debug inspection path for active/loaded prewarm entries.
6. Tune the first four authored transitions.

## Notes

- The current hardcoded prewarm is still a good proof that authored warmup can solve real transition hitches.
- The authored system should preserve that benefit while making the policy explicit, reusable, and easier to maintain.
