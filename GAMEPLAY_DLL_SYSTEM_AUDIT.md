# Gameplay DLL System Audit

This document audits the systems that are actually instantiated in the gameplay DLL runtime registry in [src/runtime/game_runtime.cpp](/D:/testgame/gs1_upstream/src/runtime/game_runtime.cpp:392).

Scope:
- Only systems registered by `GameRuntime::initialize_system_registry()` are included.
- `RegionalSupportSystem` is not included because it is only a placeholder header and is not registered in the live DLL.
- "Subscribed message" means internal `GameMessage` subscriptions.
- "Subscribed host message" means `Gs1HostMessage` subscriptions at the DLL boundary.
- "Sends" lists internal `GameMessage` traffic emitted by the system implementation.
- For site systems, "ECS data" below uses the authoritative `SiteSystemAccess` contract from [src/site/systems/site_system_context.h](/D:/testgame/gs1_upstream/src/site/systems/site_system_context.h:24). Where the implementation obviously uses sparse flecs data, that is called out briefly.
- No gameplay system in the current `src/` tree directly calls `push_runtime_message(...)`; host-visible output is primarily produced through owned state, the exported game-state view, and runtime-managed queues.

## Runtime Registry

Registered gameplay systems:

1. `CampaignFlowSystem`
2. `LoadoutPlannerSystem`
3. `FactionReputationSystem`
4. `TechnologySystem`
5. `ActionExecutionSystem`
6. `WeatherEventSystem`
7. `WorkerConditionSystem`
8. `EcologySystem`
9. `PlantWeatherContributionSystem`
10. `DeviceWeatherContributionSystem`
11. `TaskBoardSystem`
12. `PlacementValidationSystem`
13. `LocalWeatherResolveSystem`
14. `DeviceMaintenanceSystem`
15. `InventorySystem`
16. `CraftSystem`
17. `EconomyPhoneSystem`
18. `CampDurabilitySystem`
19. `DeviceSupportSystem`
20. `ModifierSystem`
21. `CampaignTimeSystem`
22. `SiteTimeSystem`
23. `SiteFlowSystem`
24. `FailureRecoverySystem`
25. `SiteCompletionSystem`

## Campaign Systems

### `CampaignFlowSystem`
- Functionality: owns app/campaign flow, boots campaigns, manages deployment-site selection, creates site runs, tears site runs down, and transitions to site-result/regional states.
- Subscribed game messages: `OpenMainMenu`, `StartNewCampaign`, `SelectDeploymentSite`, `ClearDeploymentSiteSelection`, `StartSiteAttempt`, `ReturnToRegionalMap`, `SiteAttemptEnded`
- Subscribed host messages: `GS1_HOST_EVENT_GAMEPLAY_ACTION`
- Subscription usage: `OpenMainMenu` returns to main menu; `StartNewCampaign` seeds a fresh campaign; `SelectDeploymentSite` / `ClearDeploymentSiteSelection` update selection and emit `DeploymentSiteSelectionChanged`; `StartSiteAttempt` creates the active site run and emits `SiteRunStarted`; `ReturnToRegionalMap` tears the site run down; `SiteAttemptEnded` resolves success/failure and rewards; `GS1_HOST_EVENT_GAMEPLAY_ACTION` now handles those actions directly before emitting the same canonical follow-up messages.
- Sends: `SiteSceneActivated`, `DeploymentSiteSelectionChanged`, `SiteRunStarted`, `CampaignReputationAwardRequested`, `FactionReputationAwardRequested`
- Owns game state: `AppState`, `CampaignCore`, `CampaignRegionalMapMeta`, `CampaignRegionalMapRevealedSites`, `CampaignRegionalMapAvailableSites`, `CampaignRegionalMapCompletedSites`, `CampaignSiteMetaEntries`, `CampaignSiteAdjacentIds`, `CampaignSiteExportedSupportItems`, `CampaignSiteNearbyAuraModifierIds`
- Accesses game state: `RuntimeAppStateTag`, `RuntimeCampaignFactionProgressTag`; directly seeds and clears the active `Site*` split state sets plus `MoveDirection` during site-run start/teardown.
- ECS data: none directly; it bootstraps and swaps the active `SiteWorld` handle.
- Marked issue: this system directly resets and seeds many `Site*` state sets plus `MoveDirection` during orchestration, but those state sets are not listed in its declared `owned_state_sets()`. In practice it acts as a cross-domain bootstrap owner, but under a strict ownership reading this is still an ownership exception.
### `CampaignTimeSystem`
- Functionality: advances campaign elapsed minutes and remaining days on fixed step.
- Subscribed game messages: none
- Subscribed host messages: none
- Subscription usage: none.
- Sends: none
- Owns game state: none declared
- Accesses game state: `RuntimeFixedStepSecondsTag`; mutates `CampaignCore` in implementation.
- ECS data: none
- Marked issue: this system mutates `CampaignCore` even though `CampaignFlowSystem` is the declared owner. That is an ownership mismatch in the current codebase.

### `FactionReputationSystem`
- Functionality: applies faction-reputation awards and updates per-faction progress.
- Subscribed game messages: `FactionReputationAwardRequested`
- Subscribed host messages: none
- Subscription usage: `FactionReputationAwardRequested` applies faction reputation gains and assistant-package unlock checks.
- Sends: none
- Owns game state: `CampaignFactionProgress`
- Accesses game state: `RuntimeCampaignFactionProgressTag`
- ECS data: none

### `LoadoutPlannerSystem`
- Functionality: rebuilds the selected deployment loadout and site-exported support package from current campaign selection.
- Subscribed game messages: `DeploymentSiteSelectionChanged`
- Subscribed host messages: none
- Subscription usage: `DeploymentSiteSelectionChanged` rebuilds planner state from the canonical validated selection result.
- Sends: none
- Owns game state: `CampaignLoadoutPlannerMeta`, `CampaignLoadoutPlannerBaselineItems`, `CampaignLoadoutPlannerAvailableSupportItems`, `CampaignLoadoutPlannerSelectedSlots`, `CampaignLoadoutPlannerNearbyAuraModifiers`
- Accesses game state: reads campaign site metadata/adjacency through split-state assembly to rebuild the currently selected site package.
- ECS data: none

### `TechnologySystem`
- Functionality: tracks total-reputation unlock progression and faction-tech unlock state; also handles tech claim/refund intents from host gameplay actions.
- Subscribed game messages: `CampaignReputationAwardRequested`
- Subscribed host messages: `GS1_HOST_EVENT_GAMEPLAY_ACTION`
- Subscription usage: `CampaignReputationAwardRequested` adds total reputation; `GS1_HOST_EVENT_GAMEPLAY_ACTION` validates claim/refund tech actions directly.
- Sends: none
- Owns game state: `CampaignTechnology`
- Accesses game state: `RuntimeCampaignTechnologyTag`; reads campaign existence and interacts with faction progress and authored tech definitions for unlock checks.
- ECS data: none

## Site Systems

### `ActionExecutionSystem`
- Functionality: starts, ticks, completes, and cancels site actions including planting, watering, harvest, excavation, crafting, repair, and placement-mode commit flow.
- Subscribed game messages: `StartSiteAction`, `CancelSiteAction`, `PlacementModeCursorMoved`, `PlacementReservationAccepted`, `PlacementReservationRejected`
- Subscribed host messages: `GS1_HOST_EVENT_SITE_ACTION_REQUEST`, `GS1_HOST_EVENT_SITE_ACTION_CANCEL`, `GS1_HOST_EVENT_SITE_CONTEXT_REQUEST`
- Subscription usage: `StartSiteAction` starts an action request; `CancelSiteAction` cancels action/placement state; `PlacementModeCursorMoved` updates placement targeting; `PlacementReservationAccepted` commits reservation state and may begin execution; `PlacementReservationRejected` fails the pending placement; `GS1_HOST_EVENT_SITE_ACTION_REQUEST` / `GS1_HOST_EVENT_SITE_ACTION_CANCEL` are handled directly as action start/cancel input; `GS1_HOST_EVENT_SITE_CONTEXT_REQUEST` feeds placement cursor updates while placement mode is active.
- Sends: `SiteActionStarted`, `SiteActionCompleted`, `WorkerMeterDeltaRequested`, `SiteActionFailed`, `PlacementReservationRequested`, `PlacementReservationReleased`, `PlacementModeCommitRejected`, `InventoryItemConsumeRequested`, `SiteTilePlantingCompleted`, `SiteGroundCoverPlaced`, `SiteTileWatered`, `SiteTileBurialCleared`, `SiteDevicePlaced`, `InventoryCraftCommitRequested`, `SiteDeviceRepaired`, `InventoryWorkerPackInsertRequested`, `SiteTileHarvested`
- Owns game state: `SiteActionMeta`, `SiteActionReservedInputItemStacks`, `SiteActionResolvedHarvestOutputs`
- Accesses game state: `RuntimeCampaignFactionProgressTag`, `RuntimeFixedStepSecondsTag`, `RuntimeMoveDirectionTag`; reads `Time`, `TileLayout`, `TileExcavation`, `TileWeather`, `DeviceCondition`, `WorkerMotion`, `WorkerNeeds`, `Inventory`, `Modifier`, `Craft`, `Action`.
- ECS data: reads `Time`, `TileLayout`, `TileExcavation`, `TileWeather`, `DeviceCondition`, `WorkerMotion`, `WorkerNeeds`, `Inventory`, `Modifier`, `Craft`, `Action`; owns `TileExcavation`, `Action`.

### `WeatherEventSystem`
- Functionality: advances authored weather-event timelines and writes current site weather/event state.
- Subscribed game messages: `SiteRunStarted`
- Subscribed host messages: none
- Subscription usage: `SiteRunStarted` seeds baseline site weather, forecast profile, and repeating-wave timers for the objective.
- Sends: none, except debug `PresentLog` probes in startup/weather diagnostics.
- Owns game state: `SiteWeather`, `SiteEvent`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `Objective`, `Weather`, `Event`.
- ECS data: reads `RunMeta`, `Objective`, `Weather`, `Event`; owns `Weather`, `Event`.

### `WorkerConditionSystem`
- Functionality: resolves worker meter changes, passive losses/recovery, and derived energy-cap/work-efficiency snapshots.
- Subscribed game messages: `SiteRunStarted`, `WorkerMeterDeltaRequested`, `InventoryItemUseCompleted`
- Subscribed host messages: none
- Subscription usage: `SiteRunStarted` emits the initial worker-meter snapshot; `WorkerMeterDeltaRequested` applies gameplay meter deltas; `InventoryItemUseCompleted` converts item-use effects into worker-condition changes.
- Sends: `WorkerMetersChanged`
- Owns game state: none declared by state-set list beyond site-component ownership mapping for `WorkerNeeds`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `TileWeather`, `WorkerMotion`, `WorkerNeeds`, `Action`.
- ECS data: reads `RunMeta`, `TileWeather`, `WorkerMotion`, `WorkerNeeds`, `Action`; owns `WorkerNeeds`.

### `EcologySystem`
- Functionality: updates plant density, watering, burial, support-driven growth pressure, restoration progress, and living-plant stability.
- Subscribed game messages: `SiteRunStarted`, `SiteGroundCoverPlaced`, `SiteTilePlantingCompleted`, `SiteTileWatered`, `SiteTileBurialCleared`, `SiteTileHarvested`
- Subscribed host messages: none
- Subscription usage: `SiteRunStarted` emits startup ecology snapshots; the tile-action messages apply ecology mutations for ground cover, planting, watering, burial clearing, and harvest results.
- Sends: `TileEcologyBatchChanged`, `RestorationProgressChanged`, `LivingPlantStabilityChanged`, debug `PresentLog`
- Owns game state: `SiteCounters`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `TileLayout`, `TileEcology`, `TileWeather`, `TilePlantWeatherContribution`, `TileDeviceWeatherContribution`, `Modifier`, `Counters`, `Objective`.
- ECS data: reads `TileLayout`, `TileEcology`, `TileWeather`, `TilePlantWeatherContribution`, `TileDeviceWeatherContribution`, `Modifier`, `Counters`, `Objective`; owns `TileEcology`, `Counters`.
- Notable flecs usage: sparse occupied-tile and ecology-related tile entities in the site world.

### `PlantWeatherContributionSystem`
- Functionality: maintains plant-owned per-tile weather contribution fields and dirty-tile rebuild queues.
- Subscribed game messages: `SiteRunStarted`, `TileEcologyChanged`, `TileEcologyBatchChanged`
- Subscribed host messages: none
- Subscription usage: `SiteRunStarted` forces a full rebuild; `TileEcologyChanged` / `TileEcologyBatchChanged` mark affected tiles dirty when occupancy or density changed.
- Sends: none
- Owns game state: `SitePlantWeatherContributionMeta`, `SitePlantWeatherContributionDirtyTileIndices`, `SitePlantWeatherContributionDirtyTileMask`
- Accesses game state: reads `TileLayout`, `TileEcology`, `TilePlantWeatherContribution`, `Weather`, `PlantWeatherRuntime`.
- ECS data: reads `TileLayout`, `TileEcology`, `TilePlantWeatherContribution`, `Weather`, `PlantWeatherRuntime`; owns `TilePlantWeatherContribution`, `PlantWeatherRuntime`.
- Notable flecs usage: writes `site_ecs::TilePlantWeatherContribution`.

### `DeviceWeatherContributionSystem`
- Functionality: maintains device-owned per-tile weather contribution fields and dirty-tile rebuild queues.
- Subscribed game messages: `SiteRunStarted`, `SiteDevicePlaced`, `SiteDeviceBroken`, `SiteDeviceRepaired`, `SiteDeviceConditionChanged`
- Subscribed host messages: none
- Subscription usage: `SiteRunStarted` forces a full rebuild; the device messages mark tiles dirty after placement, breakage, repair, or integrity/condition changes.
- Sends: none
- Owns game state: `SiteDeviceWeatherContributionMeta`, `SiteDeviceWeatherContributionDirtyTileIndices`, `SiteDeviceWeatherContributionDirtyTileMask`
- Accesses game state: reads `TileLayout`, `TileDeviceWeatherContribution`, `DeviceCondition`, `DeviceRuntime`, `Weather`, `DeviceWeatherRuntime`.
- ECS data: reads `TileLayout`, `TileDeviceWeatherContribution`, `DeviceCondition`, `DeviceRuntime`, `Weather`, `DeviceWeatherRuntime`; owns `TileDeviceWeatherContribution`, `DeviceWeatherRuntime`.
- Notable flecs usage: writes `site_ecs::TileDeviceWeatherContribution`.

### `TaskBoardSystem`
- Functionality: seeds onboarding tasks, refreshes board tasks, tracks task progress from cross-system messages, and resolves task reward claims.
- Subscribed game messages: `SiteRunStarted`, `SiteRefreshTick`, `TaskAcceptRequested`, `TaskRewardClaimRequested`, `RestorationProgressChanged`, `TileEcologyChanged`, `TileEcologyBatchChanged`, `LivingPlantStabilityChanged`, `SiteTileStateChanged`, `WorkerMetersChanged`, `PhoneListingPurchased`, `PhoneListingSold`, `InventoryTransferCompleted`, `InventoryItemSubmitted`, `InventoryItemUseCompleted`, `InventoryCraftCompleted`, `SiteTilePlantingCompleted`, `SiteActionCompleted`, `SiteDevicePlaced`, `SiteDeviceConditionChanged`, `EconomyMoneyAwardRequested`
- Subscribed host messages: `GS1_HOST_EVENT_GAMEPLAY_ACTION`
- Subscription usage: `SiteRunStarted` seeds startup/onboarding tasks; `SiteRefreshTick` refreshes the task pool; `TaskAcceptRequested` / `TaskRewardClaimRequested` resolve accept/claim commands; the restoration/ecology/stability/tile/worker/inventory/economy/action/device messages all update task progress from their respective simulation changes; `GS1_HOST_EVENT_GAMEPLAY_ACTION` handles accept/claim host actions directly.
- Sends: `EconomyMoneyAwardRequested`, `InventoryDeliveryRequested`, `SiteUnlockableRevealRequested`, `RunModifierAwardRequested`, `CampaignReputationAwardRequested`, `FactionReputationAwardRequested`
- Owns game state: `SiteTaskBoardMeta`, `SiteTaskBoardVisibleTasks`, `SiteTaskBoardRewardDraftOptions`, `SiteTaskBoardTrackedTiles`, `SiteTaskBoardAcceptedTaskIds`, `SiteTaskBoardCompletedTaskIds`, `SiteTaskBoardClaimedTaskIds`
- Accesses game state: `RuntimeCampaignFactionProgressTag`, `RuntimeCampaignTechnologyTag`, `RuntimeFixedStepSecondsTag`; reads `TaskBoard`, `TileLayout`, `Counters`, `Objective`.
- ECS data: reads `TaskBoard`, `TileLayout`, `Counters`, `Objective`; owns `TaskBoard`.
### `PlacementValidationSystem`
- Functionality: validates placement reservations/releases and answers whether a placement can be committed.
- Subscribed game messages: `SiteRunStarted`, `PlacementReservationRequested`, `PlacementReservationReleased`
- Subscribed host messages: none
- Subscription usage: `SiteRunStarted` initializes reservation state; `PlacementReservationRequested` validates a reservation request and emits accepted/rejected; `PlacementReservationReleased` frees a reservation token.
- Sends: `PlacementReservationAccepted`, `PlacementReservationRejected`
- Owns game state: none declared
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `TileLayout`, `TileEcology`.
- ECS data: reads `RunMeta`, `TileLayout`, `TileEcology`; owns none.

### `LocalWeatherResolveSystem`
- Functionality: combines base weather plus plant/device contribution channels into final tile local-weather state and dirty-tile updates.
- Subscribed game messages: `SiteRunStarted`
- Subscribed host messages: none
- Subscription usage: `SiteRunStarted` sets a flag to emit a full local-weather snapshot on the next run.
- Sends: `SiteTileStateChanged`, debug `PresentLog`
- Owns game state: `SiteLocalWeatherResolveMeta`, `SiteLocalWeatherResolveLastTotalContributions`
- Accesses game state: reads `TileLayout`, `TileEcology`, `TileWeather`, `TilePlantWeatherContribution`, `TileDeviceWeatherContribution`, `Weather`, `Modifier`, `LocalWeatherRuntime`.
- ECS data: reads `TileLayout`, `TileEcology`, `TileWeather`, `TilePlantWeatherContribution`, `TileDeviceWeatherContribution`, `Weather`, `Modifier`, `LocalWeatherRuntime`; owns `TileWeather`, `LocalWeatherRuntime`.

### `DeviceMaintenanceSystem`
- Functionality: applies weather-driven device wear, marks broken devices, and reflects repair/integrity changes.
- Subscribed game messages: `SiteRunStarted`, `SiteDevicePlaced`, `SiteDeviceRepaired`
- Subscribed host messages: none
- Subscription usage: `SiteRunStarted` emits initial device-condition messages for existing devices; `SiteDevicePlaced` initializes placed-device condition/runtime state; `SiteDeviceRepaired` restores repaired-device integrity and re-emits condition changes.
- Sends: `SiteDeviceBroken`, `SiteDeviceConditionChanged`
- Owns game state: none declared by state-set list beyond site-component ownership mapping for `DeviceCondition`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `TileLayout`, `TileEcology`, `DeviceCondition`, `Weather`.
- ECS data: reads `TileLayout`, `TileEcology`, `DeviceCondition`, `Weather`; owns `DeviceCondition`.
- Notable flecs usage: sparse device iteration over `TileCoordComponent`, `DeviceStructureId`, `DeviceIntegrity`, `DeviceFixedIntegrity`, `DeviceTag`.
- Marked issue: this system also mutates `SiteWorld::TileDeviceData` through `site_world->set_tile_device(...)` while only declaring ownership of `DeviceCondition`. That bypasses the narrower ownership contract and effectively writes tile/device runtime data without declaring it.

### `InventorySystem`
- Functionality: owns storage containers, worker pack, deliveries, use/consume/submit/transfer/craft-commit resolution, and site-start loadout insertion.
- Subscribed game messages: `StartSiteAction`, `SiteRunStarted`, `SiteDevicePlaced`, `SiteDeviceBroken`, `InventoryDeliveryRequested`, `InventoryDeliveryBatchRequested`, `InventoryWorkerPackInsertRequested`, `InventoryItemUseRequested`, `InventoryItemConsumeRequested`, `InventoryGlobalItemConsumeRequested`, `InventoryTransferRequested`, `InventoryItemSubmitRequested`, `InventorySlotTapped`, `InventoryCraftCommitRequested`
- Subscribed host messages: `GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP`
- Subscription usage: `StartSiteAction` is used for action-linked inventory preparation; `SiteRunStarted` seeds inventory from the selected loadout; `SiteDevicePlaced` / `SiteDeviceBroken` maintain device storage; the `Inventory*Requested` messages resolve deliveries, insert/use/consume/transfer/submit/craft actions; `InventorySlotTapped` resolves slot interactions; `GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP` is handled directly as a slot-tap command.
- Sends: `InventoryItemUseCompleted`, `StartSiteAction`, `InventoryCraftCompleted`, `InventoryTransferCompleted`, `InventoryItemSubmitted`
- Owns game state: `SiteInventoryMeta`, `SiteInventoryStorageContainers`, `SiteInventoryStorageSlotItemIds`, `SiteInventoryWorkerPackSlots`, `SiteInventoryPendingDeliveries`, `SiteInventoryPendingDeliveryItemStacks`
- Accesses game state: `RuntimeCampaignFactionProgressTag`, `RuntimeCampaignTechnologyTag`; reads `RunMeta`, `TileLayout`, `WorkerMotion`, `Inventory`, `Action`.
- ECS data: reads `RunMeta`, `TileLayout`, `WorkerMotion`, `Inventory`, `Action`; owns `Inventory`.

### `CraftSystem`
- Functionality: maintains craft-device caches, nearby-item caches, phone inventory cache, and craft-context option projection.
- Subscribed game messages: `SiteRunStarted`, `SiteDevicePlaced`, `SiteDeviceBroken`, `InventoryCraftContextRequested`
- Subscribed host messages: `GS1_HOST_EVENT_SITE_CONTEXT_REQUEST`
- Subscription usage: `SiteRunStarted` clears craft caches/context; `SiteDevicePlaced` / `SiteDeviceBroken` dirty craft caches; `InventoryCraftContextRequested` rebuilds craft context for a target tile/device; `GS1_HOST_EVENT_SITE_CONTEXT_REQUEST` is handled directly as craft-context input when not in placement mode.
- Sends: none
- Owns game state: `SiteCraftDeviceCacheRuntime`, `SiteCraftDeviceCaches`, `SiteCraftNearbyItems`, `SiteCraftPhoneCacheMeta`, `SiteCraftPhoneItems`, `SiteCraftContextMeta`, `SiteCraftContextOptions`
- Accesses game state: reads `TileLayout`, `WorkerMotion`, `Inventory`, `Craft`.
- ECS data: reads `TileLayout`, `WorkerMotion`, `Inventory`, `Craft`; owns `Craft`.
- Notable flecs usage: sparse device iteration with tile coords and device structure ids.

### `EconomyPhoneSystem`
- Functionality: owns site-session cash, phone listings, unlockable reveal/purchase flow, buy/sell/hire handling, and delivery-batch generation.
- Subscribed game messages: `SiteRunStarted`, `SiteRefreshTick`, `EconomyMoneyAwardRequested`, `PhoneListingPurchaseRequested`, `PhoneListingSaleRequested`, `ContractorHireRequested`, `SiteUnlockableRevealRequested`, `SiteUnlockablePurchaseRequested`
- Subscribed host messages: `GS1_HOST_EVENT_GAMEPLAY_ACTION`
- Subscription usage: `SiteRunStarted` seeds phone economy state; `SiteRefreshTick` refreshes generated buy-stock listings; `EconomyMoneyAwardRequested` changes site cash; purchase/sale/hire/unlockable messages resolve those economy operations; `SiteUnlockableRevealRequested` reveals unlockables; `GS1_HOST_EVENT_GAMEPLAY_ACTION` handles buy/sell/hire/unlockable actions directly.
- Sends: `InventoryDeliveryBatchRequested`, `PhoneListingPurchased`, `PhoneListingSold`, `InventoryGlobalItemConsumeRequested`
- Owns game state: `SiteEconomyMeta`, `SiteEconomyRevealedUnlockableIds`, `SiteEconomyDirectPurchaseUnlockableIds`, `SiteEconomyPhoneListings`
- Accesses game state: reads `Inventory`, `Craft`, `Action`, `TaskBoard`, `Economy`.
- ECS data: reads `Inventory`, `Craft`, `Action`, `TaskBoard`, `Economy`; owns `Economy`.
### `CampDurabilitySystem`
- Functionality: applies camp wear and records service/failure-facing durability changes.
- Subscribed game messages: `SiteRunStarted`
- Subscribed host messages: none
- Subscription usage: `SiteRunStarted` resets camp durability/service flags to their starting values.
- Sends: none
- Owns game state: `SiteCamp`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `Camp`, `Weather`, `Event`.
- ECS data: reads `RunMeta`, `Camp`, `Weather`, `Event`; owns `Camp`.

### `DeviceSupportSystem`
- Functionality: resolves support-dependent device efficiency and stored-water behavior.
- Subscribed game messages: none
- Subscribed host messages: none
- Subscription usage: none.
- Sends: none
- Owns game state: none declared by state-set list beyond site-component ownership mapping for `DeviceRuntime`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `TileLayout`, `TileWeather`, `DeviceCondition`, `DeviceRuntime`.
- ECS data: reads `TileLayout`, `TileWeather`, `DeviceCondition`, `DeviceRuntime`; owns `DeviceRuntime`.
- Notable flecs usage: sparse device iteration with `DeviceEfficiency` and `DeviceStoredWater`.

### `ModifierSystem`
- Functionality: owns active site modifiers plus resolved modifier totals/effects and applies timed expiry and item-triggered buffs.
- Subscribed game messages: `SiteRunStarted`, `RunModifierAwardRequested`, `InventoryItemUseCompleted`, `SiteModifierEndRequested`
- Subscribed host messages: `GS1_HOST_EVENT_GAMEPLAY_ACTION`
- Subscription usage: `SiteRunStarted` seeds initial modifiers; `RunModifierAwardRequested` adds a modifier; `InventoryItemUseCompleted` applies item-triggered modifier effects; `SiteModifierEndRequested` removes a modifier; `GS1_HOST_EVENT_GAMEPLAY_ACTION` handles modifier-end host actions directly.
- Sends: none
- Owns game state: `SiteModifierMeta`, `SiteModifierNearbyAuraIds`, `SiteModifierActiveSiteModifiers`
- Accesses game state: `RuntimeCampaignFactionProgressTag`, `RuntimeCampaignTechnologyTag`, `RuntimeFixedStepSecondsTag`; reads `Camp`, `Modifier`.
- ECS data: reads `Camp`, `Modifier`; owns `Modifier`.
### `SiteTimeSystem`
- Functionality: advances site clock/day phase and emits periodic refresh ticks.
- Subscribed game messages: none
- Subscribed host messages: none
- Subscription usage: none.
- Sends: `SiteRefreshTick`
- Owns game state: `SiteClock`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `Time`.
- ECS data: reads `Time`; owns `Time`.

### `SiteFlowSystem`
- Functionality: owns worker movement and site-scene motion/approach flow.
- Subscribed game messages: none
- Subscribed host messages: `GS1_HOST_EVENT_SITE_MOVE_DIRECTION`
- Subscription usage: `GS1_HOST_EVENT_SITE_MOVE_DIRECTION` captures raw movement input into the runtime move-direction snapshot consumed by `run()`.
- Sends: none
- Owns game state: none declared by state-set list beyond site-component ownership mapping for `WorkerMotion`
- Accesses game state: `RuntimeFixedStepSecondsTag`, `RuntimeMoveDirectionTag`; reads `TileLayout`, `WorkerMotion`, `Action`, `Inventory`.
- ECS data: reads `TileLayout`, `WorkerMotion`, `Action`, `Inventory`; owns `WorkerMotion`.

### `FailureRecoverySystem`
- Functionality: watches failure conditions and requests site failure transition when the worker collapses or recovery conditions fail.
- Subscribed game messages: none
- Subscribed host messages: none
- Subscription usage: none.
- Sends: `SiteAttemptEnded`
- Owns game state: none declared
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `WorkerNeeds`.
- ECS data: reads `RunMeta`, `WorkerNeeds`; owns none.

### `SiteCompletionSystem`
- Functionality: evaluates active site objectives and requests site success/failure completion transitions.
- Subscribed game messages: none
- Subscribed host messages: none
- Subscription usage: none.
- Sends: `SiteAttemptEnded`
- Owns game state: none declared by state-set list beyond site-component ownership mapping for `Counters` and `Objective`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `Time`, `Counters`, `TileLayout`, `TileEcology`, `Objective`.
- ECS data: reads `RunMeta`, `Time`, `Counters`, `TileLayout`, `TileEcology`, `Objective`; owns `Counters`, `Objective`.

## Ownership / Access Notes

- `CampaignFlowSystem` is the practical root owner for app/campaign flow and also the boot/teardown bridge for all active site split-state sets.
- `CampaignTimeSystem` mutates `CampaignCore` without declaring ownership; this is consistent with the folder guideline but not with a strict ownership reading.
- `DeviceMaintenanceSystem` mutates `TileDeviceData` in `SiteWorld` while declaring ownership only for `DeviceCondition`.
- The weather pipeline is intentionally split:
  - `WeatherEventSystem` owns global site weather/event state.
  - `PlantWeatherContributionSystem` owns plant contribution fields.
  - `DeviceWeatherContributionSystem` owns device contribution fields.
  - `LocalWeatherResolveSystem` owns final local-weather state.
- The task/economy/inventory/action chain is message-heavy by design:
  - `TaskBoardSystem` emits reward/intention messages.
  - `EconomyPhoneSystem`, `InventorySystem`, `ModifierSystem`, and campaign systems resolve only their own slices in response.
- The access layer is asymmetric:
  - `write_*()` helpers like `write_tile_ecology`, `write_tile_excavation`, `write_tile_local_weather`, and `write_worker` enforce ownership with `static_assert`.
  - Many `own_*()` helpers such as `own_inventory()`, `own_weather()`, `own_site_world()`, `own_event()`, `own_task_board()`, `own_modifier()`, `own_economy()`, `own_counters()`, and `own_objective()` return writable state without an ownership assertion.
  - Because of that API shape, some ownership violations compile cleanly instead of failing at build time.

## Ownership Violations Found In Source

### 1. `CampaignTimeSystem` writes `CampaignCore` without declaring ownership
- Declared ownership: none.
- Actual write: `CampaignCore` is mutated in-place to advance elapsed minutes and remaining days.
- Why this is a violation: `CampaignFlowSystem` is the declared owner of `CampaignCore`, but `CampaignTimeSystem` writes that same state directly.
- Evidence:
  - [src/campaign/systems/campaign_time_system.cpp](/D:/testgame/gs1_upstream/src/campaign/systems/campaign_time_system.cpp:65)
  - [src/campaign/systems/campaign_flow_system.h](/D:/testgame/gs1_upstream/src/campaign/systems/campaign_flow_system.h:14)

### 2. `CampaignFlowSystem` writes many `Site*` split states without listing them in `owned_state_sets()`
- Declared ownership: campaign/app state sets only.
- Actual writes: clears and seeds `SiteRunMeta`, `SiteClock`, `SiteCamp`, `SiteInventory*`, `SiteWeather`, `SiteEvent`, `SiteTaskBoard*`, `SiteModifier*`, `SiteEconomy*`, `SiteCraft*`, `SiteAction*`, `SiteCounters`, `SiteObjective*`, `SiteLocalWeatherResolve*`, `SitePlantWeatherContribution*`, `SiteDeviceWeatherContribution*`, and `MoveDirection`.
- Why this is a violation: the implementation behaves like a bootstrap owner for active site split-state, but that ownership is not declared in `owned_state_sets()`.
- Evidence:
  - [src/campaign/systems/campaign_flow_system.cpp](/D:/testgame/gs1_upstream/src/campaign/systems/campaign_flow_system.cpp:42)
  - [src/campaign/systems/campaign_flow_system.cpp](/D:/testgame/gs1_upstream/src/campaign/systems/campaign_flow_system.cpp:102)
  - [src/campaign/systems/campaign_flow_system.h](/D:/testgame/gs1_upstream/src/campaign/systems/campaign_flow_system.h:14)

### 3. `DeviceMaintenanceSystem` mutates `TileDeviceData` while declaring ownership only for `DeviceCondition`
- Declared ownership: `DeviceCondition`.
- Actual writes: updates `site_ecs::DeviceIntegrity` and also overwrites `SiteWorld::TileDeviceData` through `set_tile_device(...)` during placement, repair, and breakage flow.
- Why this is a violation: if tile/device world data is part of the tile/device runtime surface, then this system is writing beyond its declared ownership boundary.
- Evidence:
  - [src/site/systems/device_maintenance_system.h](/D:/testgame/gs1_upstream/src/site/systems/device_maintenance_system.h:30)
  - [src/site/systems/device_maintenance_system.cpp](/D:/testgame/gs1_upstream/src/site/systems/device_maintenance_system.cpp:140)
  - [src/site/systems/device_maintenance_system.cpp](/D:/testgame/gs1_upstream/src/site/systems/device_maintenance_system.cpp:172)
  - [src/site/systems/device_maintenance_system.cpp](/D:/testgame/gs1_upstream/src/site/systems/device_maintenance_system.cpp:273)
  - [src/site/systems/device_maintenance_system.cpp](/D:/testgame/gs1_upstream/src/site/systems/device_maintenance_system.cpp:283)

## Subscription Usage

This section explains what each system actually does with the `GameMessage` and `HostMessage` subscriptions it declares.

### `CampaignFlowSystem`
- `OpenMainMenu`: sets app state back to main menu.
- `StartNewCampaign`: creates a fresh campaign, clears site-run split state, seeds campaign state, and transitions to the regional map.
- `SelectDeploymentSite`: validates and stores the selected regional-map site, then emits `DeploymentSiteSelectionChanged`.
- `ClearDeploymentSiteSelection`: clears the selected deployment target, then emits `DeploymentSiteSelectionChanged`.
- `StartSiteAttempt`: creates the active `SiteRun`, installs the site world, seeds the active `Site*` split state, and emits `SiteRunStarted`.
- `ReturnToRegionalMap`: tears down the active site run and returns app state to the regional map.
- `SiteAttemptEnded`: resolves site completion/failure results, unlocks adjacent sites, emits campaign/faction rewards, and transitions to site-result state.
- `GS1_HOST_EVENT_GAMEPLAY_ACTION`: handles host gameplay actions like start/select/return directly, then emits canonical follow-up gameplay messages where needed.

### `CampaignTimeSystem`
- No subscribed `GameMessage`.
- No subscribed `HostMessage`.

### `FactionReputationSystem`
- `FactionReputationAwardRequested`: adds faction reputation and unlocks the assistant package when the authored threshold is crossed.
- No subscribed `HostMessage`.

### `LoadoutPlannerSystem`
- `DeploymentSiteSelectionChanged`: refreshes the planner from the canonical selected-site message, even if the original selection came indirectly from another system.
- No subscribed `HostMessage`.

### `TechnologySystem`
- `CampaignReputationAwardRequested`: adds awarded reputation into `CampaignTechnology.total_reputation`.
- `GS1_HOST_EVENT_GAMEPLAY_ACTION`: validates tech-tree claim/refund host actions directly.

### `ActionExecutionSystem`
- `StartSiteAction`: validates and starts a site action request.
- `CancelSiteAction`: cancels the current or targeted action / placement flow.
- `PlacementModeCursorMoved`: updates placement-mode target tile and validation context.
- `PlacementReservationAccepted`: records the reservation token and begins execution when approach requirements are satisfied.
- `PlacementReservationRejected`: fails the pending placement action and clears action state.
- `GS1_HOST_EVENT_SITE_ACTION_REQUEST`: translates host action input into `StartSiteAction`.
- `GS1_HOST_EVENT_SITE_ACTION_CANCEL`: translates host cancel input into `CancelSiteAction`.
- `GS1_HOST_EVENT_SITE_CONTEXT_REQUEST`: when placement mode is active, treats the hovered tile as a `PlacementModeCursorMoved` update.

### `WeatherEventSystem`
- `SiteRunStarted`: seeds baseline weather, forecast profile, and repeating-wave timers for the active site objective.
- No subscribed `HostMessage`.

### `WorkerConditionSystem`
- `SiteRunStarted`: emits the initial worker-meter snapshot for the new run.
- `WorkerMeterDeltaRequested`: applies requested meter deltas to worker condition state.
- `InventoryItemUseCompleted`: converts completed item-use effects into worker meter deltas.
- No subscribed `HostMessage`.

### `EcologySystem`
- `SiteRunStarted`: emits startup ecology snapshots.
- `SiteGroundCoverPlaced`: applies ground-cover placement to tile ecology and marks dirty ecology state.
- `SiteTilePlantingCompleted`: applies planting across the plant footprint and marks changed ecology tiles.
- `SiteTileWatered`: applies watering to tile ecology.
- `SiteTileBurialCleared`: applies burial-clear results to tile ecology.
- `SiteTileHarvested`: applies harvest results across the harvested footprint and marks ecology changes.
- No subscribed `HostMessage`.

### `PlantWeatherContributionSystem`
- `SiteRunStarted`: forces a full plant-contribution rebuild on the next run.
- `TileEcologyChanged`: marks tiles dirty when occupancy or plant density changed.
- `TileEcologyBatchChanged`: marks multiple tiles dirty when batch ecology updates changed occupancy or plant density.
- No subscribed `HostMessage`.

### `DeviceWeatherContributionSystem`
- `SiteRunStarted`: forces a full device-contribution rebuild on the next run.
- `SiteDevicePlaced`: marks tiles affected by the placed device as dirty.
- `SiteDeviceBroken`: marks tiles affected by the broken device as dirty.
- `SiteDeviceRepaired`: marks tiles affected by the repaired device as dirty.
- `SiteDeviceConditionChanged`: marks tiles affected by integrity/condition changes as dirty.
- No subscribed `HostMessage`.

### `TaskBoardSystem`
- `SiteRunStarted`: seeds startup/onboarding task-board state.
- `SiteRefreshTick`: refreshes the normal task pool when the task-board refresh bit is present.
- `TaskAcceptRequested`: accepts a task.
- `TaskRewardClaimRequested`: claims a selected task reward.
- `RestorationProgressChanged`: updates task progress from restoration progress.
- `TileEcologyChanged`: updates ecology-driven task progress from a single-tile change.
- `TileEcologyBatchChanged`: updates ecology-driven task progress from batched tile changes.
- `LivingPlantStabilityChanged`: updates stability-related task progress.
- `SiteTileStateChanged`: updates tasks that depend on resolved tile state/weather.
- `WorkerMetersChanged`: updates worker-condition-driven task progress.
- `PhoneListingPurchased`: updates economy/phone tasks after a purchase.
- `PhoneListingSold`: updates economy/phone tasks after a sale.
- `InventoryTransferCompleted`: updates inventory-movement tasks.
- `InventoryItemSubmitted`: updates submission tasks.
- `InventoryItemUseCompleted`: updates consumable/use tasks.
- `InventoryCraftCompleted`: updates crafting tasks.
- `SiteTilePlantingCompleted`: updates planting tasks.
- `SiteActionCompleted`: updates generic action-completion tasks.
- `SiteDevicePlaced`: updates device-placement tasks.
- `SiteDeviceConditionChanged`: updates device-condition / repair tasks.
- `EconomyMoneyAwardRequested`: updates money-earning tasks.
- `GS1_HOST_EVENT_GAMEPLAY_ACTION`: handles host task accept / claim actions directly.

### `PlacementValidationSystem`
- `SiteRunStarted`: clears or initializes placement reservation state for the new run.
- `PlacementReservationRequested`: validates a requested placement reservation and emits accepted/rejected.
- `PlacementReservationReleased`: releases an existing placement reservation token.
- No subscribed `HostMessage`.

### `LocalWeatherResolveSystem`
- `SiteRunStarted`: sets a flag to emit a full local-weather snapshot on the next run.
- No subscribed `HostMessage`.

### `DeviceMaintenanceSystem`
- `SiteRunStarted`: emits initial `SiteDeviceConditionChanged` messages for existing devices so downstream systems start from synchronized device state.
- `SiteDevicePlaced`: initializes placed-device tile/device condition data and emits condition-changed notifications.
- `SiteDeviceRepaired`: restores repaired-device integrity to full and emits condition-changed notifications.
- No subscribed `HostMessage`.

### `InventorySystem`
- `SiteRunStarted`: seeds inventory from the selected campaign loadout.
- `SiteDevicePlaced`: ensures device-linked storage containers exist.
- `SiteDeviceBroken`: handles device breakage cleanup for inventory storage.
- `InventoryDeliveryRequested`: resolves a single delivery request.
- `InventoryDeliveryBatchRequested`: resolves batched deliveries.
- `InventoryWorkerPackInsertRequested`: inserts items into the worker pack.
- `InventoryItemUseRequested`: resolves item-use requests.
- `InventoryItemConsumeRequested`: resolves targeted item consumption.
- `InventoryGlobalItemConsumeRequested`: resolves broader/global item consumption.
- `InventoryTransferRequested`: resolves storage transfer requests.
- `InventoryItemSubmitRequested`: resolves item submission requests.
- `InventorySlotTapped`: resolves slot-tap interactions.
- `InventoryCraftCommitRequested`: consumes inputs and commits crafting output.
- `GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP`: translates a host inventory slot tap directly into `InventorySlotTapped`.

### `CraftSystem`
- `SiteRunStarted`: clears craft caches and context state for the new run.
- `SiteDevicePlaced`: marks device craft caches dirty.
- `SiteDeviceBroken`: marks device craft caches dirty.
- `InventoryCraftContextRequested`: rebuilds or returns the craft context for the requested tile/device.
- `GS1_HOST_EVENT_SITE_CONTEXT_REQUEST`: when not in placement mode, translates a host site-context request into `InventoryCraftContextRequested` behavior.

### `EconomyPhoneSystem`
- `SiteRunStarted`: seeds phone economy state for the site.
- `SiteRefreshTick`: refreshes generated buy-stock phone listings when the phone refresh bit is set.
- `EconomyMoneyAwardRequested`: applies a site cash delta.
- `PhoneListingPurchaseRequested`: resolves buying an item or purchasing an unlockable listing.
- `PhoneListingSaleRequested`: resolves selling through a listing or dynamic sell fallback.
- `ContractorHireRequested`: resolves contractor hire flow.
- `SiteUnlockablePurchaseRequested`: purchases a specific site unlockable.
- `SiteUnlockableRevealRequested`: reveals an unlockable in the phone economy state.
- `GS1_HOST_EVENT_GAMEPLAY_ACTION`: handles host buy/sell/hire/purchase-unlockable actions directly.

### `CampDurabilitySystem`
- `SiteRunStarted`: resets camp durability and availability flags to a clean starting state for the run.
- No subscribed `HostMessage`.

### `DeviceSupportSystem`
- No subscribed `GameMessage`.
- No subscribed `HostMessage`.

### `ModifierSystem`
- `SiteRunStarted`: seeds run modifiers and resolves the initial modifier totals.
- `RunModifierAwardRequested`: adds a new run modifier and recomputes resolved modifier totals.
- `InventoryItemUseCompleted`: applies modifier effects triggered by consumed/used items.
- `SiteModifierEndRequested`: removes a modifier and recomputes totals.
- `GS1_HOST_EVENT_GAMEPLAY_ACTION`: handles host modifier-end actions directly.

### `SiteTimeSystem`
- No subscribed `GameMessage`.
- No subscribed `HostMessage`.

### `SiteFlowSystem`
- No subscribed `GameMessage`.
- `GS1_HOST_EVENT_SITE_MOVE_DIRECTION`: captures host movement input into the runtime move-direction snapshot consumed during `run()`.

### `FailureRecoverySystem`
- No subscribed `GameMessage`.
- No subscribed `HostMessage`.

### `SiteCompletionSystem`
- No subscribed `GameMessage`.
- No subscribed `HostMessage`.

## Semantic Fan-out Cases That Do Look Justified

These are not the same problem as host-input reroutes. They are semantic gameplay events emitted after state changes, and they already have real multi-system consumers today.

- `SiteDevicePlaced`
  - Produced by `ActionExecutionSystem`.
  - Consumed by `DeviceMaintenanceSystem`, `DeviceWeatherContributionSystem`, `InventorySystem`, `CraftSystem`, and `TaskBoardSystem`.
  - Why it looks justified: one gameplay action completion needs several independent systems to refresh their own state.

- `TileEcologyBatchChanged`
  - Produced by `EcologySystem`.
  - Consumed by `PlantWeatherContributionSystem` and `TaskBoardSystem`.
  - Why it looks justified: ecology changes fan out to weather-contribution rebuilds and task-progress tracking.

- `SiteRefreshTick`
  - Produced by `SiteTimeSystem`.
  - Consumed by `TaskBoardSystem` and `EconomyPhoneSystem`.
  - Why it looks justified: one clock tick drives multiple independent periodic refresh systems.

- `InventoryItemUseCompleted`
  - Produced by `InventorySystem`.
  - Consumed by `WorkerConditionSystem`, `ModifierSystem`, and `TaskBoardSystem`.
  - Why it looks justified: a completed item-use changes worker stats, modifier state, and task progress at the same time.

## Source Anchors

- Runtime registry: [src/runtime/game_runtime.cpp](/D:/testgame/gs1_upstream/src/runtime/game_runtime.cpp:392)
- Internal message catalog: [src/messages/game_message.h](/D:/testgame/gs1_upstream/src/messages/game_message.h:28)
- Site ownership contract: [src/site/systems/site_system_context.h](/D:/testgame/gs1_upstream/src/site/systems/site_system_context.h:24)
- Campaign system headers: [src/campaign/systems](/D:/testgame/gs1_upstream/src/campaign/systems)
- Site system headers: [src/site/systems](/D:/testgame/gs1_upstream/src/site/systems)
