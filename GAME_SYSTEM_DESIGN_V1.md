# Game System Design V1

Sources:

- [GDD.md](GDD.md)
- [GAME_STRUCTURE.md](GAME_STRUCTURE.md)
- [CONTENT_AUTHORING_CONTRACT.md](CONTENT_AUTHORING_CONTRACT.md)

Purpose: define the first implementation-ready game system design for the playable prototype.

This document is intentionally code-facing. It is not another high-level architecture note. Its job is to define the concrete code structure, runtime ownership, data layout, execution flow, and implementation boundaries strongly enough that coding can begin directly after this pass.

## 1. Implementation Policy

### 1.1 V1 Architecture Decision

For the first game version:

- use one monolithic game codebase
- do not extract reusable feature modules
- do not extract a separate reusable core-schema package
- keep all gameplay structs, commands, systems, and content definitions inside one game namespace
- still preserve clean ownership and layering inside that single game codebase

This means the generic layering described in [GAME_STRUCTURE.md](GAME_STRUCTURE.md) is kept only as a conceptual discipline:

- engine integration remains separate from gameplay logic
- gameplay still owns simulation authority
- cross-ownership changes still use commands
- but all of it now lives inside one game project instead of split reusable modules

DLL boundary rule for v1:

- the gameplay code built here is the game DLL
- engine adapters are host-side code and are not compiled into this DLL
- the DLL exposes only engine-agnostic public headers and exported entry points
- build output must package the DLL together with its public headers and import library for host-side integration
- host-to-game DLL ingress should use input snapshots, host events, translated feedback events, and phase entrypoints
- game-to-host DLL egress should use engine commands only

### 1.2 Non-Goals For V1

These are intentionally out of scope for the first coding stage:

- reusable cross-game modules
- multiplayer support
- disk save/load
- broad off-screen simulation
- procedural regional-map generation
- a generalized plugin architecture

### 1.3 Hard Decisions Locked Now

To avoid implementation drift, the following choices are now fixed:

- use one `GameRuntime` with one in-memory `CampaignState`
- use one fixed-step simulation loop for site gameplay
- use dense arrays for tile simulation instead of one ECS entity per tile
- use entities only for dynamic actors and action executors that benefit from entity identity
- use explicit command queues for cross-owner mutation
- use authored prototype regional-map adjacency as a plain adjacency list per site
- use deterministic random seeds per campaign and per site attempt
- do not implement disk save/load in v1; all persistence is in-process only

## 2. Top-Level Code Structure

This is the required source layout for v1. Folder names may adapt to the target language, but the ownership split should stay equivalent.

```text
include/
  gs1/
    export.h
    status.h
    types.h
    game_api.h
src/
  app/
    GameApp
    GameBootstrap
    GameLoop
    AppStateMachine
    SceneCoordinator
    CampaignFactory
    SiteRunFactory
  runtime/
    GameRuntime
    RuntimeClock
    FixedStepRunner
    InputSnapshot
    CommandQueue
    EngineCommandQueue
    EngineFeedbackBuffer
    RandomService
    IdAllocator
  content/
    ContentDatabase
    ContentIndex
    ContentLoader
    ContentValidator
    defs/
      CampaignContentDefs
      SiteContentDefs
      PlantDefs
      StructureDefs
      ItemDefs
      RecipeDefs
      TechnologyDefs
      TaskDefs
      RewardDefs
      EventDefs
      ModifierDefs
  campaign/
    CampaignState
    RegionalMapState
    SiteMetaState
    FactionProgressState
    TechnologyState
    LoadoutPlannerState
    systems/
      CampaignFlowSystem
      RegionalSupportSystem
      TechnologySystem
      LoadoutPlannerSystem
  site/
    SiteRunState
    SiteClockState
    TileGridState
    TileLayers
    WorkerState
    CampState
    InventoryState
    ContractorState
    WeatherState
    EventState
    EcologyState
    DeviceState
    TaskBoardState
    ModifierState
    EconomyState
    ActionState
    systems/
      SiteFlowSystem
      ActionExecutionSystem
      DeviceSupportSystem
      PlacementValidationSystem
      WeatherEventSystem
      LocalWeatherResolveSystem
      WorkerConditionSystem
      CampDurabilitySystem
      DeviceMaintenanceSystem
      EcologySystem
      InventorySystem
      EconomyPhoneSystem
      TaskBoardSystem
      ModifierSystem
      SiteCompletionSystem
      FailureRecoverySystem
  commands/
    GameCommand
    CommandIds
    handlers/
  events/
    EngineFeedbackEvent
    RuntimeEvent
  ui/
    UiState
    HudViewModel
    PhoneViewModel
    RegionalMapViewModel
    InspectPanelViewModel
    NotificationViewModel
    UiPresenter
  support/
    math/
    logging/
    assertions/
    devtools/
tests/
  unit/
  integration/
  simulation/
tools/
  content_build/
  content_validate/
```

## 3. Layering Rules Inside The Monolith

Even though v1 is monolithic, code is still layered.

### 3.1 `app/`

Owns:

- process startup
- dependency wiring
- top-level app state transitions
- scene switching between menu, regional map, and active site

Must not own:

- gameplay rule calculations
- tile simulation
- task generation logic

### 3.2 `runtime/`

Owns:

- fixed-step execution
- queues
- input snapshots
- runtime clocks
- id allocation
- deterministic rng access

Must not own:

- site gameplay rules
- campaign progression rules

### 3.3 `content/`

Owns:

- loading authored content
- validating authored content
- indexing content ids to runtime-usable definitions

Must not own:

- live mutable state
- gameplay progression

### 3.4 `campaign/`

Owns:

- regional map state
- completed-site support output
- faction reputation
- technology progression
- pre-deployment loadout assembly

### 3.5 `site/`

Owns:

- one active site attempt and all mutable in-site state
- tile simulation
- worker condition
- inventory and camp storage
- weather and events
- task board
- money flow during the site

### 3.6 `ui/`

Owns:

- view models
- widget state
- display prioritization

Must not own:

- authoritative gameplay state
- gameplay mutation except through app commands

### 3.7 Host-Side Engine Integration

This does not live inside the gameplay DLL source tree.

The engine-side host owns:

- engine-specific input translation into the DLL input contract
- engine command playback from the DLL engine-command contract
- engine feedback translation into the DLL feedback/input contract
- view-model binding or presentation mapping when the host chooses to use the DLL's query surface

The gameplay DLL must not contain engine-native adapter code.

## 4. Runtime State Machines

### 4.1 Application State Machine

```text
Boot
  -> MainMenu
  -> CampaignSetup
  -> RegionalMap
  -> SiteLoading
  -> SiteActive
  -> SitePaused
  -> SiteResult
  -> RegionalMap
  -> CampaignEnd
```

Rules:

- `RegionalMap` is a paused full-screen campaign UI
- `SiteActive` is the only state that runs fixed-step gameplay simulation
- `SitePaused` freezes both fixed-step simulation and board refresh timers
- `SiteResult` resolves success or failure and decides whether to return to `RegionalMap` or `CampaignEnd`

### 4.2 Campaign Flow State Machine

```text
CampaignNotStarted
  -> CampaignRunning
  -> CampaignEnded
```

While `CampaignRunning`, one target site may be:

- selected for deployment
- active in an attempt
- returned from as completed or failed

### 4.3 Site Attempt State Machine

```text
NotLoaded
  -> Loading
  -> Active
  -> Completed
  -> Failed
  -> Unloading
```

Rules:

- `Completed` means `fullyGrownTileCount >= siteCompletionTileThreshold`
- `Failed` means `playerHealth <= 0`
- completing or failing a site ends the active attempt
- retrying a failed site creates a fresh new attempt from authored site state plus current nearby support

### 4.4 Startup And Site-Build Flow

`CampaignFactory` build sequence:

1. load validated content pack
2. create `campaignSeed`
3. build authored prototype regional-map state
4. initialize faction progress and technology state
5. mark initial site availability
6. enter `RegionalMap`

`SiteRunFactory` build sequence:

1. read selected `SiteMetaState`
2. derive `siteAttemptSeed`
3. create empty `SiteRunState`
4. build tile grid from authored site archetype
5. initialize worker, camp, inventory, weather, event, modifier, economy, and counters
6. apply baseline deployment support
7. apply adjacent completed-site support packages and nearby auras
8. build direct-purchase unlockable eligibility view
9. initialize onboarding or normal task board
10. enter `SiteActive`

## 5. Determinism And Randomness

### 5.1 Seeds

Use these runtime seeds:

- `campaignSeed`
- `siteAttemptSeed`

Rules:

- `campaignSeed` is created when a new campaign starts
- `siteAttemptSeed` is derived from `campaignSeed + siteId + attemptIndex`
- task generation, event generation, reward draft generation, and site-local random rolls must use the active site attempt rng stream
- authored prototype sites still use deterministic runtime seeds for generated board content and events

### 5.2 RNG Ownership

`RandomService` owns named rng streams:

- `taskRng`
- `rewardRng`
- `eventRng`
- `minorVariationRng`

Never use engine global randomness inside gameplay code.

## 6. Canonical Runtime Data Model

### 6.1 Identifier Types

Use explicit strong ids or equivalent wrappers:

- `CampaignId`
- `SiteId`
- `FactionId`
- `TechNodeId`
- `ItemId`
- `RecipeId`
- `PlantId`
- `StructureId`
- `TaskTemplateId`
- `TaskInstanceId`
- `RewardCandidateId`
- `EventTemplateId`
- `ModifierId`
- `TileCoord`

Runtime id rule:

- authored content uses stable string ids
- live runtime instances use integer or handle ids allocated by `IdAllocator`

### 6.2 `CampaignState`

`CampaignState` is the single authoritative in-memory campaign object.

Required fields:

```text
campaignId
campaignSeed
campaignClockMinutesElapsed
campaignDaysRemaining
currentAppState
regionalMapState
siteMetaById
factionProgressById
technologyState
availableSupportCache
currentSelectedSiteId?
activeSiteRunId?
```

Rules:

- `campaignClockMinutesElapsed` advances only while a site attempt is active
- `campaignDaysRemaining` is derived from authored campaign duration minus elapsed minutes
- `siteMetaById` stores only campaign-level state, not live site-run state

### 6.3 `RegionalMapState`

Required fields:

```text
revealedSiteIds[]
availableSiteIds[]
completedSiteIds[]
selectedSiteId?
supportPreviewByTargetSiteId
```

Rules:

- `revealedSiteIds[]` is the authoritative visibility set
- `availableSiteIds[]` is derived and cached for UI convenience
- `supportPreviewByTargetSiteId` is rebuilt whenever the regional map opens or the selected site changes

### 6.4 `SiteMetaState`

This is the regional-map record for one site.

Required fields:

```text
siteId
siteState              // Locked | Available | Completed
adjacentSiteIds[]
siteArchetypeId
supportPackageId?
siteOutputModifierId?
completedAtCampaignMinute?
attemptCount
```

Rules:

- use explicit adjacency lists in v1 instead of deriving neighbors from a shared map shape
- `supportPackageId` points to authored completed-site support output
- `attemptCount` increments whenever a new attempt starts

### 6.5 `FactionProgressState`

Required fields:

```text
factionId
factionReputation
unlockedAssistantPackage
onboardingCompleted
tutorialCompleted
```

### 6.6 `TechnologyState`

Required fields:

```text
reputation
availableTechPicks
unlockedNodeIds[]
appliedContentUpdateNodeIds[]
```

Rules:

- `reputation` is never spent
- `availableTechPicks` is spent when claiming nodes
- `unlockedNodeIds[]` is authoritative progression state

### 6.7 `LoadoutPlannerState`

Required fields:

```text
selectedTargetSiteId?
baselineDeploymentItems[]
availableExportedSupportItems[]
selectedLoadoutSlots[]
activeNearbyAuraModifierIds[]
supportQuota
```

Rules:

- `baselineDeploymentItems[]` are always available and are not consumed from completed-site support
- `availableExportedSupportItems[]` are consumed when the player confirms deployment
- `selectedLoadoutSlots[]` must respect worker-pack slot limits

### 6.8 `SiteRunState`

This is the authoritative live state for one active site attempt.

Required fields:

```text
siteRunId
siteId
attemptIndex
siteAttemptSeed
siteRunStatus                 // Active | Completed | Failed
siteClock
tileGrid
workerState
campState
inventoryState
contractorState
weatherState
eventState
taskBoardState
modifierState
economyState
siteActionState
siteCounters
pendingEnginePresentationDirtyFlags
```

### 6.9 `SiteClockState`

Required fields:

```text
worldTimeMinutes
dayIndex
dayPhase
ecologyPulseAccumulator
taskRefreshAccumulator
deliveryAccumulator
```

Rules:

- fixed simulation step is `0.25` real seconds
- each simulation step advances `0.2` in-game minutes
- `ecologyPulseAccumulator` triggers every `10` in-game minutes
- `taskRefreshAccumulator` uses the authored or tuned board refresh interval
- `deliveryAccumulator` is used for the `30` minute delivery arrival rule

### 6.10 `TileGridState`

Use a dense row-major grid.

Required fields:

```text
width
height
terrainTypeId[width * height]
tileTraversable[...]
tilePlantable[...]
tileReservedByStructure[...]

tileSoilFertility[...]
tileMoisture[...]
tileSoilSalinity[...]
tileSandBurial[...]

tileHeat[...]
tileWind[...]
tileDust[...]

plantTypeId[...]
groundCoverTypeId[...]
tilePlantDensity[...]
growthPressure[...]

structureTypeId[...]
deviceIntegrity[...]
deviceEfficiency[...]
deviceStoredWater[...]
```

Rules:

- array index is `y * width + x`
- occupancy fields are authoritative and are not duplicated elsewhere
- `groundCoverTypeId` and `plantTypeId` may coexist only where the GDD sharing rules allow it
- tiles are not runtime entities in v1

### 6.11 `WorkerState`

Required fields:

```text
tileCoord
facingDirection
playerHealth
playerHydration
playerNourishment
playerEnergyCap
playerEnergy
playerMorale
playerWorkEfficiency
isSheltered
currentActionId?
```

### 6.12 `CampState`

Required fields:

```text
campAnchorTile
campDurability
campProtectionResolved
deliveryPointOperational
sharedCampStorageAccessEnabled
```

Rules:

- camp durability is the authoritative base-safety meter
- local camp recovery and storage safety calculations read from camp durability plus local protection

### 6.13 `InventoryState`

Required fields:

```text
workerPackSlotCount
campStorageSlotCount
workerPackSlots[]
campStorageSlots[]
pendingDeliveryQueue[]
```

Slot entry:

```text
itemId?
itemQuantity
itemCondition
itemFreshness
```

Delivery entry:

```text
deliveryId
itemStacks[]
minutesUntilArrival
```

Rules:

- one slot holds one stack of one exact item
- empty slot means `itemId = null`
- `itemQuantity` is removed when it reaches `0`
- selling requires items to be in `campStorageSlots[]`

### 6.14 `ContractorState`

Required fields:

```text
availableWorkUnits
assignedWorkOrders[]
```

Work order:

```text
workOrderId
taskKind
targetTile?
remainingWorkUnits
```

Rules:

- contractor work is a pooled resource, not a persistent npc roster
- unused purchased work remains available until consumed or the site attempt ends

### 6.15 `ActionState`

Required fields:

```text
currentActionId?
actionKind?
targetTile?
remainingActionMinutes
reservedInputItemStacks[]
startedAtWorldMinute?
```

Rules:

- only one player action may be active at once in v1
- reserved input items are not permanently consumed until successful action completion unless the action definition explicitly spends on start
- interruption clears the active action and releases unspent reserved inputs

### 6.16 `WeatherState`

Required fields:

```text
weatherHeat
weatherWind
weatherDust
forecastProfileState
siteWeatherBias
```

### 6.17 `EventState`

Required fields:

```text
activeEventTemplateId?
eventPhase                // None | Warning | Build | Peak | Decay | Aftermath
phaseMinutesRemaining
eventHeatPressure
eventWindPressure
eventDustPressure
aftermathReliefResolved
```

### 6.18 `TaskBoardState`

Required fields:

```text
visibleTasks[]
acceptedTaskIds[]
completedTaskIds[]
activeChainState?
taskPoolSize
acceptedTaskCap
minutesUntilNextRefresh
```

Task instance:

```text
taskInstanceId
taskTemplateId
publisherFactionId
taskTierId
targetAmount
currentProgressAmount
rewardDraftOptions[]
runtimeListKind          // Visible | Accepted | Completed
chainId?
chainStepIndex?
followUpTaskTemplateId?
```

Rules:

- do not store a separate failed task state in v1
- runtime list ownership is the authoritative task status
- reward draft options are instantiated when the task enters the visible pool

`activeChainState` fields:

```text
taskChainId
currentAcceptedStepIndex?
surfacedFollowUpTaskInstanceId?
isBroken
```

### 6.19 `ModifierState`

Required fields:

```text
activeRunModifierIds[]
activeNearbyAuraModifierIds[]
resolvedChannelTotals
```

`resolvedChannelTotals` stores the summed effective percentages for:

- heat
- wind
- dust
- moisture
- fertility
- salinity
- growth pressure
- salinity density cap
- plant density
- health
- hydration
- nourishment
- energy cap
- energy
- morale
- work efficiency

### 6.20 `EconomyState`

Required fields:

```text
money
revealedSiteUnlockableIds[]
directPurchaseUnlockableIds[]
availablePhoneListings
```

`availablePhoneListings` contains:

- buyable item listings
- sellable item summaries from camp storage
- contractor hiring offerings
- revealed unlockable purchase offerings

### 6.21 `SiteCounters`

Required fields:

```text
fullyGrownTileCount
siteCompletionTileThreshold
```

## 7. Entity Usage In V1

V1 uses a hybrid model:

- tiles and occupancy are dense arrays in `TileGridState`
- the player worker may be represented as one entity or as `WorkerState` only
- for v1, use `WorkerState` only unless the target engine already requires actor-entity identity

Recommendation:

- keep only one gameplay actor entity for the player if the engine-side host benefits from stable actor binding
- do not create entities for every plant or structure in v1
- presentation-layer spawned objects may still exist engine-side, but gameplay authority stays in the site arrays

This is simpler, faster to code, and fits the tile-heavy simulation better than forcing full ECS everywhere.

## 8. Command Model For V1

All commands now live in one game namespace:

- `game.commands`

There is no reusable command package for v1.

### 8.1 Command Families

Required command families:

- app commands
- site action commands
- inventory commands
- economy commands
- task board commands
- technology commands
- campaign flow commands
- engine presentation commands

Command handling rule:

- each command id must map to exactly one owning handler
- handlers may enqueue follow-up commands but must not call peer handlers directly
- commands are plain data objects and must not carry engine references or function callbacks

### 8.2 Required App Commands

- `StartNewCampaign`
- `OpenRegionalMap`
- `SelectDeploymentSite`
- `StartSiteAttempt`
- `ExitSiteToResult`
- `ReturnToRegionalMap`

### 8.3 Required Site Action Commands

- `BeginPlantAction`
- `BeginBuildAction`
- `BeginRepairAction`
- `BeginWaterAction`
- `BeginBurialClearAction`
- `CancelCurrentAction`
- `CompleteAction`

### 8.4 Required Inventory Commands

- `TransferItemWorkerToCamp`
- `TransferItemCampToWorker`
- `ConsumeItem`
- `UseMedicine`
- `DrinkWater`
- `EatFood`
- `TransferWaterToDevice`

### 8.5 Required Economy Commands

- `BuyItemPackage`
- `SellCampStoredItem`
- `HireContractorWork`
- `PurchaseSiteUnlockable`
- `PurchaseDirectFallbackUnlockable`
- `ClaimDeliveryAtCamp`

### 8.6 Required Task Board Commands

- `AcceptTask`
- `RefreshTaskBoard`
- `CompleteTask`
- `ClaimTaskReward`
- `SpawnChainFollowUp`

### 8.7 Required Technology Commands

- `ClaimTechnologyNode`

### 8.8 Required Campaign Commands

- `ApplyCompletedSiteSupport`
- `MarkSiteCompleted`
- `MarkSiteFailed`
- `RevealAdjacentSites`

### 8.9 Required Engine Presentation Command Families

- app-state presentation commands
- regional-map snapshot commands
- regional-map delta commands
- site snapshot commands
- site delta commands
- HUD projection commands
- phone and task-board projection commands
- notification and one-shot cue commands
- site-result presentation commands

Rules:

- presentation commands contain only world meaning
- gameplay systems never call adapter functions directly
- if the public DLL query surface stays minimal, engine commands must be sufficient for the host to reconstruct long-lived presentation state without reaching back into gameplay internals
- long-lived presentation state should use snapshot begin/end plus upsert/remove commands instead of relying on opaque dirty-only notifications
- transient presentation such as sound/VFX hints should use dedicated one-shot cue commands rather than be mixed into persistent projection state

## 9. System Ownership And Update Order

This is the required authoritative order during one fixed simulation step while a site is active.

### 9.1 Step Order

1. `SiteFlowSystem`
2. `ActionExecutionSystem`
3. command flush phase A
4. `ModifierSystem`
5. `WeatherEventSystem`
6. `DeviceSupportSystem`
7. `LocalWeatherResolveSystem`
8. `WorkerConditionSystem`
9. `CampDurabilitySystem`
10. `DeviceMaintenanceSystem`
11. `EcologySystem`
12. `InventorySystem`
13. `TaskBoardSystem`
14. `SiteCompletionSystem`
15. `FailureRecoverySystem`
16. command flush phase B
17. engine presentation flush

Phase rule:

- command flush phase A exists so completed actions, timed deliveries, and other same-step state transitions can affect local weather, hazard handling, and recovery during that same fixed step
- command flush phase B handles late-step follow-up commands such as completion transitions, reward payouts, and presentation updates

### 9.2 Ownership Table

| System | Owns Direct Writes | Reads | Emits Commands |
|---|---|---|---|
| `SiteFlowSystem` | `SiteClockState`, site timers | all site state | task refresh, delivery arrival, completion/failure transitions |
| `ActionExecutionSystem` | `siteActionState`, completed action results | worker, inventory, tile grid | inventory consume, plant/build/repair completion |
| `DeviceSupportSystem` | device support/output cache for current step | structure arrays, camp, modifiers | none |
| `WeatherEventSystem` | `WeatherState`, `EventState` | site clock, content defs, modifier state | aftermath relief resolution, presentation updates |
| `LocalWeatherResolveSystem` | `tileHeat`, `tileWind`, `tileDust` arrays | weather, event, camp, devices, plants, modifiers | none |
| `WorkerConditionSystem` | `WorkerState` meters | local weather, modifiers, consumed items, current action | site failure, presentation updates |
| `CampDurabilitySystem` | `CampState.campDurability` | weather, local protection, event phase | presentation updates |
| `DeviceMaintenanceSystem` | structure-related arrays | inventory, camp, weather, event, modifiers | device breakdown, water consumption, output updates |
| `EcologySystem` | soil arrays, plant arrays, `fullyGrownTileCount` | local weather, devices, modifiers | site completion candidate, presentation updates |
| `InventorySystem` | inventory slots, deliveries | actions, economy, devices, camp | none |
| `TaskBoardSystem` | task lists, progress, refresh timer | site clock, site counters, item totals, action completions | reward claim, faction reputation gain, chain continuation |
| `ModifierSystem` | `resolvedChannelTotals` | nearby auras, run modifiers | none |
| `SiteCompletionSystem` | site run status | site counters | `MarkSiteCompleted` |
| `FailureRecoverySystem` | site run status | worker health | `MarkSiteFailed` |

Rule:

- if a system does not own a state block, it reads that block and emits a command instead of mutating it directly

## 10. Detailed System Responsibilities

### 10.1 `SiteFlowSystem`

Responsibilities:

- advance `worldTimeMinutes`
- compute `dayIndex` and `dayPhase`
- tick task refresh timer
- tick ecology pulse timer
- tick delivery timers
- create refresh commands when the board timer expires

### 10.2 `ActionExecutionSystem`

Responsibilities:

- hold one active manual action for the worker
- check action completion against elapsed action time
- on completion, emit domain commands that mutate inventory, tiles, or devices
- apply interruption rules during severe hazards or cancellation

V1 rule:

- only one player manual action can be active at a time
- action-completion commands must resolve in command flush phase A so a completed watering, repair, burial clear, or build can affect the same simulation step before hazard damage is applied

### 10.3 `WeatherEventSystem`

Responsibilities:

- advance event phases
- set `eventHeatPressure`, `eventWindPressure`, `eventDustPressure`
- resolve event start from eligible templates
- resolve `Aftermath` and potential relief offers

### 10.4 `LocalWeatherResolveSystem`

Responsibilities:

- recompute `tileHeat`, `tileWind`, `tileDust` every step
- combine site weather, event pressure, nearby support, local plant support, structure support, terrain shelter, and modifier channels

### 10.5 `WorkerConditionSystem`

Responsibilities:

- apply hydration, nourishment, energy, morale, and health changes
- apply recovery when sheltered
- clamp all worker meters
- emit site-failure command when health reaches zero

### 10.6 `CampDurabilitySystem`

Responsibilities:

- apply ongoing camp durability loss from harsh exposure
- weaken camp-side protection when durability drops
- expose camp inventory risk to severe hazard logic

### 10.7 `DeviceSupportSystem`

Responsibilities:

- resolve which placed devices are operational for the current step
- resolve current-step support and output contributions before local weather is rebuilt
- expose read-only device support values for `LocalWeatherResolveSystem`

### 10.8 `DeviceMaintenanceSystem`

Responsibilities:

- update `deviceIntegrity`
- update `deviceEfficiency`
- update `deviceStoredWater`
- process irrigation water consumption
- process solar surplus summaries
- apply ongoing hazard-side device wear and burial-related degradation after local weather and exposure are already known

### 10.9 `EcologySystem`

Responsibilities:

- apply soil moisture/fertility/salinity changes
- apply burial gain and clearing effects
- compute `growthPressure`
- apply `tilePlantDensity` gain/loss
- resolve density-state thresholds
- update `fullyGrownTileCount`
- run ecology pulse actions such as spread

### 10.10 `InventorySystem`

Responsibilities:

- move items between worker pack and camp storage
- process delivery arrival
- handle stack merge/split
- consume carried items for actions
- apply camp-stored item damage or spoilage during severe hazards

### 10.11 `EconomyPhoneSystem`

Responsibilities:

- build current phone listings from content plus run state
- buy items
- sell items
- hire contractor work
- purchase revealed or direct-fallback unlockables

### 10.12 `TaskBoardSystem`

Responsibilities:

- build eligible visible task pool
- instantiate task runtime entries from templates
- track progress
- move tasks between visible, accepted, and completed lists
- generate reward drafts at task instantiation time
- award guaranteed faction reputation on task completion
- handle chain continuation

### 10.13 `ModifierSystem`

Responsibilities:

- aggregate nearby aura modifiers and run modifiers into resolved modifier channels
- clamp stacked modifier totals
- expose read-only resolved modifier totals to all simulation systems

### 10.14 `RegionalSupportSystem`

Responsibilities:

- when entering `RegionalMap`, apply elapsed support catch-up to completed sites
- assemble available exported item counts
- assemble nearby-aura packages for the selected site

### 10.15 `TechnologySystem`

Responsibilities:

- validate node claim prerequisites
- spend `availableTechPicks`
- mark node unlocked
- update `ContentDatabase` eligibility view for recipes, plants, and devices

## 11. Content Loading And Runtime Eligibility

### 11.1 Content Build Flow

Required content pipeline:

1. raw authored files
2. content validation
3. compiled runtime content pack
4. load into `ContentDatabase`
5. build fast id indexes in `ContentIndex`

### 11.2 Runtime Content Access Rule

Gameplay code must not parse raw data files at runtime.

Gameplay code reads only:

- validated definition structs from `ContentDatabase`
- fast lookup indexes from `ContentIndex`

### 11.3 Eligibility Views

`ContentDatabase` must expose runtime eligibility queries:

- `getEligibleTaskTemplates(siteRun, campaignState)`
- `getEligibleRewardCandidates(taskTemplate, campaignState)`
- `getEligibleEventTemplates(siteRun, campaignState)`
- `getEligibleSiteUnlockables(siteRun, campaignState)`
- `getEligibleDirectPurchaseUnlockables(siteRun, campaignState)`

These are query helpers over validated content plus current progression state. They are not the generator themselves.

## 12. UI Architecture

UI code is read-only over gameplay state and mutates gameplay only through commands.

### 12.1 DLL-Defined UI Setup Contract

For v1, menu and panel UI should cross the DLL boundary as semantic UI setup data, not as engine-native widget calls.

Rules:

- the gameplay DLL defines the binary contract for UI setup batches and UI elements
- each UI element descriptor carries only semantic meaning such as element type, text, state flags, and a DLL-defined action token
- the DLL must not define engine layout details such as absolute position, size, color theme, animation asset, or engine widget class
- the engine adapter owns layout, sizing, styling, animation, focus behavior, and actual rendering
- when the player activates a host-rendered UI element, the adapter sends the matching DLL-defined UI action back through the host-event contract
- UI action events may be submitted in the pre-`phase1` host-event window or in the between-phase host-event window depending on when the host resolved the interaction

Current v1 command shape:

- `BEGIN_UI_SETUP`
- `UI_ELEMENT_UPSERT`
- `END_UI_SETUP`

Prototype note:

- until a dedicated campaign-setup surface exists, the main-menu start action may carry DLL-authored default campaign parameters inside its returned action token

The begin/end fence defines one transactional UI setup batch for one semantic surface such as:

- main menu
- regional-map selection panel
- site-result panel

### 12.2 Required UI View Models

- `HudViewModel`
- `PhoneViewModel`
- `RegionalMapViewModel`
- `InventoryViewModel`
- `TaskBoardViewModel`
- `WeatherForecastViewModel`
- `SiteInspectViewModel`
- `NotificationViewModel`

### 12.3 HUD Minimum Fields

The HUD must always show:

- `playerHealth`
- `playerHydration`
- `playerEnergy`
- current money
- current action
- current weather warning
- task summary
- site progress summary

### 12.4 Phone UI Minimum Fields

The phone must expose:

- visible tasks
- accepted tasks
- reward preview for visible tasks
- buy listings
- sellable camp inventory
- contractor hire option
- revealed unlockables

### 12.5 Regional Map UI Minimum Fields

The regional map must expose:

- all revealed sites
- current site states
- adjacency
- support contributors to the selected site
- loadout planning view
- tech claim screen

Regional-map note:

- site nodes are still projected through the dedicated regional-map engine-command contract, not through generic UI setup elements
- host-side interaction with projected site nodes should still come back into the DLL as DLL-defined UI action tokens such as `SELECT_DEPLOYMENT_SITE`

## 13. Engine Adapter Contract For V1

This contract is for the engine-side host, not the gameplay DLL.

The host-side engine integration layer must provide:

- `queueHostEventsBeforePhase1()`
- `queueHostEventsBetweenPhases()`
- `pollInputSnapshot()`
- `flushEngineCommands()`
- `collectFeedbackEvents()`

### 13.1 Reduced Exported DLL Surface

The gameplay DLL should expose only the minimum engine-agnostic runtime boundary:

- runtime lifecycle: create and destroy
- host-event submission for gameplay/app requests
- translated feedback-event submission for engine execution observations
- `phase1` runtime entry
- `phase2` runtime entry
- engine-command pop/read

Current boundary rule:

- host-to-game data is submitted as input snapshots plus host/feedback events
- game-to-host data is emitted as engine commands
- input snapshot is supplied at phase entry, not through engine-native callback objects
- translated feedback events must already be in world language before entering the DLL
- host events may be submitted in two windows each frame: once before `phase1` and once after the phase-1 engine-command flush but before `phase2`
- pre-`phase1` host events are for interactions the host already resolved before gameplay tick, including host-rendered UI actions that should affect phase-1 gameplay
- between-phase host events remain valid for interactions or adapter work that completes after the phase-1 flush
- per-frame real delta time is supplied to `phase1`; `phase2` is the same frame continuation and does not take a second frame delta

Host-event examples:

- `UI_ACTION`
- future host-owned semantic interactions that are not raw input snapshots

Translated feedback-event examples:

- translated contact observation
- translated trace hit
- translated animation notify

Engine-command examples:

- `SET_APP_STATE`
- `BEGIN_REGIONAL_MAP_SNAPSHOT`
- `REGIONAL_MAP_SITE_UPSERT`
- `REGIONAL_MAP_LINK_UPSERT`
- `END_REGIONAL_MAP_SNAPSHOT`
- `BEGIN_UI_SETUP`
- `UI_ELEMENT_UPSERT`
- `END_UI_SETUP`
- `BEGIN_SITE_SNAPSHOT`
- `SITE_TILE_UPSERT`
- `SITE_WORKER_UPDATE`
- `SITE_CAMP_UPDATE`
- `SITE_WEATHER_UPDATE`
- `SITE_INVENTORY_SLOT_UPSERT`
- `SITE_TASK_UPSERT`
- `SITE_PHONE_LISTING_UPSERT`
- `HUD_STATE`
- `NOTIFICATION_PUSH`
- `SITE_RESULT_READY`
- `PLAY_ONE_SHOT_CUE`

Current implementation note:

- the public header now defines a richer engine-command schema centered on app-state projection, regional-map projection, semantic UI setup projection, site projection, HUD/UI projection, site results, and one-shot cues
- the current runtime now emits a practical prototype subset of that richer contract: log text, set app state, UI setup batches for menu/panel surfaces, regional-map snapshot commands, site bootstrap snapshots on site entry/resync, authoritative site partial-update batches after bootstrap, HUD state, and site result ready
- inventory/task/phone/notification/one-shot cue commands are defined in the schema now but are not fully emitted yet by gameplay runtime code

### 13.2 Engine Command Contract Shape

To keep DLL exposure low, the engine-command contract should be the main host-facing projection channel.

The required shape is:

- snapshot begin/end commands for long-lived surfaces such as regional map and active site
- snapshot begin/end plus element upsert commands for semantic UI surfaces owned by gameplay
- upsert/remove commands for list-like state such as sites, links, tasks, phone listings, and inventory slots
- direct state-update commands for singleton surfaces such as app state, worker state, camp state, weather state, HUD state, and site result state
- one-shot cue commands for transient presentation hints

Regional-map command contract:

- `BEGIN_REGIONAL_MAP_SNAPSHOT` starts a full authoritative map projection update
- `REGIONAL_MAP_SITE_UPSERT` updates one site node projection
- `REGIONAL_MAP_LINK_UPSERT` updates one adjacency link projection
- `END_REGIONAL_MAP_SNAPSHOT` closes the batch

Semantic UI setup contract:

- `BEGIN_UI_SETUP` starts a full authoritative semantic UI surface update
- `UI_ELEMENT_UPSERT` describes one element using DLL-owned meaning such as text, element kind, flags, and returned action token
- `END_UI_SETUP` closes the batch
- UI setup data intentionally omits host-owned layout, size, animation, and style details

Site command contract:

- `BEGIN_SITE_SNAPSHOT` with `SNAPSHOT` mode starts a full authoritative site bootstrap/resync batch
- `BEGIN_SITE_SNAPSHOT` with `DELTA` mode starts an authoritative partial site-update batch against an already projected site world
- `SITE_TILE_UPSERT` projects one tile cell or changed tile cell
- `SITE_WORKER_UPDATE`, `SITE_CAMP_UPDATE`, and `SITE_WEATHER_UPDATE` project authoritative site-state slices
- `SITE_INVENTORY_SLOT_UPSERT`, `SITE_TASK_UPSERT`, and `SITE_PHONE_LISTING_UPSERT` project host-side UI collections when those systems are active
- `END_SITE_SNAPSHOT` closes the batch

Projection rules:

- snapshot commands describe authoritative visual rebuild/bootstrap state
- delta-mode site batches describe authoritative partial state updates after the host already has a valid site projection baseline
- the host should treat begin/end snapshot commands as transactional fences for the matching presentation surface
- the host must not infer gameplay state that was never sent; if a surface is host-relevant, the DLL should emit a command for it
- the gameplay DLL should emit projection commands only when the authoritative gameplay state for that surface actually changed
- if a specific typed projection command already describes the changed presentation slice, prefer that over emitting an extra generic dirty notification
- delta-mode site updates should carry authoritative changed state, not arithmetic change amounts; for example worker movement should send the worker's new transform state, not a movement delta to be accumulated by the host
- adapters should build the long-lived site world from the bootstrap batch, maintain stable object mappings, and apply later site-state slices onto those existing projected objects

### 13.3 Mock Engine Update Loop

For a simulated or real engine host, one frame update should look like:

1. gather engine input and translate it into the shared input snapshot
2. optionally submit any host events already resolved before gameplay tick
3. call `phase1(delta_seconds, input_snapshot)`
4. flush phase-1 engine commands through the engine adapter
5. optionally submit newly resolved host events plus translated feedback events
6. call `phase2()`
7. flush phase-2 engine commands through the engine adapter

Timing rule:

- every engine frame has a real delta time
- the gameplay DLL needs that delta for `phase1` so fixed-step simulation can advance correctly
- `phase2` does not take another delta because it belongs to the same frame and only resolves between-phase ingress plus follow-up commands
- the same host-event API may be used in both ingress windows; submission timing decides whether the runtime handles those events in `phase1` or `phase2`

It must not:

- decide task completion
- decide site completion
- decide damage outcomes
- change authoritative gameplay state directly

## 14. Persistence Rule For V1

V1 has no disk save/load.

To remove ambiguity:

- `CampaignState` lives only in memory during process lifetime
- closing the app discards the campaign
- retrying a failed site rebuilds a fresh `SiteRunState` from authored content plus current campaign progression
- no serializer or save repository is required in the first coding stage

This is an explicit implementation decision, not an omission.

## 15. Testing Strategy

Required test layers:

### 15.1 Unit Tests

Test pure rules and calculators:

- reward draft generation
- task progress checks
- plant density updates
- modifier aggregation
- loadout assembly
- technology prerequisite validation

### 15.2 Integration Tests

Test full subsystem flows:

- site attempt start
- buy item -> delivery -> storage -> use item
- accept task -> complete task -> claim reward
- site failure -> retry -> fresh attempt state
- site completion -> regional support unlock

### 15.3 Simulation Tests

Run deterministic fixed-step site simulations for:

- harsh event survival
- moisture/erosion/growth balance sanity
- board refresh and chain behavior
- modifier stacking limits

## 16. Immediate Coding Order

Code in this order:

1. `content/` loading, validation, and id indexes
2. `runtime/` fixed-step runner, queues, rng, ids
3. `campaign/` state and regional-map flow
4. `site/` state structs and tile grid
5. weather/event and local-weather resolution
6. worker condition, camp durability, and inventory
7. action execution and placement validation
8. ecology and device systems
9. task board, rewards, modifiers, and economy phone
10. ui view models and host-side integration mapping

This order minimizes rework because content and runtime foundations exist before high-level systems start depending on them.

## 17. Final Readiness Check

After this document, the only major system-design topic that remains intentionally deferred is disk save/load.

For the first playable coding stage, the implementation shape is now locked strongly enough to start writing code:

- one game project
- one authoritative campaign state
- one authoritative site run state
- dense tile arrays
- command-driven cross-owner mutation
- validated content database
- explicit system ownership
- explicit folder structure
- explicit update order

That is the v1 coding baseline.
