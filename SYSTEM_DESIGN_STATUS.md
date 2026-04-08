# System Design Status

Source: [GDD.md](d:/testgame/gs1_upstream/GDD.md)

Purpose: summarize what the current GDD already defines well enough for the next system-design stage, and identify the remaining missing design contracts before we move into code modules, data structures, and game-structure planning.

## Overall Verdict

- Prototype path: ready to move into system design.
- Full-campaign path: not fully ready yet.
- Current strength: the core site loop is already coherent across survival, hazards, planting, devices, tasks, faction reputation, and small tech growth.
- Current risk: the campaign meta layer and task-generation layer still have missing formal contracts that can cause churn if we jump into large-scale architecture too early.

## What Is Already Stable Enough

These areas are defined strongly enough to start module design and runtime-structure planning now:

- Core gameplay loop and site loop
- Fixed-step simulation contract and high-level update order
- Tile model, occupancy model, tile layers, placement, and sharing rules
- Terrain meters and plant meters
- Weather, hazard, harsh-event, and aftermath model
- Worker condition model
- Player base, containers, water handling, and contractor direction
- Data-driven item model with runtime item meters
- Small device roster and device runtime contract
- Plant roster direction, density model, growth pressure, spread, salinity, and straw checkerboard behavior
- Contract-board concept, faction publishing, reward-draft direction, chain-task direction, and accepted-task-cap concept
- Faction identities, assistants, onboarding order, and prototype four-site arc
- Data-driven faction-technology model with shared node schema and per-unlockable update families
- Meter-relationship chapter as the current implementation-facing causal reference

These areas are good enough that the next step can define code ownership, runtime structs, content-definition tables, and service boundaries without waiting for more game-loop design.

## What Is Defined But Still Needs One More Contract Pass

These systems are conceptually solid, but still need one tighter design pass before their final runtime schema should be frozen:

- `Site Task` runtime schema
- exact task refresh timing
- exact task completion detection rules
- exact reward-draft generation rules
- anti-duplication and anti-dead-roll rules for reward drafts
- exact `Task Chain` generation and continuation rules
- site certification and completion thresholds
- exact `Loadout` assembly contract
- exact regional-support accrual timing
- exact regional-support export timing
- exact `Regional Support Output` numeric contract
- exact `Site Output Modifier` generation and stacking contract
- failure trigger and restart-recovery contract
- off-screen site simulation contract
- economy numeric contract for prices, payouts, contractor costs, and direct-purchase costs

These are not vague ideas anymore, but they are still not tight enough to lock final schemas without risking redesign.

## What Is Still Missing As Formal Design

These areas are the main missing formal sections if we want a safe full-game architecture:

- save-data boundaries
- persistence ownership by system
- explicit off-screen simulation formulas
- explicit failure and restart formulas
- full campaign-map progression contract beyond the prototype slice
- final authored-versus-procedural site-content boundary
- explicit authoring contract for task templates, event templates, and reward-draft templates

## Practical Readiness By System

| Area | Status | Why |
|---|---|---|
| Site runtime simulation | `Ready` | Update order, meters, causal loop, and tile/runtime state categories are already defined. |
| Terrain and ecology runtime | `Ready` | Terrain, plant, weather, device, and meter relationships are clear enough for module and struct design. |
| Inventory and item runtime | `Ready` | Shared item schema, item meters, storage flow, and hazard interaction are already explicit. |
| Device runtime | `Ready` | Device roster is small and the runtime contract is defined well enough for data tables and instance state. |
| Faction reputation and tech | `Ready` | Branch identity, node schema, progression meters, and content-update model are already strong enough for persistent-state design. |
| Contract board runtime | `Yellow` | Strong direction exists, but final task-instance schema and generation rules still need one more contract pass. |
| Economy runtime | `Yellow` | Economic flow is clear, but numeric tables and exact pricing contracts are still missing. |
| Campaign meta and nearby-site support | `Yellow/Red` | Good direction exists, but loadout assembly, regional-output timing, and support formulas are not yet formal enough. |
| Failure, retry, and save systems | `Red` | Still missing formal persistence and restart boundaries. |
| Off-screen simulation | `Red` | High-level intent exists, but not enough contract detail for safe implementation. |

## What The Next System-Design Stage Can Safely Produce Now

The next stage can already define these artifacts:

- runtime module boundaries
- content-definition tables
- persistent-state boundaries for systems that are already stable
- site-session runtime state structs
- tile and occupancy data structures
- plant, device, item, recipe, task, and tech definition schemas
- modifier-bundle representation
- simulation service boundaries
- event/update pipeline ownership

The next stage should not wait for exact balance numbers. It should instead define the structural contract for how those numbers are stored and applied.

## Recommended Runtime Modules

Suggested first-pass code-module split:

- `CampaignMetaModule`
- `FactionReputationModule`
- `TechnologyModule`
- `SiteSessionModule`
- `TileGridModule`
- `PlacementValidationModule`
- `WeatherEventModule`
- `WorkerConditionModule`
- `EcologyModule`
- `StructureDeviceModule`
- `InventoryItemModule`
- `CraftingRecipeModule`
- `EconomyPhoneModule`
- `TaskBoardModule`
- `ModifierModule`
- `FailureRecoveryModule`
- `SaveLoadModule`

The names can change later. The important point is that these ownership lines are now visible in the design.

## Recommended Core Data Definitions

The next stage should formalize these data definitions first:

- `CampaignState`
- `FactionProgressState`
- `TechnologyState`
- `TechnologyNodeDef`
- `SiteSessionState`
- `SiteArchetypeDef`
- `TileState`
- `PlantDef`
- `PlantOccupantState`
- `StructureDef`
- `StructureState`
- `ItemDef`
- `ItemStackState`
- `RecipeDef`
- `TaskTemplateDef`
- `TaskInstanceState`
- `RewardDraftDef`
- `ModifierBundleDef`
- `WeatherState`
- `EventState`

## Recommended First System-Design Order

If the goal is prototype-first implementation, use this order:

1. Site-session runtime state and fixed-step simulation ownership
2. Tile, plant, weather, and device data structures
3. Item, inventory, water-transfer, and crafting data structures
4. Task-board runtime schema and reward-draft schema
5. Faction reputation and technology persistence schema
6. Prototype site-flow state machine across the four authored sites
7. Failure/restart/save boundaries for the prototype path
8. Regional-support and off-screen systems only after the prototype path is proven

This order follows the current GDD maturity level and avoids overcommitting to campaign-meta systems before the site loop is playable.

## Main Missing Contracts Before Full System Design

If we want to move beyond prototype-scoped system design into full-game architecture, the next required design documents should be:

- `TASK_RUNTIME_CONTRACT.md`
- `TASK_GENERATION_AND_REWARD_CONTRACT.md`
- `LOADOUT_AND_REGIONAL_SUPPORT_CONTRACT.md`
- `SITE_COMPLETION_AND_CERTIFICATION_CONTRACT.md`
- `FAILURE_RESTART_AND_SAVE_CONTRACT.md`
- `OFFSCREEN_SIMULATION_CONTRACT.md`
- `ECONOMY_NUMERIC_CONTRACT.md`

These do not need final balance. They do need exact field ownership, event timing, and persistence rules.

## Prototype-Specific Guidance

Because the current target is still the minimum playable prototype, several missing full-game contracts do not need to block the next system-design stage immediately:

- procedural regional-map generation can stay deferred
- broad off-screen site simulation can stay deferred
- full save schema can be minimal at first
- broad economy simulation can stay deferred
- full regional-support formulas can be stubbed or replaced by authored prototype progression

So the correct reading of the current GDD is:

- strong enough for prototype system design
- not yet fully safe for final full-campaign architecture

## Final Recommendation

- Proceed now into prototype-focused system design.
- Freeze runtime ownership for site simulation, ecology, items, devices, tasks, reputation, and technology first.
- Do one more contract pass on tasks, campaign meta, restart/save, and off-screen logic before committing to final full-game architecture.

That is the cleanest next step from the current GDD state.
