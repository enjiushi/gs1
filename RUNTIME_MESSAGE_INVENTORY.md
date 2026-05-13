# Runtime Message Inventory

This document records the current runtime-to-host message families for the gameplay-state refactor in [refactor.md](/d:/testgame/gs1_upstream/refactor.md), along with keep/remove/replace decisions.

The intent is to reduce `gs1_pop_runtime_message(...)` to a small semantic transport while the host reads authoritative gameplay state through `Gs1GameStateView` and targeted query helpers.

## Intended Minimal First-Pass Coarse Refresh Message Set

These are the runtime messages that should remain as the default first-pass host boundary once the remaining projection traffic is removed:

- `GS1_ENGINE_MESSAGE_SET_APP_STATE`
  Purpose: authoritative gameplay answer for app/scene flow.
- `GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY`
  Purpose: coarse refresh invalidation only.
  Current dirty flags already map well to the first-pass refresh buckets:
  `GS1_PRESENTATION_DIRTY_APP_STATE`, `REGIONAL_MAP`, `SITE`, `HUD`, `PHONE`, `NOTIFICATIONS`, `SITE_RESULT`.
- `GS1_ENGINE_MESSAGE_LOG_TEXT`
  Purpose: debug/log transport.
- `GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE`
  Purpose: small event-like cue not naturally represented as durable state.
- `GS1_ENGINE_MESSAGE_SITE_RESULT_READY`
  Purpose: one-shot completion cue while the host opens result UI from authoritative state.

Transitional semantic messages that still exist today but should not remain part of the long-term minimal set:

- `GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES`
- `GS1_ENGINE_MESSAGE_HUD_STATE`
- `GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE`
- `GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE`
- `GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE`

Those messages currently duplicate or package state that is already readable, or should become readable, through the exported gameplay-state surface.

## Family Inventory

| Message family | Types | Current role | Decision | Replacement / target |
| --- | --- | --- | --- | --- |
| Lifecycle and coarse refresh | `SET_APP_STATE`, `PRESENTATION_DIRTY` | Scene-flow answer plus controller invalidation | Keep | This is the target coarse refresh layer. |
| Logs and event-like cues | `LOG_TEXT`, `PLAY_ONE_SHOT_CUE`, `SITE_RESULT_READY` | Debug text, audio/UI cue, end-of-run cue | Keep | Remain small semantic messages. |
| Transitional semantic state mirrors | `CAMPAIGN_RESOURCES`, `HUD_STATE`, `SITE_ACTION_UPDATE`, `SITE_PHONE_PANEL_STATE`, `SITE_PROTECTION_OVERLAY_STATE` | Legacy controller-friendly packaged state mirrors; no gameplay-side producer remains in the current `src/` tree | Remove | Read from `Gs1GameStateView` or targeted helper APIs instead. |
| Regional map scene snapshot | `BEGIN_REGIONAL_MAP_SNAPSHOT`, `REGIONAL_MAP_SITE_UPSERT`, `REGIONAL_MAP_SITE_REMOVE`, `REGIONAL_MAP_LINK_UPSERT`, `REGIONAL_MAP_LINK_REMOVE`, `END_REGIONAL_MAP_SNAPSHOT` | Legacy authoritative regional world projection stream retained only in the public message catalog | Remove | Direct campaign/read-state controllers plus local host reconciliation. |
| Regional HUD snapshot | `BEGIN_REGIONAL_MAP_HUD_SNAPSHOT` through `END_REGIONAL_MAP_HUD_SNAPSHOT` | Legacy HUD-local regional projection stream retained only in the public message catalog | Remove | Direct reads from `Gs1CampaignStateView` and local host UI state. |
| Regional summary snapshot | `BEGIN_REGIONAL_SUMMARY_SNAPSHOT` through `END_REGIONAL_SUMMARY_SNAPSHOT` | Legacy summary-panel regional projection stream retained only in the public message catalog | Remove | Direct reads from `Gs1CampaignStateView`. |
| Regional selection snapshot | `BEGIN_REGIONAL_SELECTION_SNAPSHOT` through `END_REGIONAL_SELECTION_SNAPSHOT` | Legacy selection-panel site/link snapshot stream retained only in the public message catalog | Remove | Direct reads from campaign state and host-side selection UI state. |
| Generic UI setup stream | `BEGIN_UI_SETUP`, `UI_ELEMENT_UPSERT`, `END_UI_SETUP`, `CLOSE_UI_SETUP` | Legacy gameplay-authored UI setup lifecycle retained only in the public message catalog | Remove | Host-owned authored scenes and host-local panel state. |
| Generic UI panel stream | `BEGIN_UI_PANEL`, `UI_PANEL_TEXT_UPSERT`, `UI_PANEL_SLOT_ACTION_UPSERT`, `UI_PANEL_LIST_ITEM_UPSERT`, `UI_PANEL_LIST_ACTION_UPSERT`, `END_UI_PANEL`, `CLOSE_UI_PANEL` | Legacy gameplay-authored panel content stream retained only in the public message catalog | Remove | Host builds panels from exported state, metadata, and local UI state. |
| Progression view stream | `BEGIN_PROGRESSION_VIEW`, `PROGRESSION_ENTRY_UPSERT`, `END_PROGRESSION_VIEW`, `CLOSE_PROGRESSION_VIEW` | Legacy gameplay-authored tech/unlock panel content stream retained only in the public message catalog | Remove | Direct reads from `Gs1CampaignStateView.progression_entries` plus host-owned overlay state. |
| UI surface visibility stream | `SET_UI_SURFACE_VISIBILITY` | Legacy gameplay-directed shell visibility stream retained only in the public message catalog | Remove | Host derives visibility from gameplay flow state plus local UI state. |
| Broad site snapshot | `BEGIN_SITE_SNAPSHOT`, `SITE_TILE_UPSERT`, `SITE_WORKER_UPDATE`, `SITE_CAMP_UPDATE`, `SITE_WEATHER_UPDATE`, `SITE_INVENTORY_SLOT_UPSERT`, `SITE_TASK_UPSERT`, `SITE_TASK_REMOVE`, `SITE_PHONE_LISTING_UPSERT`, `SITE_PHONE_LISTING_REMOVE`, `END_SITE_SNAPSHOT`, `SITE_ACTION_UPDATE`, `SITE_INVENTORY_STORAGE_UPSERT`, `SITE_INVENTORY_VIEW_STATE`, `SITE_PHONE_PANEL_STATE`, `SITE_PROTECTION_OVERLAY_STATE`, `SITE_MODIFIER_LIST_BEGIN`, `SITE_MODIFIER_UPSERT` | Legacy mixed state-transfer family retained only in the public message catalog | Remove | Replace with direct `Gs1SiteStateView` reads, tile queries, and small semantic cues only. |
| Site scene visual snapshot | `BEGIN_SITE_SCENE_SNAPSHOT`, `SITE_SCENE_TILE_UPSERT`, `SITE_SCENE_WORKER_UPDATE`, `SITE_SCENE_PLANT_VISUAL_UPSERT`, `SITE_SCENE_PLANT_VISUAL_REMOVE`, `SITE_SCENE_DEVICE_VISUAL_UPSERT`, `SITE_SCENE_DEVICE_VISUAL_REMOVE`, `END_SITE_SCENE_SNAPSHOT` | Legacy Godot world-scene reconciliation stream retained only in the public message catalog | Remove | Move to direct state reads and query helpers while keeping stable gameplay-ID to Godot-object reconciliation. |
| Dedicated inventory panel snapshot | `BEGIN_SITE_INVENTORY_PANEL_SNAPSHOT` through `END_SITE_INVENTORY_PANEL_SNAPSHOT` | Legacy inventory-panel-only state transfer retained only in the public message catalog | Remove | Inventory controller reads `Gs1SiteStateView.storages` and `storage_slots`. |
| Dedicated phone panel snapshot | `BEGIN_SITE_PHONE_PANEL_SNAPSHOT`, `SITE_PHONE_PANEL_LISTING_UPSERT`, `SITE_PHONE_PANEL_LISTING_REMOVE`, `END_SITE_PHONE_PANEL_SNAPSHOT` | Legacy phone-panel-only state transfer retained only in the public message catalog | Remove | Phone controller reads `Gs1SiteStateView.phone_listings` and host-owned phone UI/session state. |
| Dedicated task panel snapshot | `BEGIN_SITE_TASK_PANEL_SNAPSHOT` through `END_SITE_TASK_PANEL_SNAPSHOT` | Legacy task/modifier panel transfer retained only in the public message catalog | Remove | Task controller reads `Gs1SiteStateView.tasks` and `active_modifiers`. |
| Site summary snapshot | `BEGIN_SITE_SUMMARY_SNAPSHOT`, `END_SITE_SUMMARY_SNAPSHOT` | Legacy summary-controller snapshot boundary retained only in the public message catalog | Remove | Summary panel reads `Gs1SiteStateView` and `Gs1CampaignStateView` directly. |
| Site HUD snapshot | `BEGIN_SITE_HUD_SNAPSHOT`, `SITE_HUD_STORAGE_UPSERT`, `END_SITE_HUD_SNAPSHOT` | Legacy HUD-local storage mirror retained only in the public message catalog | Remove | HUD reads site state directly. |
| Regional selection dedicated UI panel stream | `BEGIN_REGIONAL_SELECTION_UI_PANEL` through `CLOSE_REGIONAL_SELECTION_UI_PANEL` | Legacy selection-panel authored-content stream retained only in the public message catalog | Remove | Host-owned authored panel plus direct campaign state reads. |
| Craft / placement contextual stream | `SITE_CRAFT_CONTEXT_BEGIN`, `SITE_CRAFT_CONTEXT_OPTION_UPSERT`, `SITE_CRAFT_CONTEXT_END`, `SITE_PLACEMENT_PREVIEW`, `SITE_PLACEMENT_PREVIEW_TILE_UPSERT`, `SITE_PLACEMENT_FAILURE` | Legacy contextual action stream retained only in the public message catalog apart from the still-useful failure cue | Replace selectively | Prefer query/helper APIs for contextual legality and preview data; keep a tiny failure cue only if still needed after the query path exists. |

## Current Producers Worth Shrinking

- No gameplay-side snapshot/upsert/panel/progression producers remain in the current `src/` tree.
- The remaining work is deleting the obsolete public runtime-message families from `include/gs1/types.h` and repairing any downstream smoke/test compatibility code that still names them.

## Follow-Through Rule

- If a runtime message payload is reconstructing durable gameplay state, move that read to `Gs1GameStateView` or a targeted query helper.
- If a runtime message payload is only telling the host that something gameplay-relevant happened once, it can stay as a small semantic cue.
