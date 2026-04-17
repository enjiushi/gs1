# System Design Status

Sources:

- [GDD.md](GDD.md)
- [GAME_STRUCTURE.md](GAME_STRUCTURE.md)
- [CONTENT_AUTHORING_CONTRACT.md](CONTENT_AUTHORING_CONTRACT.md)

Purpose: summarize what the current design documents already define well enough for the next system-design stage, and identify the remaining missing design contracts before we move into detailed code modules, data structures, and implementation ownership.

Boundary rule:

- `GDD.md` should stay at gameplay-contract, content-direction, and meter-relationship level.
- `GAME_STRUCTURE.md` defines the global code framework, ECS world model, message/event pipeline, world-level engine messages, and engine-adapter boundary.
- Exact structs, field lists, save schemas, and module ownership belong in the later system-design documents.

## Overall Verdict

- Prototype path: ready to move into system design.
- Full-campaign path: not fully ready yet.
- Current strength: the core site loop is already coherent across survival, hazards, planting, devices, tasks, faction reputation, and small tech growth; the global ECS/message architecture is explicitly defined; and the missing authoring-side content contract is now formalized.
- Current risk: save/load boundaries remain an intentional later TODO, so full long-session architecture should still avoid pretending persistence is already solved.

## What Is Already Stable Enough

These areas are defined strongly enough to start module design and runtime-structure planning now:

- Global world-first ECS architecture
- Message/event-driven cross-system communication
- World-level engine-message boundary and engine-adapter boundary
- Shared schema layering direction from global to feature to game-specific data
- Core gameplay loop and site loop
- Fixed-step simulation contract and high-level update order
- Tile model, occupancy model, tile layers, placement, and sharing rules
- Terrain meters and plant meters
- Weather, hazard, harsh-event, and aftermath model
- Worker condition model
- Player base, containers, water handling, and contractor direction
- Data-driven item model with runtime item meters
- Small device roster and device behavior contract
- Plant roster direction, density model, growth pressure, spread, salinity, and straw checkerboard behavior
- Contract-board concept, prototype task model, faction publishing, reward-draft direction, chain-task direction, and accepted-task-cap concept
- Faction identities, assistants, onboarding order, and prototype four-site arc
- Data-driven faction-technology model with shared node model and per-unlockable update families
- Meter-relationship chapter as the current implementation-facing causal reference

These areas are good enough that the next step can define code ownership, runtime structs, content-definition tables, and service boundaries without waiting for more game-loop design.

## What Is Defined But Still Needs One More Contract Pass

No remaining prototype-critical gameplay or content-authoring contract is still in this state. The main deferred formal topic is now save/load boundaries for later full-campaign architecture.

## What Is Still Missing As Formal Design

These areas are the main missing formal sections if we want a safe full-game architecture:

- save-data boundaries as a later save/load TODO

## Practical Readiness By System

| Area | Status | Why |
|---|---|---|
| Global framework and engine boundary | `Ready` | `GAME_STRUCTURE.md` now defines the ECS-world-first architecture, message/event flow, world-level engine messages, engine-adapter responsibilities, the semantic host-event intake contract, and the bootstrap-plus-partial-update adapter projection rule for long-lived presentation surfaces. |
| Site runtime simulation | `Ready` | Update order, meters, causal loop, and tile/runtime state categories are already defined. |
| Terrain and ecology runtime | `Ready` | Terrain, plant, weather, device, and meter relationships are clear enough for module and struct design. |
| Inventory and item runtime | `Ready` | Shared item model, item meters, storage flow, and hazard interaction are already explicit. |
| Device runtime | `Ready` | Device roster is small and the behavior contract is defined well enough for data tables and instance state. |
| Crafting and device storage runtime | `Ready` | Crafting now assumes authored item materials, per-device storage, nearby-item caching, timed craft actions, and global sell visibility. |
| Faction reputation and tech | `Ready` | Branch identity, node model, progression meters, and content-update model are already strong enough for persistent-state design. |
| Contract board runtime | `Ready` | Prototype task data shape, completion, reward-generation, and chain rules are now defined strongly enough for system design. |
| Economy runtime | `Ready` | Economic flow, buy/sell directions, and relative value hierarchy are clear enough for system design; exact prices stay as later tuning tables. |
| Campaign meta and nearby-site support | `Ready` | Reveal rules, static adjacency, route choice, campaign-time behavior, authored support packages, export timing, and the single-site-output-modifier rule are now defined strongly enough for system design. |
| Failure, retry, and save systems | `Ready for prototype` | Failure and retry rules are defined strongly enough for the current prototype path. Save/load remains a later TODO rather than an active blocker for now. |
| Off-screen simulation | `Ready` | Prototype scope only resolves completed-site exported-support catch-up when the `Regional Map` opens; broader future off-screen simulation stays deferred. |

## What The Next System-Design Stage Can Safely Produce Now

The next stage can already define these artifacts:

- gameplay module boundaries inside the ECS world
- content-definition tables
- persistent-state boundaries for systems that are already stable
- site-session runtime state structs
- tile and occupancy data structures
- plant, device, item, recipe, task, and tech definition schemas
- modifier-bundle representation
- simulation service boundaries inside the world layer
- game-specific message/event contracts within the global message pipeline

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
- `SaveLoadModule` later, when save/load enters scope

The names can change later. The important point is that these ownership lines are now visible in the design.

When the next system-design pass assigns concrete game-state ownership, use this module split as the reference layout. One module may contain multiple closely related systems, but authority should still map back to one owning module rather than being duplicated across several systems.

These gameplay modules should live inside the world-first ECS architecture defined in [GAME_STRUCTURE.md](GAME_STRUCTURE.md). Cross-module interaction should follow message/event flow and shared state access rules rather than direct system-to-system calls.

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

These should be treated as game-specific schemas layered on top of the more global schema split already established in [GAME_STRUCTURE.md](GAME_STRUCTURE.md).

## Recommended First System-Design Order

If the goal is prototype-first implementation, use this order:

1. Site-session runtime state and fixed-step simulation ownership
2. Tile, plant, weather, and device data structures
3. Item, inventory, water-transfer, and crafting data structures
4. Task-board runtime schema and reward-draft schema
5. Faction reputation and technology persistence schema
6. Prototype site-flow state machine across the four authored sites
7. Longer-term regional-support expansion and save/load boundaries only after the prototype path is proven and save/load enters scope

This order follows the current GDD maturity level and avoids overcommitting to campaign-meta systems before the site loop is playable.

## Main Missing Contracts Before Full System Design

If we want to move beyond prototype-scoped system design into full-game architecture, the next required design documents should be:

- `SAVE_BOUNDARIES_CONTRACT.md` later, when save/load enters scope

The authoring-side contract is now covered by [CONTENT_AUTHORING_CONTRACT.md](CONTENT_AUTHORING_CONTRACT.md). The remaining later document is the save-boundary contract when persistence actually enters implementation.

## Prototype-Specific Guidance

Because the current target is still the minimum playable prototype, several missing full-game contracts do not need to block the next system-design stage immediately:

- procedural regional-map generation can stay deferred
- broad off-screen site simulation can stay deferred beyond the prototype catch-up rule
- full save design can stay deferred for the prototype, but the save-boundary chapter should remain in the backlog because it is still necessary before longer-form architecture is finalized
- broad economy simulation can stay deferred
- full regional-support formulas can be stubbed or replaced by authored prototype progression

So the correct reading of the current GDD is:

- strong enough for prototype system design
- not yet fully safe for final full-campaign architecture

## Final Recommendation

- Proceed now into prototype-focused system design.
- Keep [GAME_STRUCTURE.md](GAME_STRUCTURE.md) as the global framework contract.
- Use [CONTENT_AUTHORING_CONTRACT.md](CONTENT_AUTHORING_CONTRACT.md) as the generator/content-schema contract.
- Freeze gameplay ownership for site simulation, ecology, items, devices, tasks, reputation, and technology inside that framework first.
- Leave save/load boundaries as an explicit later TODO before committing to final full-game architecture.

That is the cleanest next step from the current GDD state.
