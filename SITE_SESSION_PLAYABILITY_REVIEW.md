# Site Session Playability Review

Checked: April 14, 2026

Purpose: summarize the current implementation state and define the next work that would make one active site session feel playable.

## Bottom Line

The current build now contains a minimal restoration-session runtime loop, but it is not yet a fully player-facing playable session through the current visual smoke UI.

What works today:

- The game boots into the main menu.
- A campaign can be started.
- The regional map shows the authored four-site prototype chain.
- Site 1 can be selected and entered.
- The runtime enters `SITE_ACTIVE`.
- The visual smoke host receives a site bootstrap snapshot.
- The worker can move around the 12x12 site with live delta projection.
- The runtime seeds Site 1 with starter inventory, one visible task, starter money, and phone listings.
- The runtime accepts site-action host input for `Plant`, `Water`, and `ClearBurial`.
- A planted tile can grow, count toward completion, and finish the site through normal runtime play.
- Passive worker-condition drain and deterministic Site 1 weather phases can fail the site through runtime play.

What is missing for playability:

- The current visual smoke UI still does not expose direct site-action controls, task acceptance, inventory use, or phone actions.
- `Build` and `Repair` actions are still unsupported in the action system.
- Task reward claim flow and reward-side cross-owner effects are still incomplete.
- Phone purchases, contractor hires, and unlockable purchases currently stay inside economy-owned state and do not yet grant full downstream gameplay effects.
- Device and camp systems are present but still mostly passive maintenance slices rather than strong strategic support loops.

The practical current state is a minimal playable loop in code plus a partial presentation/UI surface.

## Live Smoke Snapshot

I launched the current visual smoke host and drove the live flow through:

1. `MAIN_MENU`
2. `START_NEW_CAMPAIGN`
3. `REGIONAL_MAP`
4. `SELECT_DEPLOYMENT_SITE` for Site 1
5. `START_SITE_ATTEMPT`
6. `SITE_ACTIVE`

Current seeded site state after entry:

- App state: `SITE_ACTIVE`
- Selected site: `1`
- Site archetype: `101`
- Grid: `12x12`
- Camp tile: `(4,4)`
- Worker tile: `(4,4)`
- Worker health: `100`
- Worker hydration: `100`
- Worker energy: `100`
- Money: `45`
- Visible task count: `1`
- Active task count: `0`
- Site completion: `0%`
- Weather: seeded Site 1 warning event, roughly heat `15`, wind `10`, dust `5`, event phase `WARNING`
- Starter inventory: worker pack and starter storage crate include seeded occupied slots
- Phone listings: one seeded buy listing, one sell listing, one contractor listing, and one unlockable listing
- Tiles: all default terrain on entry, but they can now change through runtime action flow

Movement check:

- The worker moved from `(4,4)` to about `(7.73,4)` through live site-control input.
- The worker facing updated to `90` degrees.
- HUD values now change over time as worker meters decay.

This confirms that presentation, movement, seeded content, passive pressure, and runtime-side site progression are all active. The main remaining gap is surfacing more of that runtime loop through the current manual UI and completing the still-thin cross-system effects.

## Current Implementation Inventory

### App Flow

Implemented:

- Boot to main menu.
- Start prototype campaign.
- Regional map snapshot.
- Site selection overlay.
- Start site attempt.
- Site result overlay messages exist for completion/failure.
- Return to regional map message exists.

Important limitation:

- Site result can only happen if code marks the site completed or failed. Normal play does not currently change the values that trigger those results.

### Campaign Content

Implemented:

- Four authored prototype sites.
- Linear adjacency: `1 -> 2 -> 3 -> 4`.
- Site 1 starts available.
- Sites 2 to 4 start locked.
- Completion thresholds exist: `10`, `12`, `14`, `16`.
- Each site has a camp anchor tile.

Important limitation:

- Prototype content currently describes site metadata only. It does not yet define tile layouts, plants, items, tasks, phone listings, weather profiles, events, or rewards.

### Site State

Implemented as data containers:

- `SiteRunState`
- `SiteClockState`
- `WorkerState`
- `CampState`
- `TileGridState`
- `InventoryState`
- `ContractorState`
- `WeatherState`
- `EventState`
- `TaskBoardState`
- `ModifierState`
- `EconomyState`
- `ActionState`
- `SiteCounters`

Important limitation:

- The containers now have minimal prototype behavior, but many slices still use hard-coded Site 1 seeding and conservative placeholder values rather than authored content tables.

### Runtime Loop

Implemented:

- Fixed-step accumulation.
- Campaign/site clock advancement.
- Day phase calculation.
- Worker movement from `GS1_HOST_EVENT_SITE_MOVE_DIRECTION`.
- Movement clamping to site bounds.
- Traversability check.
- Dirty projection for worker movement.
- Fixed-step calls to the full current site stack: flow, modifiers, weather/event, actions, local weather, worker condition, camp durability, device maintenance, device support, ecology, inventory, task board, economy/phone, failure recovery, and completion.
- Completion/failure message checks.

Important limitation:

- Many systems still implement a minimal Site 1 slice rather than the full intended ruleset.
- `Build` and `Repair` actions still do not traverse the full placement/device/camp loop.
- Reward claims, inventory grants from purchases, contractor-side outcomes, and unlockable-side effects remain incomplete cross-owner flows.

### Site Systems

Implemented and runtime-wired:

- `SiteFlowSystem`: advances site time/day phase and owns worker tile movement/facing from the per-phase movement input.
- `ModifierSystem`: resolves conservative nearby-aura/run modifier totals for the current prototype.
- `WeatherEventSystem`: seeds and advances a deterministic mild Site 1 harsh-event profile.
- `ActionExecutionSystem`: owns action lifecycle for `Plant`, `Water`, and `ClearBurial`, including placement reservation handoff for plant actions.
- `PlacementValidationSystem`: owns plant-action reservation accept/reject/release flow.
- `LocalWeatherResolveSystem`: resolves per-tile heat/wind/dust from site weather plus simple local cover rules.
- `WorkerConditionSystem`: applies passive drain, accepts worker meter deltas, and drives HUD/worker updates.
- `CampDurabilitySystem`: applies weather/event wear and camp service-flag thresholds.
- `DeviceMaintenanceSystem`: applies passive structure wear from weather and burial.
- `DeviceSupportSystem`: updates device-owned efficiency/stored-water state.
- `EcologySystem`: owns tile occupancy/density/burial changes plus restoration progress.
- `InventorySystem`: seeds starter inventory and supports minimal item use/transfer.
- `TaskBoardSystem`: seeds one Site 1 task, accepts it, and advances/completes it from restoration progress.
- `EconomyPhoneSystem`: seeds Site 1 money and listings and processes minimal economy requests inside economy-owned state.
- `FailureRecoverySystem`: observes failed-site conditions and emits `SiteAttemptEnded` with `FAILED`.
- `SiteCompletionSystem`: observes restoration threshold progress and emits `SiteAttemptEnded` with `COMPLETED`.

Still thin or incomplete:

- `ActionExecutionSystem`: `Build` and `Repair` remain unsupported.
- `PlacementValidationSystem`: currently covers the plant/ground-cover reservation path only.
- `DeviceSupportSystem` and `CampDurabilitySystem`: mostly passive upkeep rather than strong strategic support loops.
- `ContractorState`: still has no dedicated system header/implementation.

Current message-dispatch status:

- The internal message subscriber table now registers the site systems that own current message-handled behavior.
- The translated feedback-event subscriber table still exists, but no gameplay system currently depends on translated feedback events for the prototype site loop.

### Engine Projection

Implemented:

- Regional map snapshot/upsert/link messages.
- Site snapshot begin/end.
- Site tile upsert.
- Site worker update.
- Site camp update.
- Site weather update.
- Site inventory slot upsert.
- Site task upsert.
- Site phone listing upsert.
- HUD state.
- Site result ready.
- Smoke host live JSON for app state, regional map, site bootstrap, site state, inventory, tasks, phone listings, HUD, and site result.
- Visual smoke viewer that renders main menu, regional map, active site placeholder, site tiles, camp, and worker.

Important limitation:

- `GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE` exists in the contract, but the runtime does not emit it yet.
- The smoke host records inventory/task/phone projections, but the current visual smoke viewer still does not present them as interactive in-site surfaces.
- Detailed local-weather, soil, device, and modifier meters are still not projected to a dedicated UI surface.

### Player Input

Implemented:

- UI actions for start campaign, select deployment site, start site attempt, clear deployment site selection, and return to regional map.
- Site movement input through host event.
- Public site-action host events for `GS1_HOST_EVENT_SITE_ACTION_REQUEST` and `GS1_HOST_EVENT_SITE_ACTION_CANCEL`.
- Public UI action ids for task accept, reward claim, phone purchase/sale, inventory use/transfer, contractor hire, and unlockable purchase.

Missing:

- The current visual smoke UI still does not surface those new public host/UI actions as clickable/manual controls.
- There is still no player-facing build/repair control path because the runtime-side build/repair action loop is incomplete.

## Site System Contract Pass

Purpose: freeze the shared message meanings before splitting work across multiple systems. The goal is not to implement every message immediately. The goal is to avoid each system inventing a private coupling path while parallel work is underway.

### Contract Rules

- Internal gameplay coordination should use `GameMessage`; do not add an internal gameplay-event queue.
- Host events are adapter/player ingress only.
- Translated feedback events are engine execution observations only.
- Engine messages are presentation output only.
- Message payloads should stay POD-like, trivial, fixed-size, and inside the current 63-byte `GameMessage` payload budget.
- Message names should describe gameplay meaning, not a private RPC to one named consumer.
- Zero, one, or many systems may subscribe to the same message.
- Each listener may mutate only its owned state and should emit follow-up messages for other ownership domains.
- Site bootstrap should publish a `SiteRunStarted` message after `CampaignFlowSystem` has created `active_site_run_`, so site systems can seed their own state without subscribing to the pre-creation `StartSiteAttempt` intent.

### Ownership Baseline

| System | Owns and may write | Should observe/read | Likely messages published |
|---|---|---|---|
| `SiteFlowSystem` | `SiteClockState`, worker tile position, worker facing | Tile traversability, movement input | `WorkerMoved` when the worker changes tile |
| `ActionExecutionSystem` | `ActionState` | Worker meters, inventory availability, placement validity, tile state | `SiteActionStarted`, `SiteActionCompleted`, `SiteActionFailed`, action result fact messages, worker/inventory cost requests |
| `PlacementValidationSystem` | Placement reservations if we keep persistent reservations | Terrain flags, occupancy, content rules | `PlacementReservationAccepted`, `PlacementReservationRejected`, `PlacementReservationReleased` |
| `WorkerConditionSystem` | Worker health, hydration, nourishment, energy cap, energy, morale, work efficiency, shelter state | Local weather, camp shelter, action facts, modifier totals | `WorkerMetersChanged`, `WorkerShelterChanged`, `WorkerIncapacitated` |
| `WeatherEventSystem` | `WeatherState`, `EventState` | Site clock, forecast content, modifier totals | `WeatherEventScheduled`, `WeatherEventPhaseChanged`, `WeatherPressureChanged` |
| `LocalWeatherResolveSystem` | `tile_heat`, `tile_wind`, `tile_dust` | Site weather, event pressure, plants, devices, terrain, modifiers | `LocalWeatherThresholdChanged` only when a meaningful threshold changes |
| `EcologySystem` | Terrain soil meters, sand burial, plant and ground-cover occupancy, plant density, growth pressure, restoration counters | Local weather, action result facts, device irrigation, modifier totals | `TileEcologyChanged`, `TileRestorationMilestoneReached`, `RestorationProgressChanged` |
| `DeviceSupportSystem` | Structure occupancy, device efficiency, device stored water, device support outputs | Build/repair facts, weather pressure, modifier totals | `DeviceSupportOutputChanged`, `DeviceIrrigationApplied`, `DeviceStoredWaterChanged` |
| `DeviceMaintenanceSystem` | Device integrity and damage/repair state | Weather pressure, burial, repair facts | `DeviceIntegrityChanged`, `DeviceDisabled`, `DeviceRecovered` |
| `CampDurabilitySystem` | Camp durability and camp service flags | Weather pressure, repair facts, site state | `CampStateChanged`, `CampDisabled`, `CampDestroyed` |
| `InventorySystem` | Worker pack slots, device-backed storage slots, pending deliveries, item reservations | Economy purchases, action needs, task rewards, hazard facts | `InventorySlotChanged`, `InventoryItemsReserved`, `InventoryReservationFailed`, `InventoryDeliveryArrived` |
| `EconomyPhoneSystem` | Money, phone listings, site unlockable availability | Task reward grants, inventory sale requests, site run start | `MoneyChanged`, `PhoneListingChanged`, `PhoneListingPurchased`, `PhoneListingRejected`, item/contractor grant requests |
| `TaskBoardSystem` | Visible, accepted, completed, and chain task runtime state | Action result facts, inventory facts, restoration progress, weather milestones | `TaskAccepted`, `TaskProgressChanged`, `TaskCompleted`, `TaskRewardClaimed`, reward grant requests |
| `ModifierSystem` | Active run modifiers, nearby-site aura modifiers, resolved modifier channel totals | Site run start, task rewards, aftermath relief, technology state | `ModifierTotalsChanged`, `ModifierExpired` |
| `FailureRecoverySystem` | Failure transition decision | Worker/camp failure facts, run status | Existing `SiteAttemptEnded` with `FAILED` |
| `SiteCompletionSystem` | Completion transition decision | Restoration progress, run status | Existing `SiteAttemptEnded` with `COMPLETED` |

### Host Event Contract

Existing host events:

| Host event | Current meaning | Current destination |
|---|---|---|
| `GS1_HOST_EVENT_UI_ACTION` | Discrete host-rendered UI selection | Translated to current app/campaign `GameMessage`s |
| `GS1_HOST_EVENT_SITE_MOVE_DIRECTION` | Continuous world-space movement vector for this phase | Runtime transient input consumed by `SiteFlowSystem` |

Implemented public host/API additions:

| Host event or UI action | Payload meaning | Translates to | Notes |
|---|---|---|---|
| `GS1_HOST_EVENT_SITE_ACTION_REQUEST` | `action_kind`, target tile, primary subject id, optional secondary subject id, optional item id, quantity, flags | `StartSiteAction` | Prefer this over packing tile/action data into `Gs1UiAction.arg0/arg1`. Revisit `Gs1HostEventPayload` size asserts if needed. |
| `GS1_HOST_EVENT_SITE_ACTION_CANCEL` | Runtime action id or "current action" sentinel | `CancelSiteAction` | Useful once action timers last long enough to need UX cancellation. |
| `GS1_UI_ACTION_ACCEPT_TASK` | Task instance id | `TaskAcceptRequested` | Can use existing `GS1_HOST_EVENT_UI_ACTION`. |
| `GS1_UI_ACTION_CLAIM_TASK_REWARD` | Task instance id plus selected reward candidate id | `TaskRewardClaimRequested` | Can use existing `Gs1UiAction.target_id/arg0`. |
| `GS1_UI_ACTION_BUY_PHONE_LISTING` | Listing id and quantity | `PhoneListingPurchaseRequested` | Can use existing `Gs1UiAction.target_id/arg0`. |
| `GS1_UI_ACTION_SELL_PHONE_LISTING` | Listing id or item id and quantity | `PhoneListingSaleRequested` | Can use existing `Gs1UiAction.target_id/arg0`. |
| `GS1_UI_ACTION_USE_INVENTORY_ITEM` | Container, slot, quantity | `InventoryItemUseRequested` | Inventory owns item removal and emits worker/effect messages. |
| `GS1_UI_ACTION_TRANSFER_INVENTORY_ITEM` | Source container/slot, destination container/slot, quantity | `InventoryTransferRequested` | Needed once worker pack and device storage are visible. |
| `GS1_UI_ACTION_HIRE_CONTRACTOR` | Listing id or contractor offer id | `ContractorHireRequested` | Economy owns payment, contractor state owns work capacity. |
| `GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE` | Unlockable id | `SiteUnlockablePurchaseRequested` | Economy owns money and available unlockables. |

### Translated Feedback Events

Existing feedback event types:

| Feedback event | Meaning | V0 recommendation |
|---|---|---|
| `GS1_FEEDBACK_EVENT_PHYSICS_CONTACT` | Adapter-translated contact observation | No site gameplay dependency for V0. |
| `GS1_FEEDBACK_EVENT_TRACE_HIT` | Adapter-translated trace observation | Avoid for normal tile actions if the host can resolve tile coordinates before submitting `GS1_HOST_EVENT_SITE_ACTION_REQUEST`. |
| `GS1_FEEDBACK_EVENT_ANIMATION_NOTIFY` | Adapter-translated animation observation | Use only for future execution sync or presentation-sensitive timing, not for authoritative action completion. |

No new translated feedback event is required for the first playable site loop. Action timers, resource checks, tile mutation, task progress, and completion should all be authoritative gameplay messages/state, not engine callbacks.

### Existing Internal Messages To Preserve

| Message | Current publisher | Current subscriber |
|---|---|---|
| `OpenMainMenu` | Runtime boot | `CampaignFlowSystem` |
| `StartNewCampaign` | UI action | `CampaignFlowSystem` |
| `SelectDeploymentSite` | UI action | `CampaignFlowSystem` |
| `ClearDeploymentSiteSelection` | UI action | `CampaignFlowSystem` |
| `DeploymentSiteSelectionChanged` | `CampaignFlowSystem` | `LoadoutPlannerSystem` |
| `StartSiteAttempt` | UI action | `CampaignFlowSystem` |
| `ReturnToRegionalMap` | UI action | `CampaignFlowSystem` |
| `SiteAttemptEnded` | `FailureRecoverySystem`, `SiteCompletionSystem` | `CampaignFlowSystem` |
| `PresentLog` | Any system needing a simple log line | Runtime presentation sync |

### Current Internal Messages By Gameplay Meaning

Lifecycle and site result:

| Message | Payload meaning | Producer | Likely subscribers |
|---|---|---|---|
| `SiteRunStarted` | Site id, site run id, archetype id, attempt index, attempt seed | `CampaignFlowSystem` after creating `active_site_run_` | Weather, ecology, task board, inventory, economy, modifiers, camp, devices |
| `SiteRunEnding` | Site id, result, reason code | Failure/completion or future abort flow | Systems that need to release reservations or stop timers |
| `SiteAttemptEnded` | Site id and result | Existing failure/completion systems | `CampaignFlowSystem`, presentation sync |

Player action lifecycle:

| Message | Payload meaning | Producer | Likely subscribers |
|---|---|---|---|
| `StartSiteAction` | Action kind, target tile, primary subject id, secondary subject id, optional item id, quantity, flags | Host event translation | `ActionExecutionSystem`, optional placement validation |
| `CancelSiteAction` | Action id or current-action sentinel | Host event translation | `ActionExecutionSystem`, inventory reservation release |
| `SiteActionStarted` | Action id, kind, target tile, primary subject id, duration minutes | `ActionExecutionSystem` | Worker condition, HUD/projection, inventory reservation tracking |
| `SiteActionCompleted` | Action id, kind, target tile, primary subject id | `ActionExecutionSystem` | Worker condition, task board, cue/notification projection |
| `SiteActionFailed` | Action id, kind, target tile, reason code | `ActionExecutionSystem` | Inventory reservation release, cue/notification projection |

Action result facts:

| Message | Payload meaning | Producer | Likely subscribers |
|---|---|---|---|
| `SiteGroundCoverPlaced` | Action id, tile, ground-cover type id, initial density | `ActionExecutionSystem` | `EcologySystem`, `TaskBoardSystem`, projection dirtying |
| `SiteTilePlantingCompleted` | Action id, tile, plant id, initial density | `ActionExecutionSystem` | `EcologySystem`, `TaskBoardSystem`, projection dirtying |
| `SiteTileWatered` | Action id/source id, tile, water amount | `ActionExecutionSystem`, `DeviceSupportSystem` | `EcologySystem`, `TaskBoardSystem` |
| `SiteTileBurialCleared` | Action id, tile, cleared amount | `ActionExecutionSystem`, contractors | `EcologySystem`, `TaskBoardSystem` |
| `SiteStructureBuildCompleted` | Action id, tile, structure id, initial integrity | `ActionExecutionSystem`, contractors | Device systems, placement validation, task board |
| `SiteStructureRepairCompleted` | Action id, tile, repair amount | `ActionExecutionSystem`, contractors | Device maintenance, camp durability, task board |

Placement:

| Message | Payload meaning | Producer | Likely subscribers |
|---|---|---|---|
| `PlacementReservationRequested` | Action id, tile, occupancy layer, subject id | `ActionExecutionSystem` | `PlacementValidationSystem` |
| `PlacementReservationAccepted` | Action id, tile, reservation token | `PlacementValidationSystem` | `ActionExecutionSystem` |
| `PlacementReservationRejected` | Action id, tile, reason code | `PlacementValidationSystem` | `ActionExecutionSystem`, notification projection |
| `PlacementReservationReleased` | Reservation token or action id | Action cancel/fail/complete flow | `PlacementValidationSystem` |

Worker condition:

| Message | Payload meaning | Producer | Likely subscribers |
|---|---|---|---|
| `WorkerMeterDeltaRequested` | Source kind/id plus health, hydration, nourishment, energy, morale deltas | Actions, inventory use, hazards, modifiers | `WorkerConditionSystem` |
| `WorkerMetersChanged` | Current meters plus changed-mask | `WorkerConditionSystem` | Failure recovery, task board, HUD/projection |
| `WorkerShelterChanged` | Sheltered flag and source tile/id | `WorkerConditionSystem` | HUD/projection, task board if needed |
| `WorkerIncapacitated` | Site id and reason code | `WorkerConditionSystem` | `FailureRecoverySystem` |
| `WorkerMoved` | Previous tile, new tile, current position | `SiteFlowSystem` | Worker condition, task board, inspection/projection if needed |

Weather, local weather, and harsh events:

| Message | Payload meaning | Producer | Likely subscribers |
|---|---|---|---|
| `WeatherEventScheduled` | Event template id and scheduled start minute | `WeatherEventSystem` | Forecast/notification projection, task board |
| `WeatherEventPhaseChanged` | Event template id, phase, phase minutes remaining, pressure channels | `WeatherEventSystem` | Worker condition, ecology, camp, devices, task board, projection |
| `WeatherPressureChanged` | Site heat, wind, dust, event template id, phase | `WeatherEventSystem` | `LocalWeatherResolveSystem`, HUD/weather projection |
| `LocalWeatherThresholdChanged` | Tile or area id plus heat/wind/dust threshold bands | `LocalWeatherResolveSystem` | Worker condition, ecology, task board |
| `AftermathReliefOffered` | Event template id, faction id, relief offer id | Weather/event or task/economy integration | Economy, task board, inventory, modifiers |

Ecology and restoration:

| Message | Payload meaning | Producer | Likely subscribers |
|---|---|---|---|
| `TileEcologyChanged` | Tile, changed-mask, plant id, ground-cover id, density, burial | `EcologySystem` | Task board, projection dirtying |
| `TileRestorationMilestoneReached` | Tile, milestone kind, plant id, density | `EcologySystem` | Task board, notifications |
| `RestorationProgressChanged` | Fully grown tile count, completion threshold, normalized progress | `EcologySystem` | `SiteCompletionSystem`, task board, HUD/projection |
| `EcologyOutputProduced` | Tile/source id, item id, quantity | `EcologySystem` | Inventory, task board, economy if sale value is generated |

Inventory:

| Message | Payload meaning | Producer | Likely subscribers |
|---|---|---|---|
| `InventoryItemGrantRequested` | Item id, quantity, target container, reason code | Task rewards, phone purchases, ecology output, support packages | `InventorySystem` |
| `InventoryItemRemoveRequested` | Item id, quantity, container mask, reason code | Selling, action costs, spoilage/hazard flow | `InventorySystem` |
| `InventoryItemReservationRequested` | Reservation/action id, item id, quantity, container mask | `ActionExecutionSystem` | `InventorySystem` |
| `InventoryItemsReserved` | Reservation/action id, item id, quantity, source container/slot | `InventorySystem` | `ActionExecutionSystem` |
| `InventoryReservationFailed` | Reservation/action id, item id, quantity, reason code | `InventorySystem` | `ActionExecutionSystem`, notification projection |
| `InventoryReservedItemsConsumed` | Reservation/action id | Action completion flow | `InventorySystem` |
| `InventoryReservationReleased` | Reservation/action id | Action cancel/fail flow | `InventorySystem` |
| `InventoryTransferRequested` | Source container/slot, destination container/slot, quantity | UI action | `InventorySystem` |
| `InventoryItemUseRequested` | Container/slot, item id, quantity | UI action | `InventorySystem` |
| `InventorySlotChanged` | Container kind, slot index, item id, quantity, condition, freshness | `InventorySystem` | Task board, projection |
| `InventoryDeliveryQueued` | Delivery id, item bundle id or item id/quantity, minutes until arrival | Economy/support systems | `InventorySystem`, projection |
| `InventoryDeliveryArrived` | Delivery id and target container | `InventorySystem` | Camp, projection, notifications |

Economy, phone, unlockables, and contractors:

| Message | Payload meaning | Producer | Likely subscribers |
|---|---|---|---|
| `MoneyDeltaRequested` | Amount delta and reason code | Task rewards, penalties, debug/test hooks | `EconomyPhoneSystem` |
| `MoneyChanged` | Current money and delta | `EconomyPhoneSystem` | HUD/projection, task board |
| `PhoneListingPurchaseRequested` | Listing id and quantity | UI action | `EconomyPhoneSystem` |
| `PhoneListingSaleRequested` | Listing id or item id and quantity | UI action | `EconomyPhoneSystem`, inventory removal flow |
| `PhoneListingPurchased` | Listing id, quantity, total price, item/unlockable id | `EconomyPhoneSystem` | Inventory, contractors, modifiers, projection |
| `PhoneListingRejected` | Listing id and reason code | `EconomyPhoneSystem` | Notification projection |
| `PhoneListingChanged` | Listing id, kind, item/unlockable id, price, quantity, flags | `EconomyPhoneSystem` | Phone projection |
| `SiteUnlockableRevealed` | Unlockable id and source reason | Task board, campaign support, economy | `EconomyPhoneSystem`, phone projection |
| `SiteUnlockablePurchaseRequested` | Unlockable id | UI action | `EconomyPhoneSystem` |
| `SiteUnlockablePurchased` | Unlockable id and price | `EconomyPhoneSystem` | Modifier, inventory, task board |
| `ContractorHireRequested` | Listing/offer id and work-unit amount | UI action | `EconomyPhoneSystem` |
| `ContractorWorkUnitsChanged` | Available work units and delta | Economy/contractor flow | Contractor/work-order projection |
| `ContractorWorkOrderRequested` | Work order id, task kind, target tile, work units | UI action or task flow | Contractor/action flow |
| `ContractorWorkOrderCompleted` | Work order id, task kind, target tile | Contractor/action flow | Ecology, device maintenance, task board |

Task board and rewards:

| Message | Payload meaning | Producer | Likely subscribers |
|---|---|---|---|
| `TaskAcceptRequested` | Task instance id | UI action | `TaskBoardSystem` |
| `TaskAccepted` | Task instance id and template id | `TaskBoardSystem` | Projection, notifications |
| `TaskAbandonRequested` | Task instance id | UI action | `TaskBoardSystem` |
| `TaskProgressChanged` | Task instance id, current progress, target progress | `TaskBoardSystem` | Task projection, notifications |
| `TaskCompleted` | Task instance id, template id, faction id | `TaskBoardSystem` | Projection, reward availability, faction reputation |
| `TaskRewardClaimRequested` | Task instance id and selected reward candidate id | UI action | `TaskBoardSystem` |
| `TaskRewardClaimed` | Task instance id and selected reward candidate id | `TaskBoardSystem` | Economy, inventory, modifiers, faction reputation, projection |
| `FactionReputationDeltaRequested` | Faction id, amount, reason code | Task board, aftermath flow | Future faction reputation system |
| `RunModifierGrantRequested` | Modifier id, source id, duration rule | Task rewards, site unlockables, aftermath relief | `ModifierSystem` |

Devices, camp, and modifiers:

| Message | Payload meaning | Producer | Likely subscribers |
|---|---|---|---|
| `DeviceSupportOutputChanged` | Tile/source id and support channel summary | Device support | Local weather, ecology, projection |
| `DeviceIrrigationApplied` | Source structure id/tile, target tile, water amount | Device support | Ecology, task board |
| `DeviceIntegrityChanged` | Tile, structure id, integrity, reason code | Device maintenance | Projection, task board, failure/recovery if critical |
| `DeviceStoredWaterChanged` | Tile, structure id, stored water | Device support/inventory transfer | Projection, task board |
| `DeviceDisabled` | Tile, structure id, reason code | Device maintenance | Projection, task board, notifications |
| `DeviceRecovered` | Tile, structure id | Device maintenance | Projection, task board |
| `CampStateChanged` | Durability, service flags, changed-mask | Camp durability | Worker condition, projection |
| `CampDisabled` | Disabled service flags and reason code | Camp durability | Worker condition, inventory/delivery flow, notification projection |
| `CampDestroyed` | Site id and reason code | Camp durability | Failure recovery |
| `ModifierTotalsChanged` | Changed channel mask plus current totals id/revision | `ModifierSystem` | Weather, worker condition, ecology, local weather |
| `ModifierExpired` | Modifier id and source reason | `ModifierSystem` | Projection/notifications |

### Current Frozen V0 Contract

Freeze these first because they unblock the shortest playable slice:

- `SiteRunStarted`
- `GS1_HOST_EVENT_SITE_ACTION_REQUEST`
- `StartSiteAction`
- `CancelSiteAction`
- `SiteActionStarted`
- `SiteActionCompleted`
- `SiteActionFailed`
- `SiteGroundCoverPlaced`
- `SiteTilePlantingCompleted`
- `SiteTileWatered`
- `SiteTileBurialCleared`
- `WorkerMeterDeltaRequested`
- `WorkerMetersChanged`
- `TileEcologyChanged`
- `RestorationProgressChanged`
- Existing `SiteAttemptEnded`

Defer inventory reservations, phone economy, contractors, and task reward drafts until action, ecology, worker meters, and completion are working end to end.

### Engine Projection Contract

Existing engine message types already cover most first playable projections:

- `GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT`
- `GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE`
- `GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE`
- `GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE`
- `GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT`
- `GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT`
- `GS1_ENGINE_MESSAGE_SITE_TASK_REMOVE`
- `GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT`
- `GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE`
- `GS1_ENGINE_MESSAGE_HUD_STATE`
- `GS1_ENGINE_MESSAGE_NOTIFICATION_PUSH`
- `GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE`

Projection gaps still open:

- Add a `GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE` or extend an existing projection if action progress needs visible progress, because worker/HUD payloads currently carry `current_action_kind` but not action id, target tile, or progress.
- Keep V0 tile projection using existing `plant_type_id`, `ground_cover_type_id`, `plant_density`, and `sand_burial`.
- Defer projecting detailed moisture, fertility, salinity, heat, wind, dust, growth pressure, and device meters until the UI has a concrete surface for those values.

## Playability Gap

For one site session to feel playable, the player needs a loop where intent changes the world and the world pushes back.

The current build has:

- place: yes
- avatar movement: yes
- visible site state: partial but much broader than before
- goals: yes, minimal seeded task plus completion threshold
- actions: yes through the public host contract, but not yet fully surfaced in the visual smoke UI
- pressure: yes, minimal worker-condition drain plus deterministic weather phases
- economy: yes, minimal seeded money and phone listings
- rewards: partial
- win/loss by play: yes in runtime, but still awkward to drive manually through the current visual UI

Minimum playable loop needed:

1. Enter a site with a visible immediate goal.
2. Move to a tile.
3. Select a tile/action.
4. Spend time/resources to change that tile.
5. See tile, HUD, and task progress update.
6. Weather/time creates survival or restoration pressure.
7. Repeating the loop can complete or fail the site.

That loop now exists in the runtime. The remaining blocker is exposing enough of it through the current player-facing tools and finishing the still-thin cross-owner outcomes.

## Recommended Next Implementation Sequence

### 1. Surface The Existing Runtime Loop In The Visual Smoke UI

Goal: make the current code-side playable loop reachable by a tester without custom host-event injection.

Next work:

- Add visual controls for `GS1_HOST_EVENT_SITE_ACTION_REQUEST`.
- Add UI surfaces for seeded tasks, inventory, and phone listings.
- Add task accept and inventory/phone interaction controls.
- Expose current action progress once `SITE_ACTION_UPDATE` is emitted.

### 2. Finish The Structure/Placement Side Of The Action Loop

Goal: move beyond the current plant/water/clear-burial slice.

Next work:

- Add real `Build` and `Repair` action support.
- Expand placement validation beyond the current plant/ground-cover reservation path.
- Emit and consume structure-side action facts for camp/device systems.
- Project visible structure-side outcomes.

### 3. Finish Cross-Owner Outcome Flow For Rewards And Purchases

Goal: make tasks and phone choices materially affect the site.

Next work:

- Add reward-claim resolution from task completion into money/items/modifiers/unlockables.
- Add inventory grants/removals for purchases and sales.
- Add contractor-side outcome flow.
- Let unlockable purchases produce downstream gameplay effects outside economy-owned state.

### 4. Deepen Device And Camp Support Effects

Goal: turn the current passive device/camp slices into meaningful strategy.

Next work:

- Let device support materially affect ecology or worker pressure through explicit messages/state observation.
- Make camp shelter/protection meaningfully change local survival pressure.
- Add stronger recovery and stabilization loops around damaged support infrastructure.

### 5. Expand Content And Site Tuning

Goal: move from a hard-coded Site 1 slice toward authored breadth.

Next work:

- Replace hard-coded Site 1 seeding with authored content rows where possible.
- Add more terrain/site variation.
- Add harsher later-site profiles after the first site loop feels good.
- Tune thresholds and drain rates so the loop feels intentional instead of merely functional.

## Suggested Immediate Sprint

The best next sprint is:

1. Surface plant/water/clear-burial in the visual smoke UI.
2. Surface seeded tasks, inventory, and phone listings in the visual smoke UI.
3. Emit and project explicit action-progress state.
4. Finish reward-claim and purchase-outcome cross-owner effects.
5. Add real build/repair structure actions on top of the new placement reservation contract.

This would turn the current slice from "runtime-playable but awkward to drive manually" into "visibly playable through the actual debug front end."

## Acceptance Criteria For "Playable Site Session V0"

A site session should not be considered playable until all of these are true:

- A tester can enter Site 1 from the visual smoke UI.
- A tester can move to a target tile.
- A tester can perform at least one restoration action.
- The target tile visibly changes in the live site view.
- HUD meters change as time/actions pass.
- At least one visible task or goal explains what to do next.
- The player can make progress toward site completion through normal actions.
- The site can reach `SITE_RESULT` with `COMPLETED` through normal play.
- The site can reach `SITE_RESULT` with `FAILED` through normal pressure or a deterministic test hook.
- A smoke test covers the happy path from main menu to completed Site 1.

Current status against those criteria:

- Runtime-side action, pressure, progress, and fail/win paths now exist.
- The remaining blocker is that the current visual smoke UI still does not expose enough in-site controls to let a tester exercise the full loop manually.

## Risks To Watch

- Do not build the full task board before there are real action/ecology events for tasks to track.
- Do not expand regional/campaign complexity before Site 1 has a satisfying action-pressure-progress loop.
- Keep prototype content static until the shape is fun; a loader/generator can come later.
- Keep new message payloads POD-like and small, matching the existing message style.
- Every gameplay state mutation should mark the right projection dirty, or the visual smoke UI will hide working gameplay.

## Decision Recommendation

Make Site 1 playable before broadening anything else.

The current foundation is good: app state, map flow, site entry, movement, projection, and live smoke UI are in place. The next valuable work is the first complete gameplay loop inside `SITE_ACTIVE`: action message, tile mutation, meter cost, growth/progress, and completion.
