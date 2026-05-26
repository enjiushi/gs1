# Gameplay DLL System Audit

This document audits the systems that are actually instantiated in the gameplay DLL runtime registry in [src/runtime/game_runtime.cpp](/D:/testgame/gs1_upstream/src/runtime/game_runtime.cpp:392).

Scope:
- Only systems registered by `GameRuntime::initialize_system_registry()` are included.
- `RegionalSupportSystem` is not included because it is only a placeholder header and is not registered in the live DLL.
- "Subscribed message" means internal `GameMessage` subscriptions.
- "Subscribed host message" means `Gs1HostMessage` subscriptions at the DLL boundary.
- "Sends" lists internal `GameMessage` traffic emitted by the system implementation.
- Some subscribed `GameMessage` entries do not currently have a normal gameplay-system producer path in `src/`; `OpenMainMenu` is queued by `GameRuntime` during first boot, while several campaign-flow messages are only exercised by tests or manual queue injection and are bypassed in the live adapter path by direct host-message handling.
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
- Subscribed game messages:
  - `OpenMainMenu`
    - Description: internal campaign-flow request to leave the active campaign surface and return to the main menu.
    - Usage: consumes the runtime boot message queued by `GameRuntime` on first boot, and resets app flow back to main menu.
    - Producer note: no gameplay-system sender found in `src/`; the only live producer is runtime bootstrap in `GameRuntime`.
  - `StartNewCampaign`
    - Description: internal request to create and install a fresh campaign snapshot.
    - Usage: seeds the campaign through lifecycle helpers, clears the active site-run snapshot, and transitions to the regional map.
    - Producer note: no gameplay-system sender found in `src/`; the live adapter path handles start-new-campaign through `GS1_HOST_EVENT_GAMEPLAY_ACTION` instead of queuing this `GameMessage`.
  - `SelectDeploymentSite`
    - Description: internal request to pick the active deployment target on the regional map.
    - Usage: validates and stores the selected site, then emits `DeploymentSiteSelectionChanged`.
    - Producer note: no gameplay-system sender found in `src/`; the live adapter path handles deployment-site selection through `GS1_HOST_EVENT_GAMEPLAY_ACTION` instead of queuing this `GameMessage`.
  - `ClearDeploymentSiteSelection`
    - Description: internal request to clear the currently selected deployment target.
    - Usage: clears the selection and emits `DeploymentSiteSelectionChanged`.
    - Producer note: no gameplay-system sender found in `src/`; the live adapter path handles clear-selection through `GS1_HOST_EVENT_GAMEPLAY_ACTION` instead of queuing this `GameMessage`.
  - `StartSiteAttempt`
    - Description: internal request to create and activate a site run from the current campaign selection.
    - Usage: installs the new `SiteRun` through lifecycle helpers and emits `SiteRunStarted`.
    - Producer note: no gameplay-system sender found in `src/`; the live adapter path handles start-site-attempt through `GS1_HOST_EVENT_GAMEPLAY_ACTION` instead of queuing this `GameMessage`.
  - `ReturnToRegionalMap`
    - Description: internal request to leave the active site run and return to the campaign map.
    - Usage: tears the site run down through lifecycle helpers and switches app flow back to the regional map.
    - Producer note: no gameplay-system sender found in `src/`; the live adapter path handles return-to-map through `GS1_HOST_EVENT_GAMEPLAY_ACTION` instead of queuing this `GameMessage`.
  - `SiteAttemptEnded`
    - Description: internal completion/failure message for the active site attempt.
    - Usage: resolves result handling, unlocks adjacent sites, and transitions to site-result state.
    - Design note: completed sites remain non-reenterable, and the broader intended direction is to eventually replace the "attempt" framing with a simpler enter/leave site-session model that does not use failure as a core player-facing concept.
- Subscribed host messages:
  - `GS1_HOST_EVENT_GAMEPLAY_ACTION`
    - Description: DLL-boundary gameplay action input covering campaign/menu/site-flow UI actions.
    - Usage: handles host start/select/return actions directly before emitting the canonical internal follow-up messages.
- Sent game messages:
  - `SiteSceneActivated`
    - Description: internal notification that the site scene should be treated as activated for the new run.
    - Usage: emitted during site-attempt startup after the run is installed.
  - `DeploymentSiteSelectionChanged`
    - Description: internal notification that the selected deployment site changed.
    - Usage: emitted after select/clear deployment-site operations so dependent systems can rebuild selection-driven state.
  - `SiteRunStarted`
    - Description: internal notification that the active `SiteRun` is installed and ready for startup initialization.
    - Usage: emitted after a site attempt begins so site systems seed their run-local state.
  - `CampaignReputationAwardRequested`
    - Description: internal request to award total campaign reputation.
    - Usage: no longer emitted from site-attempt result resolution; task rewards remain the active producer path in the current codebase.
  - `FactionReputationAwardRequested`
    - Description: internal request to award faction-specific reputation.
    - Usage: no longer emitted from site-attempt result resolution; task rewards remain the active producer path in the current codebase.
- Owns game state: `AppState`, `CampaignCore`, `CampaignRegionalMapMeta`, `CampaignRegionalMapRevealedSites`, `CampaignRegionalMapAvailableSites`, `CampaignRegionalMapCompletedSites`, `CampaignSiteMetaEntries`, `CampaignSiteAdjacentIds`, `CampaignSiteExportedSupportItems`, `CampaignSiteNearbyAuraModifierIds`
- Accesses game state: `RuntimeAppStateTag`, `RuntimeCampaignFactionProgressTag`; installs and resets campaign/site lifecycle state through narrow runtime lifecycle helpers and owns `SiteRunMeta` for active site flow bookkeeping.
- ECS data: none directly; it bootstraps and swaps the active `SiteWorld` handle.
- Ownership note: site-run bootstrap/reset work now goes through runtime lifecycle helpers instead of direct broad `Site*` split-state writes.
### `CampaignTimeSystem`
- Functionality: advances campaign elapsed minutes and remaining days on fixed step.
- Subscribed game messages: none.
- Subscribed host messages: none.
- Sent game messages:
  - `CampaignTimeDeltaRequested`
    - Description: internal request for the `CampaignCore` owner to apply a fixed-step campaign-time delta.
    - Usage: emitted on fixed-step runs instead of mutating `CampaignCore` directly.
- Owns game state: none declared
- Accesses game state: `RuntimeFixedStepSecondsTag`; does not mutate `CampaignCore` directly and instead emits `CampaignTimeDeltaRequested` for the declared owner to apply.
- ECS data: none
- Ownership note: `CampaignFlowSystem` remains the `CampaignCore` owner and applies the time delta requested by this system.

### `FactionReputationSystem`
- Functionality: applies faction-reputation awards and updates per-faction progress.
- Subscribed game messages:
  - `FactionReputationAwardRequested`
    - Description: internal request to add reputation to a specific campaign faction track.
    - Usage: applies the reputation gain and assistant-package unlock checks.
- Subscribed host messages: none.
- Sent game messages: none.
- Owns game state: `CampaignFactionProgress`
- Accesses game state: `RuntimeCampaignFactionProgressTag`
- ECS data: none

### `LoadoutPlannerSystem`
- Functionality: rebuilds the selected deployment loadout and site-exported support package from current campaign selection.
- Subscribed game messages:
  - `DeploymentSiteSelectionChanged`
    - Description: internal notification that the selected deployment target changed.
    - Usage: rebuilds planner state from the canonical validated selection result.
- Subscribed host messages: none.
- Sent game messages: none.
- Owns game state: `CampaignLoadoutPlannerMeta`, `CampaignLoadoutPlannerBaselineItems`, `CampaignLoadoutPlannerAvailableSupportItems`, `CampaignLoadoutPlannerSelectedSlots`, `CampaignLoadoutPlannerNearbyAuraModifiers`
- Accesses game state: reads campaign site metadata/adjacency through split-state assembly to rebuild the currently selected site package.
- ECS data: none

### `TechnologySystem`
- Functionality: tracks total-reputation unlock progression and faction-tech unlock state; also handles tech claim/refund intents from host gameplay actions.
- Subscribed game messages:
  - `CampaignReputationAwardRequested`
    - Description: internal request to add total campaign reputation.
    - Usage: adds awarded reputation into `CampaignTechnology.total_reputation` and re-evaluates total-reputation unlock progress.
- Subscribed host messages:
  - `GS1_HOST_EVENT_GAMEPLAY_ACTION`
    - Description: DLL-boundary gameplay action input that includes tech-tree claim and refund commands.
    - Usage: validates tech claim/refund actions directly against authored tech and campaign state.
- Sent game messages: none.
- Owns game state: `CampaignTechnology`
- Accesses game state: `RuntimeCampaignTechnologyTag`; reads campaign existence and interacts with faction progress and authored tech definitions for unlock checks.
- ECS data: none

## Site Systems

### `ActionExecutionSystem`
- Functionality: starts, ticks, completes, and cancels site actions including planting, watering, harvest, excavation, crafting, repair, and placement-mode commit flow.
- Subscribed game messages:
  - `StartSiteAction`
    - Description: internal request to begin a site action.
    - Usage: validates and starts the requested action.
  - `CancelSiteAction`
    - Description: internal request to cancel the active or targeted site action.
    - Usage: cancels action execution and clears placement/action state as needed.
  - `PlacementModeCursorMoved`
    - Description: internal placement-mode cursor update carrying the current hovered tile.
    - Usage: updates placement targeting and validation context.
  - `PlacementReservationAccepted`
    - Description: internal acceptance result for a requested placement reservation.
    - Usage: records the reservation state and begins execution when approach requirements are satisfied.
  - `PlacementReservationRejected`
    - Description: internal rejection result for a requested placement reservation.
    - Usage: fails the pending placement flow and clears action state.
- Subscribed host messages:
  - `GS1_HOST_EVENT_SITE_ACTION_REQUEST`
    - Description: DLL-boundary host input for starting a site action.
    - Usage: translated directly into canonical action-start handling.
  - `GS1_HOST_EVENT_SITE_ACTION_CANCEL`
    - Description: DLL-boundary host input for canceling a site action.
    - Usage: translated directly into canonical action-cancel handling.
  - `GS1_HOST_EVENT_SITE_CONTEXT_REQUEST`
    - Description: DLL-boundary site-context request that includes the currently hovered tile.
    - Usage: when placement mode is active, feeds placement cursor updates.
- Sent game messages:
  - `SiteActionStarted`
    - Description: internal notification that a site action successfully began.
    - Usage: emitted after action validation succeeds and execution starts.
  - `SiteActionCompleted`
    - Description: internal notification that a site action finished successfully.
    - Usage: emitted when execution resolves a completed action result.
  - `WorkerMeterDeltaRequested`
    - Description: internal request to apply worker meter changes from action costs or outcomes.
    - Usage: emitted when actions should drain or restore worker condition meters.
  - `SiteActionFailed`
    - Description: internal notification that a site action could not be completed.
    - Usage: emitted on validation or execution failure paths.
  - `PlacementReservationRequested`
    - Description: internal request to reserve a placement target before commit.
    - Usage: emitted when placement-mode execution needs validation and reservation ownership.
  - `PlacementReservationReleased`
    - Description: internal request to release an existing placement reservation.
    - Usage: emitted when placement state is canceled or completed.
  - `PlacementModeCommitRejected`
    - Description: internal notification that a placement-mode commit was rejected.
    - Usage: emitted after reservation or validation failure prevents placement completion.
  - `InventoryItemConsumeRequested`
    - Description: internal request to consume inventory inputs required by an action.
    - Usage: emitted when action execution needs targeted inventory consumption.
  - `SiteTilePlantingCompleted`
    - Description: internal semantic result that planting finished on a tile footprint.
    - Usage: emitted after a planting action completes so ecology/task systems can react.
  - `SiteGroundCoverPlaced`
    - Description: internal semantic result that ground cover was placed.
    - Usage: emitted after the corresponding placement action completes.
  - `SiteTileWatered`
    - Description: internal semantic result that watering was applied to a tile.
    - Usage: emitted after watering completes so downstream systems can update ecology/task state.
  - `SiteTileBurialCleared`
    - Description: internal semantic result that burial/excavation clearing progressed or completed.
    - Usage: emitted after the relevant action resolves on a tile.
  - `SiteDevicePlaced`
    - Description: internal semantic result that a device was placed into the site world.
    - Usage: emitted after placement completes so maintenance/weather/inventory/craft/task systems can refresh.
  - `InventoryCraftCommitRequested`
    - Description: internal request to commit crafting inventory inputs and outputs.
    - Usage: emitted when a craft action completes and inventory should apply the transaction.
  - `SiteDeviceRepaired`
    - Description: internal semantic result that a placed device was repaired.
    - Usage: emitted after a repair action completes so maintenance/weather systems can refresh device state.
  - `InventoryWorkerPackInsertRequested`
    - Description: internal request to insert generated items into the worker pack.
    - Usage: emitted when an action yields portable outputs that should go straight to the worker inventory.
  - `SiteTileHarvested`
    - Description: internal semantic result that a harvest completed on a tile footprint.
    - Usage: emitted after harvest resolution so ecology/task systems can process the result.
- Owns game state: `SiteActionMeta`, `SiteActionReservedInputItemStacks`, `SiteActionResolvedHarvestOutputs`
- Accesses game state: `RuntimeCampaignFactionProgressTag`, `RuntimeFixedStepSecondsTag`, `RuntimeMoveDirectionTag`; reads `Time`, `TileLayout`, `TileExcavation`, `TileWeather`, `DeviceCondition`, `WorkerMotion`, `WorkerNeeds`, `Inventory`, `Modifier`, `Craft`, `Action`.
- ECS data: reads `Time`, `TileLayout`, `TileExcavation`, `TileWeather`, `DeviceCondition`, `WorkerMotion`, `WorkerNeeds`, `Inventory`, `Modifier`, `Craft`, `Action`; owns `TileExcavation`, `Action`.

### `WeatherEventSystem`
- Functionality: advances authored weather-event timelines and writes current site weather/event state.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active and ready for system startup.
    - Usage: seeds baseline site weather, forecast profile, and repeating-wave timers for the active objective.
- Subscribed host messages: none.
- Sent game messages: none, aside from debug `PresentLog` probes in startup/weather diagnostics.
- Owns game state: `SiteWeather`, `SiteEvent`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `Objective`, `Weather`, `Event`.
- ECS data: reads `RunMeta`, `Objective`, `Weather`, `Event`; owns `Weather`, `Event`.

### `WorkerConditionSystem`
- Functionality: resolves worker meter changes, passive losses/recovery, and derived energy-cap/work-efficiency snapshots.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: seeds and publishes the initial worker-meter snapshot for the run.
  - `WorkerMeterDeltaRequested`
    - Description: internal request to apply worker meter deltas.
    - Usage: mutates worker-condition state from gameplay meter changes.
  - `InventoryItemUseCompleted`
    - Description: internal notification that an inventory item-use action completed.
    - Usage: converts item-use effects into worker-condition changes.
- Subscribed host messages: none.
- Sent game messages:
  - `WorkerMetersChanged`
    - Description: internal notification that authoritative worker meters changed.
    - Usage: emitted after startup seeding or later condition updates so dependent systems can refresh progress/state.
- Owns game state: none declared by state-set list beyond site-component ownership mapping for `WorkerNeeds`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `TileWeather`, `WorkerMotion`, `WorkerNeeds`, `Action`.
- ECS data: reads `RunMeta`, `TileWeather`, `WorkerMotion`, `WorkerNeeds`, `Action`; owns `WorkerNeeds`.

### `EcologySystem`
- Functionality: updates plant density, watering, burial, support-driven growth pressure, restoration progress, and living-plant stability.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that the site run is active.
    - Usage: emits startup ecology snapshots after initializing the run-local ecology view.
  - `SiteGroundCoverPlaced`
    - Description: internal semantic result that ground cover was placed.
    - Usage: applies the ground-cover mutation to tile ecology.
  - `SiteTilePlantingCompleted`
    - Description: internal semantic result that planting completed on a tile footprint.
    - Usage: applies planting across the plant footprint and marks changed ecology tiles.
  - `SiteTileWatered`
    - Description: internal semantic result that watering completed on a tile.
    - Usage: applies watering to tile ecology.
  - `SiteTileBurialCleared`
    - Description: internal semantic result that burial/excavation clearing completed on a tile.
    - Usage: applies burial-clear results to tile ecology.
  - `SiteTileHarvested`
    - Description: internal semantic result that harvesting completed on a tile footprint.
    - Usage: applies harvest results across the harvested footprint and marks ecology changes.
- Subscribed host messages: none.
- Sent game messages:
  - `TileEcologyBatchChanged`
    - Description: internal notification that a batch of tile ecology values changed.
    - Usage: emitted after ecology mutations so contribution/task systems can refresh affected tiles.
  - `RestorationProgressChanged`
    - Description: internal notification that aggregate site restoration progress changed.
    - Usage: emitted when ecology changes alter restoration progress.
  - `LivingPlantStabilityChanged`
    - Description: internal notification that living-plant stability changed.
    - Usage: emitted when ecology updates affect aggregate plant stability.
  - `PresentLog`
    - Description: debug-only internal log message.
    - Usage: emitted only for diagnostic ecology logging.
- Owns game state: `SiteCounters`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `TileLayout`, `TileEcology`, `TileWeather`, `TilePlantWeatherContribution`, `TileDeviceWeatherContribution`, `Modifier`, `Counters`, `Objective`.
- ECS data: reads `TileLayout`, `TileEcology`, `TileWeather`, `TilePlantWeatherContribution`, `TileDeviceWeatherContribution`, `Modifier`, `Counters`, `Objective`; owns `TileEcology`, `Counters`.
- Notable flecs usage: sparse occupied-tile and ecology-related tile entities in the site world.

### `PlantWeatherContributionSystem`
- Functionality: maintains plant-owned per-tile weather contribution fields and dirty-tile rebuild queues.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: forces a full plant-contribution rebuild on the next run step.
  - `TileEcologyChanged`
    - Description: internal notification that a single tile's ecology state changed.
    - Usage: marks the affected tile dirty when occupancy or plant density changed.
  - `TileEcologyBatchChanged`
    - Description: internal notification that a batch of tile ecology states changed.
    - Usage: marks multiple affected tiles dirty when occupancy or density changed in bulk.
- Subscribed host messages: none.
- Sent game messages: none.
- Owns game state: `SitePlantWeatherContributionMeta`, `SitePlantWeatherContributionDirtyTileIndices`, `SitePlantWeatherContributionDirtyTileMask`
- Accesses game state: reads `TileLayout`, `TileEcology`, `TilePlantWeatherContribution`, `Weather`, `PlantWeatherRuntime`.
- ECS data: reads `TileLayout`, `TileEcology`, `TilePlantWeatherContribution`, `Weather`, `PlantWeatherRuntime`; owns `TilePlantWeatherContribution`, `PlantWeatherRuntime`.
- Notable flecs usage: writes `site_ecs::TilePlantWeatherContribution`.

### `DeviceWeatherContributionSystem`
- Functionality: maintains device-owned per-tile weather contribution fields and dirty-tile rebuild queues.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: forces a full device-contribution rebuild on the next run step.
  - `SiteDevicePlaced`
    - Description: internal semantic result that a device was placed into the site world.
    - Usage: marks tiles affected by the placed device as dirty.
  - `SiteDeviceBroken`
    - Description: internal notification that a device became broken.
    - Usage: marks tiles affected by the broken device as dirty.
  - `SiteDeviceRepaired`
    - Description: internal semantic result that a device was repaired.
    - Usage: marks tiles affected by the repaired device as dirty.
  - `SiteDeviceConditionChanged`
    - Description: internal notification that device integrity/condition changed.
    - Usage: marks tiles affected by the condition change as dirty.
- Subscribed host messages: none.
- Sent game messages: none.
- Owns game state: `SiteDeviceWeatherContributionMeta`, `SiteDeviceWeatherContributionDirtyTileIndices`, `SiteDeviceWeatherContributionDirtyTileMask`
- Accesses game state: reads `TileLayout`, `TileDeviceWeatherContribution`, `DeviceCondition`, `DeviceRuntime`, `Weather`, `DeviceWeatherRuntime`.
- ECS data: reads `TileLayout`, `TileDeviceWeatherContribution`, `DeviceCondition`, `DeviceRuntime`, `Weather`, `DeviceWeatherRuntime`; owns `TileDeviceWeatherContribution`, `DeviceWeatherRuntime`.
- Notable flecs usage: writes `site_ecs::TileDeviceWeatherContribution`.

### `TaskBoardSystem`
- Functionality: seeds onboarding tasks, refreshes board tasks, tracks task progress from cross-system messages, and resolves task reward claims.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: seeds startup and onboarding task-board state.
  - `SiteRefreshTick`
    - Description: internal periodic site refresh tick.
    - Usage: refreshes the normal task pool when the task-board refresh bit is present.
  - `TaskAcceptRequested`
    - Description: internal request to accept a visible task.
    - Usage: resolves task acceptance.
  - `TaskRewardClaimRequested`
    - Description: internal request to claim a completed task reward.
    - Usage: resolves reward-claim handling.
  - `RestorationProgressChanged`
    - Description: internal notification that restoration progress changed.
    - Usage: updates restoration-driven task progress.
  - `TileEcologyChanged`
    - Description: internal notification that a single tile's ecology changed.
    - Usage: updates ecology-driven task progress from a single-tile change.
  - `TileEcologyBatchChanged`
    - Description: internal notification that a batch of tile ecology changed.
    - Usage: updates ecology-driven task progress from batched tile changes.
  - `LivingPlantStabilityChanged`
    - Description: internal notification that aggregate living-plant stability changed.
    - Usage: updates stability-related task progress.
  - `SiteTileStateChanged`
    - Description: internal notification that resolved tile weather/state changed.
    - Usage: updates tasks that depend on tile state or local weather.
  - `WorkerMetersChanged`
    - Description: internal notification that worker meters changed.
    - Usage: updates worker-condition-driven task progress.
  - `PhoneListingPurchased`
    - Description: internal notification that a phone listing purchase completed.
    - Usage: updates economy and phone-related task progress.
  - `PhoneListingSold`
    - Description: internal notification that a phone listing sale completed.
    - Usage: updates economy and sale-related task progress.
  - `InventoryTransferCompleted`
    - Description: internal notification that an inventory transfer completed.
    - Usage: updates inventory-movement task progress.
  - `InventoryItemSubmitted`
    - Description: internal notification that an inventory submission completed.
    - Usage: updates submission-related task progress.
  - `InventoryItemUseCompleted`
    - Description: internal notification that an item-use action completed.
    - Usage: updates consumable/use task progress.
  - `InventoryCraftCompleted`
    - Description: internal notification that a craft inventory commit completed.
    - Usage: updates crafting task progress.
  - `SiteTilePlantingCompleted`
    - Description: internal semantic result that planting completed on a tile footprint.
    - Usage: updates planting task progress.
  - `SiteActionCompleted`
    - Description: internal notification that a site action completed successfully.
    - Usage: updates generic action-completion task progress.
  - `SiteDevicePlaced`
    - Description: internal semantic result that a device was placed.
    - Usage: updates device-placement task progress.
  - `SiteDeviceConditionChanged`
    - Description: internal notification that device condition changed.
    - Usage: updates device-condition and repair task progress.
  - `EconomyMoneyAwardRequested`
    - Description: internal request to apply a site cash delta.
    - Usage: updates money-earning task progress when cash awards are requested.
- Subscribed host messages:
  - `GS1_HOST_EVENT_GAMEPLAY_ACTION`
    - Description: DLL-boundary gameplay action input that includes task accept and reward-claim commands.
    - Usage: handles task accept and claim host actions directly.
- Sent game messages:
  - `EconomyMoneyAwardRequested`
    - Description: internal request to award site cash.
    - Usage: emitted when a claimed task reward should grant money.
  - `InventoryDeliveryRequested`
    - Description: internal request to deliver item rewards into inventory.
    - Usage: emitted when a claimed task reward grants delivered items.
  - `SiteUnlockableRevealRequested`
    - Description: internal request to reveal a site unlockable in the economy/phone surface.
    - Usage: emitted when a claimed task reward should reveal unlockable content.
  - `RunModifierAwardRequested`
    - Description: internal request to award a run modifier.
    - Usage: emitted when a claimed task reward should add a modifier to the active site run.
  - `CampaignReputationAwardRequested`
    - Description: internal request to award total campaign reputation.
    - Usage: emitted when a claimed task reward grants campaign reputation.
  - `FactionReputationAwardRequested`
    - Description: internal request to award faction-specific reputation.
    - Usage: emitted when a claimed task reward grants faction reputation.
- Owns game state: `SiteTaskBoardMeta`, `SiteTaskBoardVisibleTasks`, `SiteTaskBoardRewardDraftOptions`, `SiteTaskBoardTrackedTiles`, `SiteTaskBoardAcceptedTaskIds`, `SiteTaskBoardCompletedTaskIds`, `SiteTaskBoardClaimedTaskIds`
- Accesses game state: `RuntimeCampaignFactionProgressTag`, `RuntimeCampaignTechnologyTag`, `RuntimeFixedStepSecondsTag`; reads `TaskBoard`, `TileLayout`, `Counters`, `Objective`.
- ECS data: reads `TaskBoard`, `TileLayout`, `Counters`, `Objective`; owns `TaskBoard`.
### `PlacementValidationSystem`
- Functionality: validates placement reservations/releases and answers whether a placement can be committed.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: clears or initializes reservation state for the run.
  - `PlacementReservationRequested`
    - Description: internal request to validate and reserve a placement target.
    - Usage: validates the reservation and decides whether it can be committed.
  - `PlacementReservationReleased`
    - Description: internal request to release a previously held placement reservation.
    - Usage: frees the reservation token/state.
- Subscribed host messages: none.
- Sent game messages:
  - `PlacementReservationAccepted`
    - Description: internal acceptance result for a placement reservation request.
    - Usage: emitted when the requested placement can reserve its target.
  - `PlacementReservationRejected`
    - Description: internal rejection result for a placement reservation request.
    - Usage: emitted when the requested placement cannot reserve its target.
- Owns game state: none declared
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `TileLayout`, `TileEcology`.
- ECS data: reads `RunMeta`, `TileLayout`, `TileEcology`; owns none.

### `LocalWeatherResolveSystem`
- Functionality: combines base weather plus plant/device contribution channels into final tile local-weather state and dirty-tile updates.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: sets a flag to emit a full local-weather snapshot on the next run.
- Subscribed host messages: none.
- Sent game messages:
  - `SiteTileStateChanged`
    - Description: internal notification that resolved tile local-weather/state changed.
    - Usage: emitted after local-weather recomputation changes authoritative tile state.
  - `PresentLog`
    - Description: debug-only internal log message.
    - Usage: emitted only for diagnostic local-weather logging.
- Owns game state: `SiteLocalWeatherResolveMeta`, `SiteLocalWeatherResolveLastTotalContributions`
- Accesses game state: reads `TileLayout`, `TileEcology`, `TileWeather`, `TilePlantWeatherContribution`, `TileDeviceWeatherContribution`, `Weather`, `Modifier`, `LocalWeatherRuntime`.
- ECS data: reads `TileLayout`, `TileEcology`, `TileWeather`, `TilePlantWeatherContribution`, `TileDeviceWeatherContribution`, `Weather`, `Modifier`, `LocalWeatherRuntime`; owns `TileWeather`, `LocalWeatherRuntime`.

### `DeviceMaintenanceSystem`
- Functionality: applies weather-driven device wear, marks broken devices, and reflects repair/integrity changes.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: emits initial device-condition messages for already present devices so downstream state starts synchronized.
  - `SiteDevicePlaced`
    - Description: internal semantic result that a device was placed.
    - Usage: initializes placed-device condition/runtime state and emits condition changes.
  - `SiteDeviceRepaired`
    - Description: internal semantic result that a device was repaired.
    - Usage: restores repaired-device integrity and re-emits condition changes.
- Subscribed host messages: none.
- Sent game messages:
  - `SiteDeviceBroken`
    - Description: internal notification that a device crossed into a broken state.
    - Usage: emitted when wear resolution breaks a device.
  - `SiteDeviceConditionChanged`
    - Description: internal notification that device integrity/condition changed.
    - Usage: emitted after startup seeding, placement, repair, or wear changes update device condition.
- Owns game state: none declared by state-set list beyond site-component ownership mapping for `DeviceCondition`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `TileLayout`, `TileEcology`, `DeviceCondition`, `Weather`.
- ECS data: reads `TileLayout`, `TileEcology`, `DeviceCondition`, `Weather`; owns `DeviceCondition`.
- Notable flecs usage: sparse device iteration over `TileCoordComponent`, `DeviceStructureId`, `DeviceIntegrity`, `DeviceFixedIntegrity`, `DeviceTag`.
- Ownership note: device tile/runtime updates now go through owner-scoped `SiteWorldAccess` helpers instead of raw tile-device writes.

### `InventorySystem`
- Functionality: owns storage containers, worker pack, deliveries, use/consume/submit/transfer/craft-commit resolution, and site-start loadout insertion.
- Subscribed game messages:
  - `StartSiteAction`
    - Description: internal request to begin a site action.
    - Usage: prepares action-linked inventory state before execution proceeds.
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: seeds inventory from the selected campaign loadout.
  - `SiteDevicePlaced`
    - Description: internal semantic result that a device was placed.
    - Usage: ensures any device-linked storage containers exist.
  - `SiteDeviceBroken`
    - Description: internal notification that a device became broken.
    - Usage: applies device-storage cleanup or state changes tied to breakage.
  - `InventoryDeliveryRequested`
    - Description: internal request for a single delivery into inventory.
    - Usage: resolves a single delivery request.
  - `InventoryDeliveryBatchRequested`
    - Description: internal request for a batch of deliveries into inventory.
    - Usage: resolves batched deliveries.
  - `InventoryWorkerPackInsertRequested`
    - Description: internal request to insert items into the worker pack.
    - Usage: resolves worker-pack insertion.
  - `InventoryItemUseRequested`
    - Description: internal request to use an inventory item.
    - Usage: resolves item-use handling.
  - `InventoryItemConsumeRequested`
    - Description: internal request to consume a targeted inventory item stack.
    - Usage: resolves targeted inventory consumption.
  - `InventoryGlobalItemConsumeRequested`
    - Description: internal request to consume inventory from broader/global sources.
    - Usage: resolves non-targeted or global item consumption.
  - `InventoryTransferRequested`
    - Description: internal request to transfer items between inventory containers.
    - Usage: resolves storage transfer handling.
  - `InventoryItemSubmitRequested`
    - Description: internal request to submit an inventory item for task/objective use.
    - Usage: resolves item submission handling.
  - `InventorySlotTapped`
    - Description: internal notification/request that an inventory slot interaction occurred.
    - Usage: resolves slot-tap interactions.
  - `InventoryCraftCommitRequested`
    - Description: internal request to commit crafting inputs and outputs.
    - Usage: consumes craft inputs and writes craft outputs to inventory.
- Subscribed host messages:
  - `GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP`
    - Description: DLL-boundary inventory slot tap input from the host/UI.
    - Usage: handled directly as the canonical slot-tap command.
- Sent game messages:
  - `InventoryItemUseCompleted`
    - Description: internal notification that an inventory item-use action completed.
    - Usage: emitted after successful use resolution so worker/modifier/task systems can react.
  - `StartSiteAction`
    - Description: internal request to begin a site action.
    - Usage: emitted when inventory-side interaction should trigger a gameplay action flow.
  - `InventoryCraftCompleted`
    - Description: internal notification that a craft inventory commit completed.
    - Usage: emitted after craft commit resolution updates authoritative inventory state.
  - `InventoryTransferCompleted`
    - Description: internal notification that an inventory transfer completed.
    - Usage: emitted after transfer resolution for task/progress consumers.
  - `InventoryItemSubmitted`
    - Description: internal notification that an inventory submission completed.
    - Usage: emitted after submission resolution for task/progress consumers.
- Owns game state: `SiteInventoryMeta`, `SiteInventoryStorageContainers`, `SiteInventoryStorageSlotItemIds`, `SiteInventoryWorkerPackSlots`, `SiteInventoryPendingDeliveries`, `SiteInventoryPendingDeliveryItemStacks`
- Accesses game state: `RuntimeCampaignFactionProgressTag`, `RuntimeCampaignTechnologyTag`; reads `RunMeta`, `TileLayout`, `WorkerMotion`, `Inventory`, `Action`.
- ECS data: reads `RunMeta`, `TileLayout`, `WorkerMotion`, `Inventory`, `Action`; owns `Inventory`.

### `CraftSystem`
- Functionality: maintains craft-device caches, nearby-item caches, phone inventory cache, and craft-context option projection.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: clears craft caches and context state for the run.
  - `SiteDevicePlaced`
    - Description: internal semantic result that a device was placed.
    - Usage: marks craft-device caches dirty.
  - `SiteDeviceBroken`
    - Description: internal notification that a device became broken.
    - Usage: marks craft-device caches dirty.
  - `InventoryCraftContextRequested`
    - Description: internal request to rebuild or expose craft context for a tile/device.
    - Usage: rebuilds the craft context for the requested tile/device.
- Subscribed host messages:
  - `GS1_HOST_EVENT_SITE_CONTEXT_REQUEST`
    - Description: DLL-boundary host context request for a site tile/device interaction.
    - Usage: when not in placement mode, handled directly as craft-context input.
- Sent game messages: none.
- Owns game state: `SiteCraftDeviceCacheRuntime`, `SiteCraftDeviceCaches`, `SiteCraftNearbyItems`, `SiteCraftPhoneCacheMeta`, `SiteCraftPhoneItems`, `SiteCraftContextMeta`, `SiteCraftContextOptions`
- Accesses game state: reads `TileLayout`, `WorkerMotion`, `Inventory`, `Craft`.
- ECS data: reads `TileLayout`, `WorkerMotion`, `Inventory`, `Craft`; owns `Craft`.
- Notable flecs usage: sparse device iteration with tile coords and device structure ids.

### `EconomyPhoneSystem`
- Functionality: owns site-session cash, phone listings, unlockable reveal/purchase flow, buy/sell/hire handling, and delivery-batch generation.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: seeds phone economy state for the run.
  - `SiteRefreshTick`
    - Description: internal periodic site refresh tick.
    - Usage: refreshes generated buy-stock phone listings when the phone refresh bit is set.
  - `EconomyMoneyAwardRequested`
    - Description: internal request to apply a site cash delta.
    - Usage: adjusts authoritative site cash.
  - `PhoneListingPurchaseRequested`
    - Description: internal request to purchase a phone listing.
    - Usage: resolves item or unlockable purchase flow.
  - `PhoneListingSaleRequested`
    - Description: internal request to sell through a phone listing or fallback sale path.
    - Usage: resolves sale handling.
  - `ContractorHireRequested`
    - Description: internal request to hire a contractor through the phone economy surface.
    - Usage: resolves contractor hire flow.
  - `SiteUnlockableRevealRequested`
    - Description: internal request to reveal an unlockable in phone economy state.
    - Usage: reveals the unlockable in authoritative economy data.
  - `SiteUnlockablePurchaseRequested`
    - Description: internal request to purchase a specific site unlockable.
    - Usage: resolves unlockable purchase handling.
- Subscribed host messages:
  - `GS1_HOST_EVENT_GAMEPLAY_ACTION`
    - Description: DLL-boundary gameplay action input that includes buy, sell, hire, and unlockable commands.
    - Usage: handles host phone-economy actions directly.
- Sent game messages:
  - `InventoryDeliveryBatchRequested`
    - Description: internal request to deliver purchased or generated item batches into inventory.
    - Usage: emitted after purchases or economy actions produce item deliveries.
  - `PhoneListingPurchased`
    - Description: internal notification that a phone listing purchase completed.
    - Usage: emitted after purchase resolution so tasks and other listeners can react.
  - `PhoneListingSold`
    - Description: internal notification that a phone listing sale completed.
    - Usage: emitted after sale resolution so tasks and other listeners can react.
  - `InventoryGlobalItemConsumeRequested`
    - Description: internal request to consume inventory items as part of an economy transaction.
    - Usage: emitted when an economy operation spends item resources from inventory.
- Owns game state: `SiteEconomyMeta`, `SiteEconomyRevealedUnlockableIds`, `SiteEconomyDirectPurchaseUnlockableIds`, `SiteEconomyPhoneListings`
- Accesses game state: reads `Inventory`, `Craft`, `Action`, `TaskBoard`, `Economy`.
- ECS data: reads `Inventory`, `Craft`, `Action`, `TaskBoard`, `Economy`; owns `Economy`.
### `CampDurabilitySystem`
- Functionality: applies camp wear and records service/failure-facing durability changes.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: resets camp durability and service flags to their starting values.
- Subscribed host messages: none.
- Sent game messages: none.
- Owns game state: `SiteCamp`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `Camp`, `Weather`, `Event`.
- ECS data: reads `RunMeta`, `Camp`, `Weather`, `Event`; owns `Camp`.

### `DeviceSupportSystem`
- Functionality: resolves support-dependent device efficiency and stored-water behavior.
- Subscribed game messages: none.
- Subscribed host messages: none.
- Sent game messages: none.
- Owns game state: none declared by state-set list beyond site-component ownership mapping for `DeviceRuntime`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `TileLayout`, `TileWeather`, `DeviceCondition`, `DeviceRuntime`.
- ECS data: reads `TileLayout`, `TileWeather`, `DeviceCondition`, `DeviceRuntime`; owns `DeviceRuntime`.
- Notable flecs usage: sparse device iteration with `DeviceEfficiency` and `DeviceStoredWater`.

### `ModifierSystem`
- Functionality: owns active site modifiers plus resolved modifier totals/effects and applies timed expiry and item-triggered buffs.
- Subscribed game messages:
  - `SiteRunStarted`
    - Description: internal notification that a new site run is active.
    - Usage: seeds initial modifiers and resolves initial modifier totals.
  - `RunModifierAwardRequested`
    - Description: internal request to add a modifier to the active site run.
    - Usage: adds the modifier and recomputes resolved totals.
  - `InventoryItemUseCompleted`
    - Description: internal notification that an inventory item-use action completed.
    - Usage: applies item-triggered modifier effects.
  - `SiteModifierEndRequested`
    - Description: internal request to end an active site modifier.
    - Usage: removes the modifier and recomputes totals.
- Subscribed host messages:
  - `GS1_HOST_EVENT_GAMEPLAY_ACTION`
    - Description: DLL-boundary gameplay action input that includes modifier-end commands.
    - Usage: handles modifier-end host actions directly.
- Sent game messages: none.
- Owns game state: `SiteModifierMeta`, `SiteModifierNearbyAuraIds`, `SiteModifierActiveSiteModifiers`
- Accesses game state: `RuntimeCampaignFactionProgressTag`, `RuntimeCampaignTechnologyTag`, `RuntimeFixedStepSecondsTag`; reads `Camp`, `Modifier`.
- ECS data: reads `Camp`, `Modifier`; owns `Modifier`.
### `SiteTimeSystem`
- Functionality: advances site clock/day phase and emits periodic refresh ticks.
- Subscribed game messages: none.
- Subscribed host messages: none.
- Sent game messages:
  - `SiteRefreshTick`
    - Description: internal periodic refresh tick for systems that update on site time cadence.
    - Usage: emitted from site-clock advancement to drive task-board and phone-economy refresh work.
- Owns game state: `SiteClock`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `Time`.
- ECS data: reads `Time`; owns `Time`.

### `SiteFlowSystem`
- Functionality: owns worker movement and site-scene motion/approach flow.
- Subscribed game messages: none.
- Subscribed host messages:
  - `GS1_HOST_EVENT_SITE_MOVE_DIRECTION`
    - Description: DLL-boundary host input carrying raw site movement direction.
    - Usage: captures host movement input into the runtime move-direction snapshot consumed by `run()`.
- Sent game messages: none.
- Owns game state: none declared by state-set list beyond site-component ownership mapping for `WorkerMotion`
- Accesses game state: `RuntimeFixedStepSecondsTag`, `RuntimeMoveDirectionTag`; reads `TileLayout`, `WorkerMotion`, `Action`, `Inventory`.
- ECS data: reads `TileLayout`, `WorkerMotion`, `Action`, `Inventory`; owns `WorkerMotion`.

### `FailureRecoverySystem`
- Functionality: watches failure conditions and requests site failure transition when the worker collapses or recovery conditions fail.
- Subscribed game messages: none.
- Subscribed host messages: none.
- Sent game messages:
  - `SiteAttemptEnded`
    - Description: internal notification that the active site attempt should resolve as ended.
    - Usage: emitted when failure conditions are met and site failure transition should begin.
- Owns game state: none declared
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `WorkerNeeds`.
- ECS data: reads `RunMeta`, `WorkerNeeds`; owns none.

### `SiteCompletionSystem`
- Functionality: evaluates active site objectives and requests site success/failure completion transitions.
- Subscribed game messages: none.
- Subscribed host messages: none.
- Sent game messages:
  - `SiteAttemptEnded`
    - Description: internal notification that the active site attempt should resolve as ended.
    - Usage: emitted when objective completion evaluation determines the run should end.
- Owns game state: none declared by state-set list beyond site-component ownership mapping for `Counters` and `Objective`
- Accesses game state: `RuntimeFixedStepSecondsTag`; reads `RunMeta`, `Time`, `Counters`, `TileLayout`, `TileEcology`, `Objective`.
- ECS data: reads `RunMeta`, `Time`, `Counters`, `TileLayout`, `TileEcology`, `Objective`; owns `Counters`, `Objective`.

## Ownership / Access Notes

- `CampaignFlowSystem` is the practical root owner for app/campaign flow and also the boot/teardown bridge for all active site split-state sets.
- `CampaignTimeSystem` no longer mutates `CampaignCore` directly; it emits `CampaignTimeDeltaRequested` so the declared owner applies the mutation.
- `DeviceMaintenanceSystem` now updates device tile/runtime state through owner-scoped `SiteWorldAccess` helpers rather than raw tile-device writes.
- Current design tension to resolve: the runtime still models site entry as a `StartSiteAttempt` / `SiteAttemptEnded` loop with success/failure/result handling, while the intended direction is closer to "enter site / leave site", with completed sites remaining non-reenterable and without "attempt" as the core player-facing framing.
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

The previously documented source violations have been fixed.

### Runtime enforcement now in place
- `StateManager::state<...>()` rejects cross-owner writes while a system is executing, unless the runtime is in a narrow privileged lifecycle-install scope.
- `GameStateAccess::read()` now uses const split-state queries, so non-owners do not silently get mutable references through tagged access.
- Tagged owner mappings now explicitly cover `RuntimeAppStateTag`, `RuntimeCampaignFactionProgressTag`, `RuntimeCampaignTechnologyTag`, and `RuntimeMoveDirectionTag`.
- `CampaignTimeSystem` now emits `CampaignTimeDeltaRequested`, and `CampaignFlowSystem` as the `CampaignCore` owner applies the actual time mutation.
- `CampaignFlowSystem` no longer seeds or clears arbitrary `Site*` state sets directly during site lifecycle transitions; it uses narrow runtime lifecycle install/reset helpers on `RuntimeInvocation`.
- `DeviceMaintenanceSystem` now mutates device state through owner-scoped `SiteWorldAccess` helpers instead of bypassing ownership with raw tile-device writes.

### Remaining ownership notes
- `CampaignFlowSystem` explicitly owns `SiteRunMeta`, because site-run lifecycle/result bookkeeping lives with campaign/site flow.
- `SiteFlowSystem` explicitly owns `MoveDirection`, so host movement snapshots have one declared writer.

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
