# State Taxonomy Inventory

This document records the current gameplay/runtime state inventory for the completed gameplay-state refactor that moved authoritative reads to `Gs1GameStateView` and removed gameplay-owned UI/presentation state.

The classification buckets used here are:

- `Gameplay-authoritative`: simulation state the gameplay DLL should continue owning and mutating.
- `Exported read-state`: ABI-safe state-view structs or query outputs read by hosts.
- `Host-owned UI/presentation`: state that should live in the engine host or adapter, even if gameplay still stores it today.
- `Transient queue/input`: short-lived intent, input, or message transport state.
- `Migration-only compatibility`: dirty flags, last-emitted caches, projection bookkeeping, or other compatibility scaffolding that should shrink or disappear as snapshot/projection traffic is removed.

## Runtime Aggregate

| Area | Current location | Classification | Notes / target direction |
| --- | --- | --- | --- |
| App flow state | `src/runtime/game_state.h` `GameState::app_state` | Gameplay-authoritative | Keep in gameplay; already exported through `Gs1GameStateView.app_state`. |
| Active campaign object | `src/runtime/game_state.h` `GameState::campaign` | Gameplay-authoritative | Keep in gameplay and continue exposing via `Gs1CampaignStateView`. |
| Active site run object | `src/runtime/game_state.h` `GameState::active_site_run` | Gameplay-authoritative | Keep in gameplay and continue exposing via `Gs1SiteStateView` plus targeted queries. |
| Internal gameplay message queue | `src/runtime/game_state.h` `GameState::message_queue` | Transient queue/input | Keep as internal intent/fact transport only. |
| Runtime-to-host message queue | `src/runtime/game_state.h` `GameState::runtime_messages` | Transient queue/input | Keep only for reduced semantic host messages, not authoritative state transfer. |
| Pending host-message queue | `src/runtime/game_runtime.h` `GameRuntime::host_messages_` | Transient queue/input | Keep as host-intent transport. |
| Runtime move-direction snapshot | `src/runtime/runtime_state_access.h` `RuntimeMoveDirectionSnapshot` and `SiteRunState::host_move_direction` | Transient queue/input | Host input snapshot, not long-lived gameplay authority. |
| Fixed-step duration | `src/runtime/game_state.h` `GameState::fixed_step_seconds` | Gameplay-authoritative | Runtime configuration used by gameplay tick scheduling. |

## Authoritative Campaign and Site State

| Area | Current location | Classification | Notes / target direction |
| --- | --- | --- | --- |
| Campaign progression, sites, loadout planner, technology state | `src/campaign/campaign_state.h` through `GameState::campaign` | Gameplay-authoritative | Keep in gameplay; already surfaced through `Gs1CampaignStateView`. |
| Site clock, camp, inventory, contractor, weather, events, task board, modifiers, economy, craft, action, objective, local weather resolve, plant/device weather contribution | `src/site/site_run_state.h` | Gameplay-authoritative | Core site simulation state; keep in gameplay and expose via state view or query helpers. |
| ECS world and entity ownership | `src/site/site_run_state.h` `SiteRunState::site_world` | Gameplay-authoritative | Keep private to gameplay; do not export Flecs internals directly. |
| Worker, tile, device, plant ECS data | `src/site/site_world.*` and query helpers | Gameplay-authoritative | Continue exposing through flattened views and targeted queries such as `gs1_query_site_tile_view(...)`. |
| Current site result count | `SiteRunState::result_newly_revealed_site_count` | Gameplay-authoritative | Exported in `Gs1SiteStateView`; keep if gameplay rules consume it. |

## Exported Read-State Surface

| Area | Current location | Classification | Notes / target direction |
| --- | --- | --- | --- |
| Root read surface | `include/gs1/state_view.h` `Gs1GameStateView` | Exported read-state | Stable ABI root object for host reads. |
| Campaign read surface | `include/gs1/state_view.h` `Gs1CampaignStateView` and backing arrays in `src/runtime/game_state_view.h` | Exported read-state | Keep as primary campaign read path. |
| Site read surface | `include/gs1/state_view.h` `Gs1SiteStateView` and backing arrays in `src/runtime/game_state_view.h` | Exported read-state | Keep as primary site read path. |
| Tile query helper | `gs1_query_site_tile_view(...)` and `build_site_tile_view(...)` | Exported read-state | Keep for dense ECS-backed tile reads. |
| Backing view cache | `src/runtime/game_state_view.h` `RuntimeGameStateViewCache` | Exported read-state | Internal cache that backs ABI pointers; keep and continue rebinding after container changes. |

## Host-Owned UI / Presentation State in the Current Tree

| Area | Current location | Classification | Notes / target direction |
| --- | --- | --- | --- |
| Godot adapter UI/session state | `engines/godot/native/gs1_godot_adapter_service.h` `Gs1GodotUiSessionState` | Host-owned UI/presentation | Correct ownership for phone, regional-tech, protection, inventory, and local badge/session state that is reconstructable from gameplay reads plus local interaction history. |
| Legacy `src/ui/` folder | `src/ui/guideline.md` | Migration-only compatibility | The gameplay-owned UI view-model/presenter headers were removed; the folder remains only as a lightweight repo-navigation checkpoint until it is repurposed or removed entirely. |

## Migration-Only Compatibility and Projection Bookkeeping

| Area | Current location | Classification | Notes / target direction |
| --- | --- | --- | --- |
| Transitional site dirty helpers | `src/site/systems/site_system_context.h` `mark_*projection_dirty(...)` | Migration-only compatibility | The helper names still exist so old site-system call sites compile, but they are now no-op shims and should disappear once those call sites are trimmed. |
| Legacy projection-dirty flag catalog | `src/site/site_projection_update_flags.h` | Migration-only compatibility | Constants remain only so transitional call sites and compatibility-oriented tests can still name the old buckets while gameplay-owned projection bookkeeping is gone. |
| Public runtime-message compatibility catalog | `include/gs1/types.h` legacy snapshot/panel/progression message enums and payload structs | Migration-only compatibility | Gameplay-side producers are gone from `src/`, but the public message catalog still carries obsolete families that should be deleted in a later host/smoke compatibility pass. |

## Current Boundary Rule After This Inventory

- Treat `GameState`, `CampaignState`, `SiteRunState`, and ECS-backed world data as authoritative gameplay state.
- Treat `RuntimeGameStateViewCache` and `gs1_query_site_tile_view(...)` as the durable exported read boundary.
- Treat the Godot adapter's local UI session structs as the correct home for host-owned presentation state, and treat the remaining no-op projection-dirty helpers plus obsolete public runtime-message catalog entries as compatibility cleanup targets rather than architecture we should preserve.
