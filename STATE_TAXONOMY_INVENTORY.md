# State Taxonomy Inventory

This document records the current gameplay/runtime state inventory for the gameplay-state refactor described in [refactor.md](/d:/testgame/gs1_upstream/refactor.md).

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

## Host-Owned UI / Presentation State Still Stored in Gameplay Runtime

| Area | Current location | Classification | Notes / target direction |
| --- | --- | --- | --- |
| Protection overlay open mode | `src/runtime/site_protection_presentation_state.h` `SiteProtectionPresentationState` | Host-owned UI/presentation | Move ownership to host-side overlay controller; gameplay should answer only gameplay queries or validations. |
| Active UI setup/panel tracking | `src/runtime/ui_presentation_state.h` `UiPresentationState::active_ui_setups`, `active_ui_panels`, `active_normal_ui_setup` | Host-owned UI/presentation | Remove from gameplay once panel/setup projection traffic is retired. |
| Last emitted app-state cache | `UiPresentationState::last_emitted_app_state` | Migration-only compatibility | Presentation emission dedupe only; not gameplay state. |
| Regional tech-tree overlay open state | `UiPresentationState::regional_map_tech_tree_view_open` | Host-owned UI/presentation | Move to Godot/regional host controller local state. |
| Phone open state | `src/runtime/presentation_runtime_state.h` `PhonePanelPresentationState::open` | Host-owned UI/presentation | Moved out of `SiteRunState`; still runtime-owned until host/controller-local ownership fully replaces it. |
| Phone active section/tab | `PhonePanelPresentationState::active_section` | Host-owned UI/presentation | Moved out of gameplay-owned `PhonePanelState`; next step is host/controller-local ownership. |
| Phone badge flags | `PhonePanelPresentationState::badge_flags` | Host-owned UI/presentation | Moved out of gameplay-owned `PhonePanelState`; host should ultimately own read/dismiss session state. |
| Phone notification init bookkeeping | `PhonePanelPresentationState::notification_state_initialized` | Host-owned UI/presentation | Pure presentation-session state now stored in runtime presentation state rather than gameplay-owned site state. |

## Migration-Only Compatibility and Projection Bookkeeping

| Area | Current location | Classification | Notes / target direction |
| --- | --- | --- | --- |
| Phone listing cache | `PhonePanelState::listings` | Migration-only compatibility | Derived from economy/inventory/task state; replace with direct exported derived slice built from authoritative state. |
| Phone count summaries | `PhonePanelState::visible_task_count`, `accepted_task_count`, `completed_task_count`, `claimed_task_count`, `buy_listing_count`, `sell_listing_count`, `service_listing_count`, `cart_item_count` | Migration-only compatibility | Derived presentation summaries; either compute into exported read-state cache or move host-side. |
| Phone change signatures | `src/runtime/presentation_runtime_state.h` `PhonePanelPresentationState::{task_snapshot_signature,buy_snapshot_signature,sell_snapshot_signature,service_snapshot_signature}` | Migration-only compatibility | Remove after gameplay stops tracking host badge/session deltas. |
| Runtime phone last-emitted ids | `src/runtime/presentation_runtime_state.h` `PresentationRuntimeState::last_emitted_phone_listing_ids` | Migration-only compatibility | Remove with phone snapshot/upsert retirement. |
| Campaign unlock snapshot cache | `PresentationRuntimeState::campaign_unlock_snapshot` | Migration-only compatibility | Remove once progression UI reads directly from state and local host caches. |
| Presentation dirty revision counter | `PresentationRuntimeState::presentation_dirty_revision` | Migration-only compatibility | Keep only if still useful as a coarse refresh signal; otherwise collapse into message-only invalidation. |
| Site projection dirty flag mask | `SiteRunState::pending_projection_update_flags` | Migration-only compatibility | Remove as snapshot/upsert paths disappear. |
| Full-tile projection dirty flag | `SiteRunState::pending_full_tile_projection_update` | Migration-only compatibility | Remove with tile snapshot retirement. |
| Tile projection update vectors and masks | `SiteRunState::pending_tile_projection_updates`, `pending_tile_projection_update_mask` | Migration-only compatibility | Replace with direct tile reads and host-side reconciliation. |
| Last projected tile cache | `SiteRunState::last_projected_tile_states` | Migration-only compatibility | Remove after no controller depends on tile upsert deltas. |
| Inventory projection dirty flags and masks | `SiteRunState::pending_full_inventory_projection_update`, `pending_worker_pack_inventory_projection_updates`, `pending_worker_pack_inventory_projection_update_mask`, `pending_inventory_storage_descriptor_projection_update`, `pending_inventory_view_state_projection_update`, `pending_opened_inventory_storage_full_projection_update`, `pending_opened_inventory_storage_projection_updates`, `pending_opened_inventory_storage_projection_update_mask` | Migration-only compatibility | Remove after inventory UI reads directly from `Gs1SiteStateView`. |
| Projection coordinator object | `src/app/game_presentation_coordinator.*` | Migration-only compatibility | Long-term shrink/remove as direct state reads replace snapshot/upsert emission. |

## Current Boundary Rule After This Inventory

- Treat `GameState`, `CampaignState`, `SiteRunState`, and ECS-backed world data as authoritative gameplay state.
- Treat `RuntimeGameStateViewCache` and `gs1_query_site_tile_view(...)` as the durable exported read boundary.
- Treat `UiPresentationState`, `PresentationRuntimeState`, `SiteProtectionPresentationState`, and the phone/session/pending-projection fields listed above as removal or migration targets, not architecture we should preserve.
