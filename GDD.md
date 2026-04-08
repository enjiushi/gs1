# Desert Restoration Survival Strategy

Version: 0.1  
Status: First draft production GDD  
Document goal: Define the v1 design clearly enough for prototyping, production planning, and future iteration.

## 1. High Concept

`Desert Restoration Survival Strategy` is a single-player PC game about surviving and reversing desertification in a remote China-inspired desert region. The player controls one government-backed field worker who enters dangerous restoration sites, endures heat, thirst, wind, and sandstorms, establishes light camp infrastructure, plants resilient species, completes faction-published site tasks, and gradually turns a failing landscape into a stable ecological network.

The game combines direct on-foot play with site-scale strategy. The player is not a commander floating above the world and not a combat hero. The fantasy is physical work under environmental pressure: carrying enough water, reading the forecast, choosing where to plant first, protecting young growth from erosion, and deciding when to spend limited funds on tools, devices, contractors, or another day of survival.

## 2. Product Summary

| Item | Decision |
|---|---|
| Genre | Survival strategy / restoration sim |
| Camera | Top-down 3D |
| Platform | PC first |
| Mode | Single-player |
| Structure | Finite campaign on a random regional map with multiple sites |
| Session shape | Medium-length site runs with forward regional progression |
| Main threats | Heat, dehydration, wind, sandstorms, extreme hazard events, erosion, poor planning |
| Combat | None in v1 |
| Setting | Present-day, grounded, China-inspired remote desert |
| Narrative weight | Mostly sandbox with light contextual storytelling |
| Replay value target | High run-to-run variety through procedural generation, task-board variation, events, and strategic progression choices |
| Tech model | Dual-layer progression with cumulative-reputation faction tech branches plus task-revealed site unlocks and session-only modifiers |

## 3. Vision And Pillars

### Vision

Create a survival strategy game where the player feels the harshness of desert field work and the satisfaction of slowly making the land safer, greener, and more stable.

### Pillars

- Harsh but fair survival: failure comes from visible physical pressure and planning mistakes, not hidden randomness.
- Restoration over extraction: the main fantasy is healing land and building resilience, not stripping the environment for profit.
- Field-work immediacy: the player acts through one worker using direct controls, so every plant, shelter, and trip for water feels personal.
- Regional ecological spillover: success at one `Site` helps nearby `Site`s through support output and environmental stabilization.
- Readable systems: tile effects, plant roles, weather, and resource pressure must be understandable at a glance.
- High replay value: every campaign should generate a distinct strategic story through different maps, site conditions, task-board offers, weather patterns, events, rewards, and tech priorities.
- Fierce hazard drama: certain environmental events should become genuinely dangerous set pieces that make survival uncertain, heighten immersion, and create strong fulfillment if the player endures them.

## 4. Core Design Terms

These terms are stable and should be used consistently in future design and implementation discussions.

| Term | Definition |
|---|---|
| `Site` | A playable restoration area on the regional map with its own terrain, hazards, task goals, local resources, and camp state. |
| `Regional Map` | The campaign-level planning layer containing multiple connected `Site`s, their adjacency, regional conditions, available site starts, support links, and access to faction-tech selection. |
| `Contract Board` | The on-site progression system available only during an active `Site` session; it contains the current site's faction-published `Site Task` pool that drives site progression and rewards. |
| `Field Phone` | The player's in-world access point for money-related actions during a `Site` session; it supports the on-site `Contract Board` plus buying, selling, hiring, and related field planning actions. |
| `Reputation` | The cumulative campaign-level government standing resource earned progressively from official work; it is never spent and instead gates faction-tech tiers and broader program trust. |
| `Persistent Tech Tree` | The long-term campaign progression layer formed by three faction tech branches plus any neutral program-wide upgrades. |
| `Faction` | One of the three institutional backers on the `Contract Board`; each `Faction` has its own play style, task bias, random events, assistant support, and tech branch. |
| `Faction Reputation` | The cumulative standing value with one specific `Faction`, earned mainly through that faction's tasks and events; it unlocks branch nodes and improves post-event help. |
| `Faction Assistant` | A temporary support package provided by a `Faction`, such as workforce aid, device upgrades, or plant-water support. |
| `Concept Unlock` | A real progression gate used during onboarding, where a new gameplay concept becomes available only after the player has meaningfully used the previous one; this is not just a tutorial popup or hidden full system. |
| `Learning Budget` | The maximum amount of genuinely new gameplay the player is expected to absorb from one site game's rewards and unlocks; reputation pacing should keep this budget small. |
| `Onboarding Task` | An authored early-campaign task whose purpose is to teach one faction or major gameplay layer through real play and permanently unlock it for future sites. |
| `Tutorial Task Set` | A manually authored set of early faction tasks that temporarily overrides normal refresh rules until the player has completed each intended basic task type at least once. |
| `Site Unlockables` | The site-scoped plants, devices, tools, and other temporary options for the current `Site`; they are primarily revealed through some task rewards, but a limited subset can also be bought directly for very high money cost. Only options already permitted by the `Persistent Tech Tree` may appear in the pool. |
| `Site Task` | A short 5 to 10 minute objective generated from the current site's state, listed in the `Contract Board`, and used to provide immediate goals and quick rewards. |
| `Task Pool Size` | The maximum number of concurrently offered `Site Task`s shown for the current `Site` in the `Contract Board`, upgradable through the `Persistent Tech Tree`. |
| `Accepted Task Cap` | The maximum number of `Site Task`s the player may have accepted at the same time; accepted tasks are pinned until completed or withdrawn through a costly emergency rule that should only reduce trust earned during the current site. |
| `Task Tier` | The rarity and difficulty level of a `Site Task`, which controls how often it appears, how hard it is to finish, and how strong its rewards feel. |
| `Task Reward Draft` | The choice set revealed after completing a `Site Task`, typically with `3` or more options; the options can include `Site Unlockables`, `Run Modifier`s, or resource-focused rewards, but not every draft must include an unlockable. |
| `Task Reward Package` | A money, resources, or mixed tactical-bundle option that can appear inside a `Task Reward Draft`. |
| `Run Modifier` | A site-session-only rule-changing effect such as better sell prices, work efficiency, plant growth rate, or weather response; it lasts only for the current site session and is lost if the player leaves, fails, or restarts that site. |
| `Task Chain` | A linked set of tasks generated inside a task pool refresh; each task pays normal rewards, but completing the full chain grants an extra reward and encourages priority-based planning. |
| `Site Commendation` | A government-issued prize awarded when a `Site` is successfully restored; it includes a title plus a cited reason based on how the site was completed, giving the player recognition and a clear record of progress. |
| `Campaign Clock` | The fixed campaign time limit that pressures the player to complete as many `Site`s as possible before the run ends. |
| `Fully Grown Tile` | A planted tile that has reached the mature density state counted toward `Site` completion. |
| `Loadout` | The supplies, seeds, tools, and devices chosen before entering a `Site`. |
| `Inventory` | The player's slot-based carried storage for `Item`s during a `Site` session. Capacity is limited and can be expanded by the `Persistent Tech Tree`. |
| `Item` | Any carriable object that can occupy `Inventory` or `Container` slots, including seeds, tools, portable devices, materials, medicines, water, and harvested goods. |
| `Container` | A camp storage object that can be built or bought on-site and used to store `Item`s outside the player's carried `Inventory`. |
| `Site Output Modifier` | A persistent bonus trait attached to a stabilized site's regional support output, such as increased wind protection, fertility support, specific resource yield, or support range. |
| `Camp Support` | The light support infrastructure on a `Site`, including shelter, `Container`s, service devices, and hired labor access. |
| `Player Condition` | The worker's physical and mental state, represented by survival and output meters such as health, hydration, nourishment, energy cap, energy, morale, and work efficiency. |
| `Aftermath Relief Offer` | A faction support offer that can appear after a harsh event enters `Aftermath`; its strength depends on current `Faction Reputation` and site damage. |
| `Wind Protection` | The total local wind-erosion shielding on a tile or patch from plant adjacency, terrain shelter, and devices; higher protection reduces exposure damage and fertility loss. |
| `Plant Density` | The current strength and maturity of living cover or plant-derived starter cover on a tile or cluster; higher density means stronger traits, better survival, and possible natural spread for living plants, while lower density means weaker effects and higher hazard vulnerability. |
| `Plant Trend` | The derived direction of plant density on a tile, shown as `Growing`, `Holding`, or `Withering`, based on whether the patch is gaining or losing density. |
| `Plant Trait` | A special property attached to a plant family or unlocked variant that modifies placement value, survival, or local ecological effects. |
| `Regional Support Output` | The passive benefit a stabilized `Site` contributes to nearby sites on the `Regional Map`, including loadout resources and small ecological spillover effects such as protection or fertility support. |

## 5. Player Fantasy And Setting

The player is a desert-control worker assigned through a public ecological restoration program. The job is practical, underfunded, and dangerous. Each deployment is a mix of field labor, survival, applied ecology, and task-driven performance.

The setting is inspired by real desert-restoration work in remote Chinese regions, but it is not tied to a specific real province, project, or policy. This allows the game to feel grounded while preserving room for fictional site design, procedural generation, and game-driven clarity.

The emotional arc should move through three phases:

- Fragility: the player arrives underprepared, exposed, and focused on basic survival.
- Control: the player learns weather patterns, stabilizes key tiles, and builds support capacity.
- Recovery: the player watches once-hostile ground become productive, protective, and connected to a wider regional effort.

The long-term replay goal is for players to finish a campaign and immediately imagine a different next run: another regional layout, another opening support network, another weather rhythm, another task-board economy, and another tech path.

## 6. Core Gameplay Loop

### Campaign Loop

1. Review the `Regional Map`, off-screen site states, and which `Site`s are currently startable.
2. Review candidate `Site`s for terrain summary, weather tendencies, local constraints, and the support modifiers currently provided by nearby stabilized sites.
3. Review and purchase available `Persistent Tech Tree` nodes before deployment.
4. Assemble a `Loadout` from nearby-site support plus currently unlocked campaign options.
5. Choose the target `Site` and deploy into it.
6. Open that site's `Contract Board` during the active session, then receive the current site-task and unlock pool for that site, with each `Site Task` carrying a publishing `Faction`, a place in the refreshable task pool, and rewards shaped by the current `Persistent Tech Tree` build.
7. Survive the day cycle while establishing basic `Camp Support`.
8. Plant, build, craft, maintain, and respond to weather.
9. Complete `Site Task`s to reveal `Task Reward Draft`s, choose from whatever mix of unlockables, modifiers, or resource rewards the draft offers, and use money either on revealed `Site Unlockables` or on very expensive direct-purchase unlockables when needed.
10. Fulfill site-task goals and any optional bonus objectives.
11. Stabilize the site enough to extract `Regional Support Output`.
12. Return to the `Regional Map`, unlock new options, and choose the next priority.

### Site Loop

1. Assess terrain, forecast, and immediate risk.
2. Secure water, shade, shelter, and basic work capacity.
3. Review the current pool of `Site Task`s, their publishing `Faction`, open acceptance slots, and the local site conditions that will shape which `Task Reward Draft`s, `Site Unlockables`, and modifiers become available.
4. Pursue whichever `Site Task`s best fit the current terrain, weather, active tech build, short-term reward priorities, and desired faction alignment.
5. Prepare tiles and place early protective or restorative plants in the most exposed lanes first.
6. Build key support devices such as irrigation, sensors, or service stations.
7. Watch weak low-density zones for density loss, erosion, burial, and other degeneration before they spread.
8. Balance personal survival against site growth, repairs, task commitments, and the pressure to finish current tasks so new opportunities can enter the pool.
9. Use the `Field Phone` to spend money on revealed `Site Unlockables`, supplies, harsh-event response, or contractors when needed.
10. Use normal field work such as burial clearing, watering, and repair under harsh-event pressure when needed to protect planted regions or rescue failing utility lines; sometimes sacrifice one fringe area to save a core patch, then evaluate any `Aftermath Relief Offer`s once the event breaks.
11. Convert surviving short-term gains into denser self-sustaining cover and the next safer expansion.

### Strategic Pressure Loop

For the first playable, the core site loop should not be "place plants and wait." It should be "stabilize, survive, exploit, repair, and expand."

The intended rhythm is:

1. Read the current terrain and forecast, then identify the most dangerous exposed patch.
2. Spend limited early money and energy on a small protective plant combo or support device.
3. Try to survive the next heat spike, wind lane, sandburst, or erosion window.
4. If the patch survives, use its output, safety, or restored fertility to unlock a better plant or tool.
5. Expand into a new patch or upgrade the first one from safe to productive.
6. When the environment hits back, choose whether to rescue the failing area with normal work, or abandon it and save the core, knowing that harsh-event conditions make every outdoor action riskier and more energy-expensive.
7. Reinvest the gains from survival into the next stronger combo.

This creates the intended strategy arc:

- good choices create safer land, stronger plants, more output, and easier future expansion
- bad choices create density loss, withering, money loss, risky rescue work, and local collapse that must be reversed deliberately

The important target is not perfect simulation. It is to make each planted patch feel like a strategic bet that can either mature into a thriving foothold or slide into a costly recovery problem.

## 7. World Structure

### Regional Map

The campaign plays across a large random map made of multiple connected `Site`s. Sites are selectable rather than fully open-world continuous. This keeps scope manageable while allowing strategic route planning and meaningful adjacency.

Regional-map site states:

-- `Locked`: unavailable and not startable for immediate deployment
-- `Available`: not yet completed and currently startable
-- `Completed`: successfully restored and contributing `Regional Support Output`

Each `Site` has:

- A biome variant within the desert theme
- Weather tendencies
- Terrain features
- Different starting constraints
- Potential links to neighboring sites

Each candidate `Site` should define at minimum:

- whether the site can currently be started or must remain locked
- its completion state and whether it is an unresolved target or a completed support site
- its major terrain and weather pressures
- its current nearby support contributors
- the exact support modifiers those nearby sites provide, such as wind protection, heat protection, fertility support, resource export, or extended support range
- any especially important `Site Output Modifier`s affecting why that site is strategically valuable

The `Regional Map` should be procedural enough that each campaign creates a new strategic puzzle. Variety should come from:

- Different site adjacency and route pressure
- Uneven starting support opportunities
- Different concentrations of harsh versus forgiving sites
- Varying access to terrain features such as lakes, dunes, rocky shelter, or exposed basins
- Different clusters of ecological synergy between neighboring sites

### Site Generation

Each `Site` should be generated with enough variation that replaying the same campaign role still feels fresh. Key variable dimensions include:

- Tile layout and traversable paths
- Exposure to wind corridors
- Shelter opportunities
- Water access or lack of it
- Soil fertility baseline
- Terrain difficulty for the fixed early plant unlock set
- Contract-friendly versus survival-hostile terrain

Starting plant access should not be procedurally generated. Each site should begin from the same small basic plant progression, and the player should unlock those basic available plants during the site session rather than receiving a random starting plant set.

Procedural generation should create different problem shapes, not random noise. The player should feel they are solving a new site, not re-learning unreadable chaos.

### Adjacency And Spillover

Sites influence nearby sites in two ways:

- Ecological spillover: stabilized plant cover can grant small wind, heat, or fertility advantages to adjacent sites.
- Resource support: successful sites can contribute limited loadout resources and starting help to nearby sites.

### No Revisits In Current Design

For the current design, completed sites should not be revisited as a normal feature. Once a site is completed, it should function only as a regional support source on the `Regional Map`.

The only repeat attempt currently supported is retrying an unresolved site after a failed site session.

### Off-Screen Simulation

Sites continue evolving when the player is away through a simplified background simulation. Stable sites remain productive. Fragile sites may degrade slowly. Severe weather can stress weak sites. This keeps the `Regional Map` alive without requiring full real-time simulation everywhere.

Because the map is dynamic, repeated campaigns should not settle into one dominant route. Site deterioration, background weather pressure, and support availability should keep campaign pacing variable from run to run.

### End State

The campaign ends when the `Campaign Clock` runs out. The player's success is measured mainly by how many `Site`s were completed within that fixed time limit. This makes finishing sites efficiently a core campaign-level goal.

## 8. Controls, Camera, And Interaction

### Control Scheme

- Movement: `WASD`
- Aiming and interaction targeting: mouse cursor
- Primary action: use tool / interact
- Secondary action: alternate tool function / cancel
- Placement mode: move into reach of a tile or object, then place or interact through the cursor-directed action
- Inventory: open the carried `Inventory` and manage `Item`s
- Phone: open the `Field Phone` for tasks, buying, selling, and hiring

### Interaction Requirement Rule

To reduce control complexity, the game should avoid a quick-slot-driven interaction model. The player only needs to keep the required `Item`s in carried `Inventory`.

For any interaction, construction, planting, repair, or other field work that requires `Item`s:

- if all requirements are satisfied, the interaction can be performed directly without separately equipping the needed tool or consumable into a quick slot

This should reduce control burden and keep the decision space focused on what the player chooses to carry rather than on rapid slot swapping.

### Field Phone

The `Field Phone` is the main diegetic access point for money-related interactions during an active `Site` session. Pre-deployment planning belongs to the `Regional Map`, not the phone. Once deployed, the player uses the phone for field economics and for opening the current site's `Contract Board`.

Core phone functions:

- Review and accept jobs from the current site's `Contract Board`
- Review the current site's `Site Task` pool and its refresh timing
- Review `Faction Reputation`, current `Faction Assistant` support, and unlocked faction-tech thresholds
- Buy water, food, medicine, seeds, tools, and approved support equipment
- Sell harvested output or approved goods
- Hire temporary contractors
- Spend money on revealed `Site Unlockables` on the current site
- Buy limited direct-purchase `Site Unlockables` at premium prices when task rewards did not provide the wanted option
- Track funds, incoming rewards, and current spending pressure

The phone should feel practical and grounded, like an everyday field tool adapted to harsh remote work.

### Camera

The camera is top-down 3D with enough tilt to read terrain, structures, and vegetation layers clearly. The view should support fast movement, readable tile effects, and weather visibility without feeling detached from the worker.

### Placement Model

Placement is tile or plot based. This improves clarity for:

- Plant adjacency
- Local environmental effects
- Build costs
- Work required per tile
- Balance and forecasting

Large devices can still have footprint differences, but all placement decisions should resolve to understandable tile logic.

### Runtime Simulation Contract

The game should use frame `deltaTime` for rendering, animation, and input responsiveness, but core gameplay simulation should not run directly on raw frame time. Instead, frame `deltaTime` should feed a fixed-step simulation accumulator.

Core time contract:

- `1` full in-game day = `30` real-time minutes
- `1` in-game day = `1440` in-game minutes
- recommended fixed simulation step = `0.25` real seconds
- each fixed simulation step advances time by `0.2` in-game minutes, or `12` in-game seconds

This keeps the simulation readable and stable while still feeling continuous to the player.

#### Canonical Runtime Value Types

Use internal continuous values for simulation.

Important cleanup rule:

- terrain soil state, occupancy state, temporary overlay state, and resolved local weather state should stay conceptually separate even if engineering stores them on one tile runtime struct

Continuous runtime values:

World and campaign values:

- `worldTimeMinutes`
- `weatherHeat`
- `weatherWind`
- `weatherDust`
- `playerHealth`
- `playerHydration`
- `playerNourishment`
- `playerEnergyCap`
- `playerEnergy`
- `playerMorale`
- `playerWorkEfficiency`
- `campDurability`
- `fullyGrownTileCount`
- `siteCompletionTileThreshold`
- `campaignDaysRemaining`

Persistent terrain soil meters on plantable `Ground`:

- `tileSoilFertility`
- `tileMoisture`
- `tileSoilSalinity`

Temporary or hazard-driven overlay values:

- `tileSandBurial`

Resolved local weather meters rebuilt every fixed simulation step:

- `tileHeat`
- `tileWind`
- `tileDust`

Plant runtime values:

- `tilePlantDensity`
- `growthPressure`

Structure runtime values:

- `deviceIntegrity`
- `deviceEfficiency`
- `deviceStoredWater`

Discrete or integer runtime values:

- `dayIndex`
- `terrainTypeId`
- `groundCoverTypeId`
- `plantTypeId`
- `structureTypeId`
- `plantDensityState`
- `taskState`
- `contractState`
- `eventState`
- `tileTraversable`
- `tilePlantable`
- `tileReservedByStructure`
- `workerPackSlotCount`
- `campStorageSlotCount`
- `money`
- `reputation`
- item counts such as water containers, food packs, repair kits, seeds, and harvested goods

#### Fixed-Step Update Order

For each fixed simulation step, resolve systems in this order:

1. Advance `worldTimeMinutes`, `dayIndex`, and current day-phase tags.
2. Advance weather timelines, forecast certainty timers, and extreme-event phase state.
3. Resolve completed player, contractor, and device actions for this step such as watering, clearing burial, repair completion, or planting completion.
4. Resolve local weather meters for each tile, especially `tileHeat`, `tileWind`, and `tileDust`, from current site weather plus current local support, shelter, terrain cover, and active device output.
5. Apply ongoing exposure and consumption to the worker, contractors, devices, and vulnerable stored resources.
6. Apply hazard pressure and environmental damage for this step, including erosion, plant density loss, burial gain, device damage, camp durability loss, and resource loss.
7. Apply recovery and beneficial change for this step, including plant density gain, plant constant-wither loss, salinity reduction, soil-fertility improvement, moisture recovery from watering or irrigation, moisture-loss reduction from protective effects, player recovery, and site stabilization gains, if current conditions allow.
8. Recompute threshold-derived cleanup states such as death or restored pocket status.
9. Run slower pulse checks whose timers have elapsed, including natural spread attempts, output pulses, task trigger checks, and other low-frequency site logic.

This order matters:

- completed actions should be allowed to protect a tile before hazard damage for that step if they finished in time
- resolved local weather should be known before hazard damage, terrain updates, and plant growth are evaluated
- hazard pressure should be applied before growth and recovery so dangerous conditions still feel dangerous
- growth should happen after support and recovery are known, not before

#### Resolution Timing Rules

Use these timing rules for consistency:

- terrain soil meter changes resolve continuously every fixed simulation step
- hazard damage resolves continuously every fixed simulation step
- player recovery and drain resolve continuously every fixed simulation step
- plant density gain and loss resolve continuously every fixed simulation step
- `constantWitherRate` loss resolves continuously every fixed simulation step
- natural spread resolves discretely on an `ecologyPulse`

Recommended pulse values:

- `ecologyPulseMinutes = 10`
- every `10` in-game minutes, evaluate natural spread, stable-pocket checks, and other low-frequency ecological logic

This means:

- growth feels continuous
- erosion and moisture loss feel continuous
- hazards feel continuous
- spread remains readable and discrete instead of noisy every frame

### Tile And Occupancy Rules

The game should use one shared tile contract for simulation, placement, save data, overlays, and basic reachability checks.

#### Tile State Categories

The tile model should separate different kinds of state instead of treating every value as generic "tile state."

| State Type | What It Represents | Fields | Notes |
|---|---|---|---|
| `terrainDefinition` | Base terrain identity and hard placement flags | `terrainTypeId`, `tileTraversable`, `tilePlantable` | Static `Ground` or `Rock` role |
| `terrainSoilState` | Persistent quality of plantable ground | `tileSoilFertility`, `tileMoisture`, `tileSoilSalinity` | Only meaningful on plantable `Ground` |
| `occupancyState` | What currently occupies the tile | `plantTypeId`, `groundCoverTypeId`, `structureTypeId` | Answers only what is present, not the state of that occupant |
| `plantState` | Runtime state of the current living plant or `Straw Checkerboard` cover | `tilePlantDensity`, `growthPressure` | `Plant Trend` is derived for display rather than stored as separate plant state; `Straw Checkerboard` uses the same density meter and shared plant contribution model, but with `growable = false` and a positive `constantWitherRate` |
| `structureState` | Runtime state of the current structure | `deviceIntegrity`, `deviceEfficiency`, `deviceStoredWater` | `deviceStoredWater` is mainly meaningful for water-storage structures |
| `resolvedLocalWeatherState` | Final local weather intensity after site weather, local support, and shelter effects are combined | `tileHeat`, `tileWind`, `tileDust` | Bridging layer between weather/support logic and final terrain or plant changes |
| `temporaryTileState` | Temporary tile-only hazard state | `tileSandBurial` | Burial is the only true temporary tile state in the current design |

Important cleanup rule:

- `occupancyState` should only answer what is on the tile; density, integrity, and efficiency belong to their own runtime states, while `Plant Trend` is a derived state
- `Straw Checkerboard` uses `groundCoverTypeId` for occupancy identity, reuses plant trait fields for its effects, and uses `tilePlantDensity` with `growable = false` plus a positive `constantWitherRate`
- `tileHeat`, `tileWind`, and `tileDust` are resolved local weather meters, not persistent terrain state
- the game should not use a separate `tileSoilStability` meter

#### Tile Layers

Each tile should conceptually contain these layers:

| Layer | Max Occupancy | Purpose | Examples |
|---|---|---|---|
| `terrainLayer` | `1` | Base terrain definition and core flags | `Ground`, `Rock` |
| `groundCoverLayer` | `1` | Surface-cover type or material that prepares or protects land beneath living plants | `Straw Checkerboard` |
| `livingPlantLayer` | `1` plant type | Main living plant type occupying the tile; density measures how much of that type fills the tile | Any living plant from the current set |
| `structureLayer` | `1` footprint reservation per tile | Player-built support devices and camp structures | Shelter, irrigation device, sensor, workshop, solar utility |
| `dynamicOverlayLayer` | many transient flags | Temporary state, not persistent occupancy identity | Burial |

For simplicity:

- a tile may contain both `groundCoverLayer` and `livingPlantLayer`
- a tile may not contain both `groundCoverLayer` and `structureLayer`
- a tile may contain both `livingPlantLayer` and `structureLayer` only if the placed structure explicitly supports plant sharing and the plant's height class is allowed by that structure
- dynamic overlays do not count as occupancy conflicts

This means `Straw Checkerboard` can prepare a tile and later coexist with one living plant type, while some structures may also coexist with certain living plants if their sharing rule allows it.

Clarification:

- `groundCoverLayer` means non-living or plant-derived surface cover that sits under later living plants and helps prepare land
- `device` means a player-built support utility or camp structure, not a plant and not a consumable item
- task markers, repair markers, reservation flags, and similar workflow indicators should belong to their own systems, not to tile runtime state

#### Terrain Flags

Each tile should expose two core placement and movement flags:

- `tileTraversable`
- `tilePlantable`

Buildability rule:

- if a tile is `tileTraversable = true`, it is buildable in the current design unless another occupancy or content rule blocks it

Recommended interpretation:

| Terrain | Traversable | Plantable | Role | Notes |
|---|---|---|---|---|
| `Ground` | Yes | Yes | Main playable terrain | Tile quality should come from soil meters such as `tileSoilFertility`, `tileMoisture`, and `tileSoilSalinity`, plus overlay pressure such as `tileSandBurial` and nearby resolved local weather |
| `Rock` | No | No | Hard blocker terrain | Cannot grow plants or host structures, but should be able to provide local shade and wind protection to nearby `Ground` tiles if that support rule is used |

Engineering can later expand terrain types, but the game should keep the base terrain layer simple.

Most meaningful ground variation should come from tile meters rather than many terrain IDs. Base terrain should answer "is this playable ground or a blocker?" while runtime meters answer "how good is this tile right now?"

#### Persistent Soil Meters

Every plantable `Ground` tile should carry these three soil meters:

- `tileSoilFertility` in range `0-100`
- `tileMoisture` in range `0-100`
- `tileSoilSalinity` in range `0-100`

Interpretation:

- `tileSoilFertility` is the long-lived land-improvement meter; it raises plant growth speed, sets the tile's maximum moisture capacity, and slows moisture reduction
- `tileMoisture` is the short-lived water-availability meter; it speeds growth when sufficient and slows or reverses it when too low
- `tileSoilSalinity` is the rehabilitation meter; it should influence which plants can thrive on the tile by capping density, not by acting as a hidden death timer

Important rules:

- these soil meters belong to plantable `Ground`, not to `Rock`
- plants, watering, devices, decay, and hazards may modify these meters over time
- fertility and salinity are persistent land-quality values, while moisture is the most volatile soil meter
- wind and sand should express erosion through exposure and burial, not through a separate stability meter

#### Device Footprints

Devices should support only this footprint class:

- `1x1`

Rules:

- the single target tile must pass placement validation
- the target tile becomes `tileReservedByStructure = true`
- rotation can still exist for visuals or directional effects, but not for footprint size
- if a device is removed, its reserved tile is released

Long defensive lines should come from repeated `1x1` placement or plant lines, not special long structure footprints in the first playable.

#### Occupancy Coexistence Rules

Use these hard rules:

- one living plant type per tile maximum
- one ground-cover type or material per tile maximum
- one structure-footprint reservation per tile maximum
- living plants can be planted onto tiles that already have `Straw Checkerboard`
- structures cannot be placed onto tiles already containing `Straw Checkerboard`
- `Straw Checkerboard` cannot be placed onto tiles already reserved by a structure
- a structure may share a tile with one living plant only if that structure's `plantShareRule` allows it
- if a structure allows plant sharing, the tile's living plant must have a `plantHeightClass` at or below the structure's `sharedPlantHeightLimit`
- if a structure does not allow plant sharing, placing it still requires the tile to contain no living plant
- if a living plant would grow or be replaced into a height class above the placed structure's allowed sharing limit, that tile becomes an invalid target for that plant until the structure is removed or the plant is changed

This keeps the system readable and prevents hidden overlap logic.

#### Movement And Pathing Rules

The player moves in continuous world space, but tile flags should still control collision, valid reachability, and action legality.

Movement rules:

- impassable terrain blocks movement entirely
- solid structure footprints block movement entirely
- living plants do not hard-block movement
- dense plants and burial may apply movement slowdown, but not hard blocking, unless a future plant explicitly says otherwise
- pathing and reachability checks should use orthogonal grid neighbors first to stay aligned with adjacency logic

For simplicity, reachability only needs to answer:

- can the player physically stand on or reach this tile
- is the tile connected to the camp or current actor position through passable tiles

Full advanced NPC navigation is not required for v1 architecture.

#### Placement Validation Rules

Plant placement validates in this order:

1. Target tile is in bounds.
2. `tilePlantable = true`.
3. If the tile is reserved by a structure, that structure must allow plant sharing for this plant's `plantHeightClass`.
4. `livingPlantLayer` is empty.
5. Additional ecological success or failure is handled by the plant systems, not by hidden placement rejection, except where a specific content rule says otherwise.

Ground-cover placement validates in this order:

1. Target tile is in bounds.
2. `tilePlantable = true`.
3. Tile is not reserved by a structure.
4. `groundCoverLayer` is empty.

Structure placement validates in this order:

1. Target tile is in bounds.
2. Target tile has `tileTraversable = true`.
3. Target tile is not reserved by another structure.
4. If the target tile contains a living plant, the structure's `plantShareRule` must allow that plant's `plantHeightClass`.
5. Target tile contains no ground cover.

Occupancy validation should happen before any cost is paid or any action timer begins.

#### Tile State Priorities

When multiple systems want to affect the same tile, resolve them in this conceptual order:

1. Terrain flags define what is fundamentally possible.
2. Structure occupancy defines reservation and hard blocking.
3. Ground cover and living plants define ecological effects.
4. Dynamic overlays modify the current moment without replacing the tile's core identity.

This keeps tile identity stable in saves and easier to reason about in code.

## 9. Survival Systems

### Player Condition

The player has explicit, separate meters:

- Health
- Hydration
- Nourishment
- Energy Cap
- Energy
- Morale
- Work Efficiency

These are the core `Player Condition` values. They are always relevant, but not all decline at the same speed or for the same reasons.

Health is the slower physical-condition meter. It reflects injury, illness risk, and bodily wear from repeated harsh exposure or risky field work. Hydration is the most urgent meter in hot conditions. `Nourishment` is slower, but important for sustained efficiency and recovery. `Energy Cap` is the currently usable ceiling on `Energy`, mainly limited by thirst and hunger. Energy limits daily work output and pushes rest timing, but it should not be fully usable when `Energy Cap` is low. Morale reflects comfort, momentum, and psychological strain under isolation and repeated setbacks. `Work Efficiency` is the resolved worker-output meter that turns current condition and local exposure into action energy cost.

Current gameplay effect for `Health`:

- `Health` should drop from dangerous outdoor exposure, severe exhaustion, and risky harsh-event work
- low `Health` should reduce safe work capacity even if the player has recovered some `Energy`
- low `Health` should also reduce `Work Efficiency`, making physically demanding tasks cost more energy or feel less reliable
- low `Health` should slow recovery and make collapse or forced retreat more likely
- shelter, rest, and medicine should recover `Health` more slowly than `Energy`

Current gameplay effect for `Morale`:

- `Morale` should directly modify `Work Efficiency`
- when `Morale` is low, routine work should cost more energy or feel less reliable
- when `Morale` is stable or high, normal `Work Efficiency` is preserved
- for the current design, this is the minimum concrete `Morale` effect required; broader morale interactions can be added later if needed

Current gameplay effect for `Energy Cap`:

- `Hydration` and `Nourishment` should be the main meters that limit `Energy Cap`
- when `Energy Cap` is low, the worker may still own some stored `Energy`, but only a smaller usable portion should be available for action planning
- recovery of `Hydration` and `Nourishment` should restore `Energy Cap` faster than rebuilding fully spent `Energy`
- `Energy Cap` should make thirst and hunger feel immediately restrictive without forcing direct collapse of every other worker meter

### Dense Cover Recovery

Dense restored plant cover should become a practical recovery aid, not only a visual reward.

When the player is standing or resting inside a local planted pocket where most nearby tiles are medium-density or high-density, the area should feel safer and more livable:

- `Energy` recovers faster during short recovery pauses or rest
- `Morale` recovers faster because the player feels protected and sees clear proof of progress
- the area feels less psychologically harsh even if the wider site boundary is still hostile

This matters mechanically because better `Morale` helps the player keep normal `Work Efficiency` instead of paying extra energy for routine actions or suffering less reliable routine actions.

This is an important emotional payoff. The player should feel they have created a small survivable paradise inside a dangerous desert, even before the whole `Site` is restored.

### Environmental Readouts

The environment is represented through visible site conditions rather than hidden formulas:

- Heat
- Wind
- Dust

These are not just flavor. They directly affect action energy cost, exposure risk, plant survival, erosion, and device performance.

### Environmental Pressure Suite

For validation, the environment should constantly push against the player's layout. Plants should not behave like passive timers. A planted area should only prosper if the player chose the right plant mix, support, and timing for that zone.

| Pressure | Strategic Purpose | Main Side Effects |
|---|---|---|
| Heat stress | Forces timing, hydration planning, and anti-dehydration planting | Faster player hydration loss, higher outdoor action energy cost, increased plant water demand, weaker seedling establishment |
| Wind abrasion | Rewards windbreaks and exposed-lane planning | Faster density loss or slower growth on exposed tiles, reduced young-plant survival, stronger erosion pressure, higher outdoor action energy cost |
| Sand burial | Creates reactive rescue decisions and maintenance work | Plants and devices can become partially buried, local support weakens, irrigation efficiency drops, movement through the zone slows until cleared |
| Constant erosion | Prevents idle passive growth and punishes weak layouts over time | Exposed tiles lose fertility over time, young plants can regress, dead tiles become harder to re-use, neighboring tiles become more vulnerable |

These pressures should interact. A strong layout can absorb one or two bad conditions. A weak layout should start to unravel when multiple pressures stack.

### Daily Rhythm

The game runs in real time with a day cycle. Daily planning matters because:

- Midday heat is more dangerous than early morning or evening
- Some work is better done before storms arrive
- Recovery requires food, water, and sleep
- Player energy is limited each day

### Weather Forecasting

Weather is forecastable, but not perfectly. Early forecasts are broad and uncertain. Tech upgrades improve:

- Forecast range
- Event confidence
- Intensity estimates
- Site-specific precision

Forecasting should turn weather into a planning skill, not a random punishment.

Weather should also be a major source of replayability. Each campaign and `Site` should produce different combinations of:

- Heat-wave frequency
- Sandstorm timing
- Wind intensity
- Calm windows for efficient work
- Forecast reliability

This ensures that even similar loadouts or tech plans create different stories across runs.

Player-facing forecast channel rule:

- the forecast should present three clear site-wide weather channels:
- `Heat`
- `Wind`
- `Dust`
- these should be shown as readable meters, bars, or severity bands rather than hidden numbers once the relevant university forecast upgrades are unlocked

### Hazard And Weather Runtime Model

The game should treat weather as one shared site-wide runtime system that constantly updates `weatherHeat`, `weatherWind`, and `weatherDust`, while rarer extreme events run through their own event-side meter layer on top.

#### Weather Runtime Fields

Use these runtime fields:

| Field | Type | Meaning |
|---|---|---|
| `weatherHeat` | Continuous `0-100` | Current site-wide heat pressure |
| `weatherWind` | Continuous `0-100` | Current site-wide wind pressure |
| `weatherDust` | Continuous `0-100` | Current site-wide airborne sand, dust-load, and visibility pressure; shown to the player as `Dust` |
| `siteHeatBias` | Continuous `-20` to `20` | Site-specific baseline heat tendency |
| `siteWindBias` | Continuous `-20` to `20` | Site-specific baseline wind tendency |
| `siteDustBias` | Continuous `-20` to `20` | Site-specific baseline loose-dust tendency |
| `forecastConfidence` | Continuous `0-100` | How trustworthy the current forecast is |
| `forecastLeadMinutes` | Continuous | How much warning time the forecast currently gives |
| `eventTypeId` | Discrete | Current extreme event archetype or `None` |
| `eventState` | Discrete | `Inactive`, `Warning`, `Build`, `Peak`, `Decay`, or `Aftermath` |
| `eventTier` | Discrete | Relative severity band for the current event |
| `eventHeatPressure` | Continuous `0-100` | Current event-side heat pressure that feeds `weatherHeat` |
| `eventWindPressure` | Continuous `0-100` | Current event-side wind pressure that feeds `weatherWind` |
| `eventDustPressure` | Continuous `0-100` | Current event-side dust pressure that feeds `weatherDust` |

Core rule:

- `weatherHeat`, `weatherWind`, and `weatherDust` are site-wide ambient values
- `eventHeatPressure`, `eventWindPressure`, and `eventDustPressure` are site-wide event meters that can sit at `0` when no harsh event is active
- local terrain, plants, devices, and player field work do not rewrite the weather itself
- they change how hard the weather lands on a given tile, worker, device, or storage area

#### Resolved Local Weather Fields

These are per-tile runtime meters, not site-wide weather replacements. They are the local resolved weather result after site weather is filtered through current local support, cover, shelter, and terrain shape.

| Field | Type | Meaning |
|---|---|---|
| `tileHeat` | Continuous `0-100` | Final local heat pressure on one tile after site heat and local mitigation are combined |
| `tileWind` | Continuous `0-100` | Final local wind exposure on one tile after site wind and local protection are combined |
| `tileDust` | Continuous `0-100` | Final local sand and dust exposure on one tile after site sand, dust, and local mitigation are combined |

Resolution rule:

- site weather stays global
- local plants, structures, cover, and terrain shelter do not change the global weather values
- instead, they directly change the resolved local results `tileHeat`, `tileWind`, and `tileDust`
- those resolved local weather meters then drive tile moisture loss, burial, fertility setback, and the weather-linked parts of plant pressure

#### Baseline Daily Weather Curve

Outside of extreme events, weather should still follow a readable daily rhythm.

Baseline behavior:

- `weatherHeat` should be lowest overnight, rise through morning, peak around midday, then fall in evening
- `weatherWind` should fluctuate more than heat and can spike during exposed parts of the day even without a named event
- `weatherDust` should be partly derived from wind, site dust bias, and recent hazard aftermath

Use this relationship:

- `weatherHeat` should come from the daily heat curve plus site bias plus current `eventHeatPressure`
- `weatherWind` should come from the daily wind curve plus site bias plus gust variation plus current `eventWindPressure`
- `weatherDust` should come from site dust bias, current wind, current `eventDustPressure`, and recent-event aftermath

Engineering note:

- `dayHeatCurve` and `dayWindCurve` can be authored as simple curves rather than hand-written formulas
- `gustNoise` should be low-amplitude short variation, not full randomness spikes with no warning

#### Extreme Event Archetypes

The current design only needs a small event set:

| Event | Typical Event-Meter Pattern | Main Gameplay Pressure |
|---|---|---|
| `HeatWave` | high `eventHeatPressure`, small `eventWindPressure`, small `eventDustPressure` | hydration drain, energy drain, exposed worker pressure, weak seedling survival |
| `Sandstorm` | medium `eventHeatPressure`, high `eventWindPressure`, very high `eventDustPressure` | visibility collapse, extreme wind-sand exposure, burial, device damage, movement pressure |
| `CompoundFront` | high values in all three channels | rare high-tier event that tests the full support layout |

Limits:

- during the first three onboarding sites, a normal site day should have `0` or `1` major extreme event most of the time
- back-to-back extreme events or authored wave sequences should be rare and mainly reserved for the fourth-site proof case or later high-tier campaign pressure
- `CompoundFront` can be used as a capstone late-wave pressure spike if needed, but it should remain uncommon enough that it feels memorable rather than routine

#### Event Phase Contract

Every extreme event should move through the same phase model:

| `eventState` | Purpose | Behavior |
|---|---|---|
| `Inactive` | No active extreme event | Weather comes only from baseline curve and minor gust noise |
| `Warning` | Forecasted approach window | No large direct modifier yet; player receives warning and prep time |
| `Build` | Pressure begins rising | Work becomes riskier, visibility may start dropping, and exposed zones start taking meaningful pressure |
| `Peak` | Main danger window | Strongest modifiers, strongest burial or hydration punishment, and the most dangerous field-work window |
| `Decay` | Event is weakening | Still dangerous, but the player can begin broader recovery actions |
| `Aftermath` | Lingering site disruption | Lower ambient pressure than peak, but burial, damage, morale hit, and repair backlog remain |

Duration targets:

- `Warning`: `60-180` in-game minutes
- `Build`: `30-90` in-game minutes
- `Peak`: `30-120` in-game minutes
- `Decay`: `30-90` in-game minutes
- `Aftermath`: `60-240` in-game minutes

#### Forecast Contract

Forecasting should be imperfect, but actionable.

Forecast output should include:

- predicted event type if one is likely
- predicted start window
- predicted intensity band for `Heat`, `Wind`, and `Dust`
- `forecastConfidence`

Base forecast values:

- `forecastLeadMinutes = 180`
- base intensity error band = `+/- 15`
- base start-time error band = `+/- 60` in-game minutes
- base `forecastConfidence` should usually sit around `55-70`

Improvement sources:

- `Weather Mast`
- persistent tech that improves forecast range or certainty
- site-scoped unlocks or modifiers that improve local site precision

Improvement effects:

- higher `forecastLeadMinutes`
- smaller intensity error band
- smaller start-time error band
- higher `forecastConfidence`
- earlier access to more precise channel-specific forecast information
- clearer identification of which weather channel is expected to be most dangerous

Important rule:

- forecasting should improve planning quality, not fully remove uncertainty
- even strong forecasting should still preserve some tension in exact intensity, duration, or zone impact

#### Runtime Pressure And Aftermath Rules

Weather should pressure more than one system at once.

Use this interpretation:

- `weatherHeat` mainly raises `tileHeat`; `tileHeat` then drives worker hydration loss, lower worker `playerWorkEfficiency`, severe exposure health risk, tile moisture loss, and the heat-linked share of plant `growthPressure`
- `weatherWind` mainly raises `tileWind`; `tileWind` then drives harder outdoor work, lower worker `playerWorkEfficiency`, exposed-tile erosion pressure, moisture loss, and the wind-linked share of plant `growthPressure`
- `weatherDust` mainly raises `tileDust`; `tileDust` then drives worker health risk, lower worker `playerWorkEfficiency`, burial, soil setback, the dust-linked share of plant `growthPressure`, and device damage risk

Design consistency rule:

- each weather channel should leave readable artifacts on at least 3 layers at once when relevant: player pressure, land/tile condition, and plant growth or degeneration

When `eventState` is `Peak`, the event should mainly push the weather system itself into extreme values rather than bypassing it with separate direct plant-damage rules.

During `Peak`, the event should mainly raise:

- `eventHeatPressure`
- `eventWindPressure`
- `eventDustPressure`
- worker outdoor action speed penalties
- worker hydration and energy drain
- worker health risk from severe exposure
- device damage pressure

Those extreme weather values should then be what actually causes:

- high plant `growthPressure`
- plant density loss on under-protected tiles
- `tileSandBurial` gain
- `deviceEfficiency` loss

When `eventState` enters `Aftermath`, the event should stop dealing peak pressure, but it should leave behind recoverable problems:

- extra `tileSandBurial`
- reduced `deviceEfficiency` from burial or damage
- possible camp storage disruption if the camp was exposed
- lower short-term worker morale until the site feels under control again
- possible `Aftermath Relief Offer`s from high-reputation factions, giving the player a recovery choice rather than automatic rescue

This phase is important because the strategy game continues after the spectacle ends. Strong reputation should improve relief speed, package quality, and subsidy level, but it should never erase the event's damage for free.

### Extreme Hazard Events

In addition to normal heat, wind, and dust pressure, some sites should experience extreme hazard events that make the environment temporarily fierce and genuinely threatening. These are not routine background conditions. They are short periods where survival, shelter, preparation, and site layout are put under serious stress.

Example extreme event directions:

- Severe sandstorm front
- Extended heat-wave spike
- Multi-phase wind burst event
- Combined heat and visibility-collapse event

Design goals for extreme events:

- Raise tension and uncertainty without feeling arbitrary
- Force the player to rely on the systems they have built
- Make the player feel small against the environment for a limited time
- Create memorable "I barely survived that" stories
- Produce strong emotional payoff after the event passes

These events should be harsh enough that failure feels possible. The excitement comes from facing the unknown result, protecting what was built, and seeing whether the site can endure. The recovery after surviving an extreme event should feel like winning a battle against the desert, even though the game has no combat.

Extreme events should be forecasted imperfectly, with enough warning to prepare but not enough certainty to remove tension completely.

Extreme events should also be interactive. The player should not only wait them out. During the event, they should be pushed into hard survival choices such as:

- Which zone to protect first
- Whether to sacrifice one exposed area to save another
- Whether to spend emergency money or resources immediately
- Whether to risk going outside for a critical action or stay sheltered and lose ground elsewhere

This creates tension plus agency, which is far more exciting than passive damage intake.

Extreme events should also act as the main stress test for plant strategy. A low-pressure, well-protected patch should come out damaged but alive. A greedy or badly matched patch should risk rapid degeneration, withering, or outright death.

Extreme hazards should not punish every layout equally. The game should clearly reward smart placement and layered protection.

Each threatened patch should effectively compare its resolved `tileHeat`, `tileWind`, and `tileDust` against current `tileMoisture`, `tileSoilFertility`, and plant density.

Main sources of local protection:

- correct support-plant adjacency
- terrain shelter or favorable terrain shape
- nearby support devices such as irrigation, shelter, shade, or windbreaks
- strong own-tile density and ground cover on the threatened patch

Hazard state-impact bands:

- these bands should describe how hazard pressure changes tile, plant, and structure state first; visible survival or collapse should emerge from those state changes rather than from separate scripted outcome rules
- low local protection: peak weather should drive strong `tileSandBurial` gain, faster `tileMoisture` loss, and clear erosion-driven `tileSoilFertility` setback; plants on the patch should see `growthPressure` rise quickly from wind and dust exposure, heat stress, and worsening soil state; exposed structures should lose `deviceEfficiency` rapidly and may begin losing `deviceIntegrity`; this combination should usually derive a `Withering` trend label, cut `tilePlantDensity` sharply, and create a major recovery burden
- moderate local protection: peak weather should still add some `tileSandBurial`, moisture loss, and limited fertility setback, but not at immediate-collapse speed; `growthPressure` should spike during the worst window and cause noticeable but recoverable `tilePlantDensity` loss; structures may suffer partial `deviceEfficiency` loss or light damage while remaining usable; the patch should stay functional enough to rescue if the player responds in time
- high local protection: peak weather should cause only light `tileSandBurial`, controlled moisture loss, and little or no lasting fertility setback; `growthPressure` should remain manageable enough that `tilePlantDensity` mostly holds, with the derived trend label often returning from `Holding` to `Growing` after the peak passes; structures should need only light clearing or repair, with limited `deviceEfficiency` loss; the visible result should be a resilient patch rather than a reset

This is a key reward loop. If the player reads the site correctly and builds protection with strategy, extreme weather should still feel dangerous, but it should no longer feel equally destructive. Good planning should convert a disaster into a manageable setback.

Extreme events should also hit the site's economy and recovery capacity. They should threaten not only what is planted right now, but also the player's ability to bounce back afterward.

Typical severe-event consequences:

- exposed plant tiles lose `Plant Density`, causing weaker output and weaker protection even before full death
- high-value crops can lose part or all of their current yield window
- stored water, food, repair kits, or other site supplies may be consumed rapidly, buried, spoiled, or damaged
- irrigation, shelter, sensors, or solar utilities may lose efficiency or stop functioning until repaired
- the worker's `Player Condition` can drop sharply, demanding more food, water, time, and shelter to recover
- multiple weakened tiles can create a cascading exposure problem for nearby tiles

This is important because it creates the real downward spiral:

- hazard hits
- `Plant Density` falls and some plants wither or die
- output and tile protection drop
- stored supplies and player condition worsen
- recovery costs rise while income falls
- the player must choose a comeback plan instead of doing everything at once

The recovery phase after a major event should be a strategy phase of its own. The player should often need to decide which problem matters most right now:

- restore the core protective patch first
- save the remaining output line before it fully collapses
- recover water and camp function
- stabilize the worker so useful work can continue

The game should reward correct triage. Coming back from a bad event should feel like solving a hard strategic knot, not only paying a repair tax.

### Harsh-Event Work Under Pressure

The game should not use a separate `Emergency Field Action` system. Instead, harsh events should make ordinary field work become dangerous, exhausting, and strategically expensive.

This means the player still uses normal verbs:

- clear burial from a plant tile, route, or device footprint
- repair a damaged irrigator, pump, or support device
- water a struggling patch
- move supplies, reconnect access, or retreat to shelter

The tension should come from conditions, not from a special rescue menu.

During severe hazards, the player should often be able to save only one or two of the following:

- the seedling line
- the irrigation path
- the output patch
- the camp edge

They should usually not be able to save everything.

The design target is still "I saved that patch," but the reason should be that the player chose the right ordinary job to risk under terrible conditions.

### Harsh-Event Work Rules

The game should formalize this as normal player actions performed under event-modified pressure during `Warning`, `Build`, `Peak`, `Decay`, and `Aftermath`, rather than as separate action types.

Core rules:

- no dedicated emergency-only action menu is required
- no temporary emergency-only structures or covers are required
- the player should still be able to perform normal field work if the target is reachable and they are physically capable of acting
- only the player performs this urgent manual work in the current design
- only one manual action can be performed at a time
- if the player cancels an action before completion, no main work effect is applied

Exposure rules:

- during `Build`, outdoor action time is multiplied by `1.15`
- during `Peak`, outdoor action time is multiplied by `1.35`
- during `Peak`, hydration and energy drain from the current weather continue on top of the action's direct work cost
- if visibility is critically low during a `Sandstorm`, movement to the target should often be the real cost, not only the work itself
- if the worker is already in critical survival danger, action start should be blocked until they recover enough to act

Interruption rules:

- being forced to shelter, collapsing from `Player Condition`, or manually canceling should interrupt the action
- interrupted actions apply no completion effect
- item costs should be spent only on successful completion unless the normal base action already consumes the resource at action start

Design intent:

- save the seedlings or save the irrigator
- rescue the fringe output tile or protect the camp edge
- go outside during `Peak` for a risky fix or accept a smaller loss and recover faster after

That tradeoff is the core feeling. Harsh events should create agency through dangerous normal work, not through a parallel emergency-action subsystem.

## 10. Site Management

### Player Base

Each `Site` includes a player base camp rather than an abstract support camp or a full colony. The base is the player's anchored safe core on the site: the place where they shelter, store harvest and water, place camp-side crafting or utility devices, receive deliveries, and recover between field pushes.

Core base functions:

- Shelter and recovery for the player
- Storage for harvest, water, seeds, parts, and other carried supplies
- Placement space for camp-side crafting, repair, and utility devices
- Delivery intake for phone-ordered supplies and hires
- Limited contractor coordination
- Strong local wind, heat, and dust protection compared with open field tiles

Base durability rule:

- the base should have `campDurability` in the `0-100` range
- the base should provide a strong local protection pocket by default, but its effective wind, heat, dust, and recovery protection should scale down as `campDurability` falls
- normal harsh conditions should chip away at exposed `campDurability` over time, while major hazard events should remove it much faster
- nearby protection plants, favorable terrain shape, and dedicated shelter or protection structures should reduce or fully offset base durability loss if the camp edge is well sheltered
- if `campDurability` falls too far, camp-side recovery, storage safety, and nearby utility reliability should all weaken until the base is repaired or re-protected

The base should feel expandable, but it is not a city builder in v1.

### Site Completion And Campaign Time

For the current design, site completion should avoid hard-to-read abstract site scores. Instead, completion should be determined by a simple count of `Fully Grown Tile`s on the current `Site`.

The core completion rule is:

- each site tracks `fullyGrownTileCount`
- each site defines a `siteCompletionTileThreshold`
- if `fullyGrownTileCount >= siteCompletionTileThreshold`, the site is considered completed

Water access, shelter, devices, and other systems still matter because they help the player survive long enough to grow and protect plants, but they should not be turned into separate opaque completion scores.

The campaign-level pressure should come from time:

- the campaign tracks `campaignDaysRemaining`
- the player is trying to complete as many sites as possible before the `Campaign Clock` expires
- this means each site session naturally pushes the player toward finishing efficiently rather than lingering for optional optimization

### Resource And Inventory Model

The game should separate economy and progression data into three layers:

1. Explicit completion and campaign-time counters
2. Exact itemized resources for inventory and gameplay actions
3. Inventory locations that define where those resources physically exist

#### Layer 1: Completion And Campaign Counters

These values are used for task generation and high-level site-state checks:

| Counter | Runtime Field | Type | Meaning |
|---|---|---|---|
| Fully Grown Tiles | `fullyGrownTileCount` | Integer | Number of mature planted tiles currently counted toward site completion |
| Completion Threshold | `siteCompletionTileThreshold` | Integer | Number of fully grown tiles required for the site to count as completed |
| Funds | `money` | Integer | Task rewards, sales, purchases, and hiring |
| Campaign Days Remaining | `campaignDaysRemaining` | Integer | Remaining campaign time before the run ends |

Important rule:

- `fullyGrownTileCount` and `siteCompletionTileThreshold` should be explicit counters, not hidden derived vibes
- `money` is both a summary value and the exact site-run currency
- `campaignDaysRemaining` is the main long-horizon pressure value in the campaign

#### Layer 2: Exact Itemized Resources

These are the exact `Item` categories that can be stored, moved, consumed, bought, sold, or damaged.

| Resource | Runtime Identity | Stack Size | Main Uses |
|---|---|---|---|
| Water Container | `waterContainer` | `5` | Hydration and watering |
| Food Pack | `foodPack` | `5` | `Nourishment` recovery, recovery prep |
| Medicine Pack | `medicinePack` | `3` | Recovery support after harsh conditions |
| Repair Kit | `repairKit` | `3` | Structure and device repair |
| Parts Bundle | `partsBundle` | `10` | Device construction and some repairs |
| Seed Bundle | `seedBundle:<plantTypeId>` | `10` | Plant placement for a specific plant type |
| Harvest Good | `harvestGood:<outputTypeId>` | `10` | Sale or task delivery |

Note:

- tools and portable devices should also be treated as `Item`s, usually as single-slot or low-stack entries when they are carriable
- stack size is a real carrying constraint, not just a presentation label
- if the player wants to carry more than one stack limit of the same `Item`, the excess must occupy additional `Inventory` slots
- this should be especially visible for seeds, water containers, harvested goods, and other common stackable resources
- `Reputation` is not an inventory item and never exists in storage

#### Layer 3: Inventory And Containers

The game should support these carried-storage and camp-storage locations:

| Location | Runtime Identity | Capacity | Purpose |
|---|---|---|---|
| Carried Inventory | `workerPack` | `6` slots | What the player carries into the field |
| Camp Containers | `campStorage` | `24` total slots initially | Main local stockpile for site `Item`s, represented by one or more built or bought `Container`s |
| Pending Delivery Queue | `pendingDeliveryQueue` | unbounded list of packages | Ordered supplies waiting to arrive at camp |

Rules:

- a slot holds one stack of one resource type
- one slot cannot exceed that `Item`'s listed stack size
- stack splitting and merging should be supported
- empty slots are explicit
- future tech can expand `workerPackSlotCount` or `campStorageSlotCount`, but the game should start at `6` and `24`
- `campStorageSlotCount` should represent the total capacity provided by current camp `Container`s
- the player should only be able to carry a limited working set away from camp, even if the camp has much more storage available
- carrying a lot of one resource type should therefore compete directly with carrying tools, seeds, devices, medicine, or repair materials

Example inventory pressure:

- `7` `waterContainer` requires `2` slots because one slot can hold only `5`
- `16` `seedBundle:windReed` requires `2` slots because one slot can hold only `10`
- `21` `harvestGood:fiber` requires `3` slots because each slot can hold only `10`

Container rules:

- `Container`s should be buyable or buildable camp objects in the current design
- interacting with any `Container` should access the shared `campStorage` rather than only that one object's personal slots
- losing, burying, or exposing camp `Container`s can put stored `Item`s at risk during harsh events

#### Resource Flow Rules

Use these resource-flow rules:

- phone purchases create a package in `pendingDeliveryQueue`
- each purchased package should arrive at camp after `30` in-game minutes if the camp delivery point is operational
- delivered items enter `campStorage`, not `workerPack`
- the player can transfer items between `campStorage` and `workerPack` only while interacting with a `Container` or another explicitly valid storage access point
- plant placement, repair work, and burial clearing consume from `workerPack` when they require carried items
- structure construction can consume from `workerPack`
- harvesting places goods into `workerPack` first
- selling through the `Field Phone` should require goods to be present in `campStorage`, not only in `workerPack`
- camp-side crafting devices should be allowed to consume required materials directly from shared `campStorage`
- there should be no distance-cost rule between `Container`s and camp crafting devices, because both are restricted to the player base area
- `Water Container`s can be transferred from `workerPack` or `campStorage` into a `Water Tank` through a valid water transfer interaction, increasing that tank's `deviceStoredWater`
- connected `Drip Irrigator`s consume `deviceStoredWater` from linked `Water Tank`s rather than directly consuming water from `workerPack` or `campStorage`

This keeps the carried `Inventory` important without forcing a full hauling sim.

#### Hazard Interaction Rules For Resources

To keep the logic readable, hazards should interact with resources by location:

Worker-carried resources:

- are not randomly destroyed just by bad weather in normal play
- are consumed by direct use, recovery, construction, planting, repair, or other field-work cost
- can be lost only through explicit failure or a specifically authored event, not through hidden attrition rules

Camp-stored resources:

- can be buried, damaged, spoiled, or partially lost during severe hazard events if the camp is exposed
- are better protected when the camp has stronger shelter structures and nearby protection
- should be vulnerable by category in readable ways:

Vulnerable categories:

- `waterContainer`
- `foodPack`
- `medicinePack`
- `repairKit`
- `partsBundle`
- `seedBundle:*`
- `harvestGood:*`

Never hazard-destroy:

- `money`
- `reputation`
- persistent-tech progress
- transient unlock state that has already been chosen

#### Completion / Inventory Relationship

For the current architecture, use these relationships:

- `fullyGrownTileCount` is the explicit completion value that matters for finishing a site
- inventory, shelter, water, and devices are separate support systems; they do not change completion directly
- those support systems matter because they change what actions the player can perform, how safely the player can perform them, and how well planted tiles can survive and mature over time
- inventory changes should only affect practical action capacity such as carrying, planting, building, repair, watering, or harvest flow

This keeps completion bookkeeping readable and concrete.

### Contractors

Contractors are optional paid help, not a permanent workforce. In the current design, hiring contractors should create a paid contractor work pool rather than a fragile per-call estimate. That work pool can be assigned to valid in-progress tasks such as planting, building, or hauling whenever the player wants.

This system keeps hired labor useful but simple:

- No deep AI scheduling in v1
- No persistent named colony crew
- Labor is a money-for-time tradeoff
- purchased contractor work should only be consumed by actual completed work, not by the player's imperfect prediction
- if the player assigns too much contractor help to a task, any unused remainder should stay in the current site's contractor work pool for later use
- if there is no valid work to do, contractor help should simply remain idle instead of being wasted
- the player's decision should be when to spend contractor work, not how perfectly to estimate the exact remaining labor on a task

### Power And Utilities

There is no fuel economy in v1. Solar-backed devices and purchased infrastructure support site operations. This keeps the focus on water, ecology, and weather rather than fossil fuel logistics.

Solar-economy rule:

- `Solar Array` output should first satisfy the player's current camp and device utility demand
- only operational surplus electricity after current local consumption should be sellable
- surplus electricity should be sold through the `Field Phone`, not through a separate terminal device
- surplus electricity should be treated as a site-summary export value, not as a carried inventory item
- exported electricity should be a low-value side income: it should sell for less than normal harvested goods and much less than crafted value-added goods such as fruit juice
- solar income should reward a well-maintained support base, but it should never outcompete the main plant, task, and crafting economy

### Device Roster

The game should use a small device set that supports the plant game, worker survival, forecasting, and recovery without turning the project into a full utility-network sim.

Important override:

- there is no buy or sell terminal device in the current design
- all buying, selling, hiring, and tech purchasing remain `Field Phone` interactions
- physical site devices exist to support survival, ecology, forecasting, storage, and repair work

#### Device Runtime Contract

Each structure definition should provide:

| Field | Meaning |
|---|---|
| `structureTypeId` | Unique runtime identity for the structure |
| `footprintClass` | `1x1` in the current design |
| `plantShareRule` | Whether the structure may share its footprint with a living plant: `None`, `LowOnly`, `UpToMedium`, or `AnyHeight` |
| `sharedPlantHeightLimit` | Highest allowed `plantHeightClass` on the same tile if sharing is enabled |
| `buildMoneyCost` | Exact integer money cost |
| `buildPartsCost` | Exact `partsBundle` cost |
| `buildTimeMinutes` | In-game minutes required to complete construction |
| `baseIntegrity` | Starting `deviceIntegrity` value |
| `burialSensitivity` | How quickly the device loses effectiveness from sand burial |
| `windProtectionPower` | Shared local wind-protection contribution if any |
| `heatProtectionPower` | Shared local heat-protection contribution if any |
| `recoveryAura` | Local worker-recovery contribution if any |
| `dustProtectionPower` | Shared local dust-protection contribution if any |
| `waterStorageCapacity` | Maximum stored water if the structure is a water-storage device |
| `waterUsePerMinute` | Stored-water consumption rate if the structure actively irrigates |
| `forecastSupport` | Forecast precision and warning support if any |
| `craftingSupport` | Whether this device unlocks camp crafting actions |

Rules:

- device effects should resolve through shared object contribution meters, adjacency, or simple site-summary contributions, not through full pipe or wire simulation
- duplicate devices of the same type should stack at `100%` for the first copy and `50%` for each additional overlapping copy
- a disabled device keeps occupying its footprint until repaired or removed
- `deviceIntegrity` tracks structural health, while `deviceEfficiency` tracks current functional output after burial, storm damage, or solar support
- if a structure allows plant sharing, the structure still occupies the tile for buildability and repair logic, but the allowed living plant may remain on that tile and continue resolving its own density and support effects
- a camp-side device with `craftingSupport` should be able to read required materials from shared `campStorage` without first moving them into `workerPack`
- `Water Tank` should use `deviceStoredWater` as a real runtime reserve that can be filled and drained during the site
- `Drip Irrigator` should only provide irrigation support while at least one linked `Water Tank` still has `deviceStoredWater > 0`
- in the current design, a tank link should be a simple logical connection rather than a pipe-building system
- one `Drip Irrigator` may link to multiple `Water Tank`s, and its water draw should be split as evenly as possible across all linked tanks that still contain water
- if all linked tanks are empty, the `Drip Irrigator` should remain placed but provide no irrigation effect until water is available again

#### Structure Set

| Structure | Family | Footprint | Plant Sharing | Build Cost (Temp) | Main Function | Failure Pressure |
|---|---|---|---|---|---|---|
| `Field Tent` | Shelter | `1x1` | `None` | `money 80`, `parts 2` | Creates a small safe rest point and improves short rest recovery on nearby tiles | Can be wind-damaged or buried; weak during severe storms without nearby protection |
| `Shade Canopy` | Shelter / Heat relief | `1x1` | `None` | `money 120`, `parts 3` | Creates a stronger heat-relief pocket for worker recovery and nearby fragile plants | Loses efficiency quickly in strong wind if exposed |
| `Water Tank` | Water support | `1x1` | `None` | `money 140`, `parts 3` | Stores site water for watering actions and for linked `Drip Irrigator`s | Can be buried, damaged, or emptied, making connected irrigation collapse until refilled |
| `Drip Irrigator` | Irrigation | `1x1` | `AnyHeight` | `money 180`, `parts 4` | Adds steady direct `tileMoisture` gain to nearby plant tiles while consuming water from one or more linked `Water Tank`s | Provides no irrigation if linked tanks are empty; efficiency also collapses when buried or damaged |
| `Wind Fence` | Protection utility | `1x1` | `None` | `money 100`, `parts 2` | Lowers local `tileWind` and `tileDust`, especially useful for exposed edges and storm-facing lanes | Provides little recovery value and degrades under repeated sand pressure |
| `Weather Mast` | Sensor | `1x1` | `None` | `money 160`, `parts 3` | Improves forecast precision, earlier warnings, and planning quality | Storm hits and burial can reduce forecast quality until repaired or cleared |
| `Solar Array` | Solar utility | `1x1` | `LowOnly` | `money 220`, `parts 5` | Improves nearby `deviceEfficiency`, supports stable utility output without fuel logistics, and can create small sellable surplus electricity when site demand is already covered | Storm damage and burial can sharply reduce output until repaired or cleared |
| `Field Workshop` | Workshop / Crafting | `1x1` | `None` | `money 200`, `parts 4` | Enables camp crafting actions using materials from shared `campStorage` | Crafting access depends on the workshop staying intact and reachable |

#### Structure Family Roles

Use these family expectations in tuning:

- shelter family devices should mainly improve worker safety, camp protection, and recovery speed
- irrigation devices should mainly improve water access, `tileMoisture`, and low-density plant survival, but only while connected stored water is available
- protection utilities should mainly reduce local `tileWind`, `tileDust`, `tileHeat`, or some combination of them, and reduce storm losses
- sensors should mainly improve forecast lead time and forecast precision
- workshops should mainly enable simple camp crafting and material conversion, not repair automation or deep production chains
- solar utilities should mainly improve nearby `deviceEfficiency` and optionally generate a small low-value export surplus, not create a separate power-minigame

Faction-gating note:

- `Drip Irrigator`, `Solar Array`, `Weather Mast`, and any `Checkerboard Deployment Machine` variant should primarily sit inside the `Autonomous Region Agricultural University` unlock path in the current design
- `Forestry Bureau of Autonomous Region` should still care deeply about water and plant survival, but in the current design it should express that through plant choices, ecology tasks, and support rewards rather than a competing device roster

Plant-sharing examples:

- `Solar Array` should be able to share with `Low` vegetation only
- `Drip Irrigator` should be able to share with any living plant height in the current design
- fully blocking structures such as `Water Tank`, `Weather Mast`, `Wind Fence`, and any future `Water Pump` should use `plantShareRule = None`

#### Device Effect Footprint Rules

Each device should use one of these simple effect models:

| Effect Model | Use Case | Rule |
|---|---|---|
| Own-tile only | Storage, workshop occupancy, local shelter anchor | Effect applies only to the device tile and direct interaction point |
| Orthogonal aura `1` | Small shelter or irrigation support | Device affects its own tile plus the 4 orthogonal neighboring tiles |
| Orthogonal aura `2` | Larger refuge or solar support | Device affects its own tile plus all tiles within Manhattan distance `2` |
| Site-summary only | Forecasting or site meter support | Device changes site-wide summary behavior without directly changing each tile |

Recommended mapping:

- `Field Tent`: orthogonal aura `1`
- `Shade Canopy`: orthogonal aura `1`
- `Water Tank`: own-tile stored-water source plus direct interaction point for refilling or transfer
- `Drip Irrigator`: orthogonal aura `1`, but only while linked tanks still contain water
- `Wind Fence`: orthogonal aura `1`, biased toward exposed perimeter placement
- `Weather Mast`: site-summary only
- `Solar Array`: orthogonal aura `2`
- `Field Workshop`: own-tile only

#### Device Design Intent

This roster should create a few clear early strategic patterns:

- build a small protected camp core before overextending planting lines
- use `Wind Fence`, `Straw Checkerboard`, and early protection plants together on exposed edges
- use `Water Tank` plus linked `Drip Irrigator`s to keep moisture pressure low for fragile starter plants during their first growth window, while keeping the tanks filled
- use `Weather Mast` to turn forecast knowledge into a real planning advantage
- use `Solar Array` to make a mature support pocket feel operationally stronger instead of just greener
- use `Field Workshop` to craft useful supplies from stored materials; it does not directly speed up hazard recovery by itself

## 11. Ecology And Restoration Systems

### Plant Families

Plants are role-based rather than based on exact real species in the first draft. This keeps the design readable and expandable.

The long-term taxonomy can still use three broad families:

- Protector plants: reduce wind, provide shade, lower local heat pressure, shield neighboring tiles, and lower growth pressure on fragile plants.
- Restorative plants: improve soil fertility, reduce erosion, expand soil moisture capacity through soil improvement, or gradually reduce tile salinity.
- Output plants: provide limited economic or food value without replacing the restoration focus.

In the current design, however, the more important lens is four gameplay roles:

- Protection
- Anti-dehydration
- Fertilize
- Output

Each plant should combine 2 of these roles so even a very small roster can still create placement decisions and simple combo stories.

Some plants in the current set can also carry a secondary worker-support identity or salinity-rehabilitation identity through `Salt Tolerance` and `Salinity Reduction` tags, but the main balance reading should still come from these core ecological roles.

### Plant Traits

Each plant family can unlock variants or upgrades through `Plant Trait`s. Traits create strategic depth without turning the game into genetics simulation.

Example trait directions:

- Better storm tolerance
- Lower water demand
- Better salt tolerance
- Soil salinity reduction
- Height class
- Stronger adjacent wind reduction
- Stronger soil retention through fertility improvement
- Faster establishment
- Better output value
- Improved synergy with nearby devices

#### Plant Height Class

Every living plant should also have a simple height tag used for structure-sharing rules:

- `Low`
- `Medium`
- `Tall`

Design intent:

- `Low` plants can coexist with the broadest set of shared structures
- `Medium` plants may coexist only with more permissive elevated structures
- `Tall` plants usually want open tiles and should be blocked by most shared infrastructure

This is mainly here so structures such as irrigation lines or solar racks can share space with some plants without making tile occupancy unreadable.

### Plant Design Goal

For concept verification, the plant system should stay intentionally small. The first playable does not need a realistic or complete ecosystem. It only needs enough plant variety to test whether:

- mixed-role plants are more interesting than single-purpose plants
- neighbor-tile effects create readable placement decisions
- protection, anti-dehydration, fertilization, output, and salinity rehabilitation can form a satisfying loop
- players enjoy building small local combos under environmental pressure

The current target is a few plants, not a full roster. If this smaller set is not fun, expanding the content count will not solve the core problem.

### Plant Rules (Temp Design)

To keep the first playable understandable:

- Each plant should combine 2 roles rather than being purely one thing
- Each plant should have a direct effect on its own tile, and any neighborhood effect should come from positive contribution values plus shared `auraSize`
- Neighbor effects should use `auraSize` with Manhattan-distance reach for readability
- Same-type bonuses should stack softly or cap early so the board stays readable
- The player should be able to understand a placement decision in a few seconds
- The player should only place the lowest-density starter version of a plant; stronger plant states should come from growth, not instant deployment
- Density should be the main driver of plant power, survival, and natural spread
- Salty tiles should create visibly different plant choices, with some plants occupying them early and others mainly preparing them for later crops
- One plant-derived starter material is allowed for extremely poor sand: `Straw Checkerboard` starts at maximum `tilePlantDensity`, uses the shared plant trait fields with notable `windProtectionPower`, `dustProtectionPower`, and `fertilityImprovePower`, and uses `growable = false` plus a positive `constantWitherRate` while its `growthPressure` stays `0`

### Plant Set (Temp Design)

The following plants are temporary placeholders for the current build. Their names, values, visuals, and real-world grounding should be refined later. Right now the goal is to verify the concept with a small, mixed-role set.

| Plant | Role Mix | Effect Ceiling (Temp) | Work Demand (Temp) | Own Tile Effect | Neighbor Tile Effect | Use |
|---|---|---|---|---|---|---|
| `Straw Checkerboard` | Protection + Fertility setup | Medium | Low | Plant-derived straw grid placed on bare sand; it starts with strong near-surface wind and sand reduction on its own tile, while its shared contribution powers scale down as density decays over time | Adjacent tiles gain early erosion relief, slightly better establishment odds, and a temporary protection buffer while the checkerboard is still fresh | Critical opener for least fertile or pure sand tiles; makes hostile sand workable for later living plants |
| `Wind Reed` | Protection + Anti-dehydration | Medium | Low | Cheap early plant that reduces wind damage and slows dehydration on its own tile | Adjacent tiles get small wind reduction and slight dehydration relief | First safety plant; teaches line placement and perimeter thinking |
| `Shade Cactus` | Anti-dehydration + Worker support | Medium | Low | Strong local heat reduction and low water demand | Adjacent tiles lose water more slowly; nearby work zones feel safer during hot periods | Teaches oasis pockets, camp-edge defense, and local survival planning |
| `Root Binder` | Fertilize + Protection | Medium | Medium | Stabilizes soil, reduces erosion, and slowly improves difficult ground on its own tile | Adjacent tiles gain fertility and mild erosion resistance, helping surrounding salty tiles recover over time | Teaches groundwork and setup before high-value planting |
| `Salt Bean` | Output + Fertilize | Medium | Medium | Produces modest food or sellable yield if the tile is stable enough while also serving as the main early salt-rehab crop | Adjacent tiles gain a small fertility bonus and light salinity-recovery support that helps future crops | Teaches low-risk output and the value of mixed production rows on difficult land |
| `Sunfruit Vine` | Output + Anti-dehydration synergy | High | High | High output but weak when exposed | Adjacent shaded or fertilized tiles improve its performance; it also gives small output synergy to nearby output plants | Teaches greedy high-reward planting that depends on support plants |
| `Dew Grass` | Anti-dehydration + Fertilize | Medium | Low | Captures trace moisture and softens local dryness on poor or mildly salty soil | Adjacent tiles gain slight moisture retention, a small fertility bump, and weak salinity-rehab support | Gentle bridge plant for expanding from safe patches into dry but not fully dead land |
| `Thorn Shrub` | Protection + Output | Medium | Medium | Tough shrub that creates a sturdier barrier and a small fiber or resin yield | Adjacent tiles gain minor wind buffering and a little protection against density loss | Teaches productive perimeter building instead of pure defense |
| `Medicinal Sage` | Output + Worker support | Medium | Medium | Produces a modest herb yield and improves comfort in its immediate area | Adjacent tiles slightly improve short-rest efficiency and morale feel when conditions are stable | Teaches support-economy patches and non-food output value |
| `Sand Willow` | Protection + Anti-dehydration anchor | High | High | Large anchor plant that creates strong local shelter and heat relief when sustained | Adjacent tiles gain one of the strongest protection and dehydration-relief effects in the current set | Late anchor plant for turning one hard-earned zone into a true refuge core |

`Straw Checkerboard` is inspired by the real Chinese straw checkerboard dune-control method, abstracted here into a plant-authored ground-cover option for sand fixation and land preparation.

`Effect Ceiling` is the rough maximum payoff of a dense healthy tile. `Work Demand` is the rough amount of setup, maintenance, and rescue attention the plant usually asks for before it feels dependable. Both are temporary tuning tags for iteration, not locked balance values.

### Trait Tags And Values (Temp Design)

For clarity, each plant should also carry a small trait package. These are temporary design tags, not final balance values, but they define what makes one plant feel different from another beyond density state alone.

| Plant | Core Trait Combination | Main Risk Or Dependency |
|---|---|---|
| `Straw Checkerboard` | `Sand Fixation`, `Surface Roughness`, `Establishment Boost`, `Organic Matter Setup` | `No Yield`, `No Natural Spread`, contribution strength fades over time as current density falls |
| `Wind Reed` | `Fast Establishment`, `Flexible Windbreak`, `Moisture Hold`, `Storm Recovery` | Effect ceiling is moderate; wants other plants behind it to convert safety into real value |
| `Shade Cactus` | `Deep Shade`, `Low Water Demand`, `Heat Buffer`, `Comfort Pocket` | Best in concentrated pockets rather than long exposed lines |
| `Root Binder` | `Soil Lock`, `Erosion Anchor`, `Fertility Lift`, `Salinity Prep` | Medium work and low direct payoff by itself; shines when followed by other plants |
| `Salt Bean` | `Modest Yield`, `Soil Enrichment`, `Salt Rehab`, `Patch Builder` | Output drops hard if protection and fertility are not maintained |
| `Sunfruit Vine` | `High Yield`, `Support Hungry`, `Shade Hungry`, `Fertility Hungry` | Very fragile in exposed or low-support zones; high rescue demand |
| `Dew Grass` | `Dew Capture`, `Dryland Bridge`, `Soft Fertility`, `Light Salt Relief` | Lower peak payoff than harder specialist plants |
| `Thorn Shrub` | `Tough Barrier`, `Edge Yield`, `Density Retention`, `Harsh Tolerance` | Not the best at pure defense or pure output; wins by doing both reasonably well |
| `Medicinal Sage` | `Herb Yield`, `Comfort Aura`, `Rest Boost`, `Stable-Zone Preference` | Wants an already somewhat safe pocket before it feels worth the slot |
| `Sand Willow` | `Refuge Canopy`, `Deep Root Anchor`, `Strong Shade`, `Refuge Core` | High early maintenance and high cost before it becomes a major payoff plant |

These trait combinations should be the main language for future tuning passes. If a plant feels weak or redundant later, adjust its trait package first before replacing the whole plant concept.

Salinity trait values should live with the rest of the plant trait data rather than as a separate system. To keep the first salinity pass light, the current design only needs two extra plant-facing trait values:

- `Salt Tolerance`: how much density the plant can still retain on salty tiles
- `Salinity Reduction`: how strongly the plant gradually improves current tile salinity for future planting

This should be enough to make salty tiles create real placement choices without adding a broader chemistry or water-quality simulation.

| Plant | Salt Tolerance | Salinity Reduction | Salty-Tile Role |
|---|---|---|---|
| `Straw Checkerboard` | `N/A` | `None` | Ground cover setup layer; it can help prepare the surface, but it is not a true saline-soil answer by itself |
| `Wind Reed` | `Medium` | `Low` | Acceptable first stabilizer on mildly salty exposed tiles |
| `Shade Cactus` | `Medium` | `None` | Can survive some salinity, but mainly solves heat and dehydration |
| `Root Binder` | `Medium` | `Medium` | Good support plant for gradually improving difficult soil |
| `Salt Bean` | `High` | `High` | Primary salt specialist and best early answer for highly salty tiles |
| `Sunfruit Vine` | `Low` | `None` | Poor choice for salty land unless the tile is improved first |
| `Dew Grass` | `Medium` | `Low` | Bridge plant for moderately salty tiles that still need fertility recovery and gentler dryness |
| `Thorn Shrub` | `Medium` | `Low` | Tough secondary option for harsher perimeter pockets |
| `Medicinal Sage` | `Low` | `None` | Wants a cleaner, safer tile before it feels worthwhile |
| `Sand Willow` | `High` | `Medium` | Late stronger anchor for salty refuge-building once support is in place |

Design intent:

- `Salt Bean` should make the salinity system legible immediately
- `Root Binder` and `Sand Willow` should create follow-up progression on difficult tiles
- low-tolerance plants should teach the player that some land must be prepared before high-value planting is realistic

### Plant Height Tags (Temp Design)

To support structure-sharing rules, the current plant set should also carry a simple height tag:

| Plant | Height Class | Structure-Sharing Read |
|---|---|---|
| `Straw Checkerboard` | `N/A` | Ground cover only; structure sharing with living plants does not apply |
| `Wind Reed` | `Low` | Good candidate for sharing under `Solar Array` or through `Drip Irrigator` |
| `Shade Cactus` | `Medium` | Usually too tall for low-clearance shared structures |
| `Root Binder` | `Low` | Good under technical support structures that allow low plants |
| `Salt Bean` | `Low` | Good candidate for shared irrigation and solar-support tiles |
| `Sunfruit Vine` | `Low` | Can share with permissive structures but still depends on broader support |
| `Dew Grass` | `Low` | Good bridge plant for shared low-clearance support lanes |
| `Thorn Shrub` | `Medium` | Usually blocked by low-clearance shared structures |
| `Medicinal Sage` | `Low` | Can share with low-clearance support structures |
| `Sand Willow` | `Tall` | Should usually require an open tile with no shared structure overhead |

Design intent:

- low vegetation enables denser technical layouts
- medium and tall plants force the player to reserve more open ecological space
- this gives structures and plant choice another readable tradeoff without requiring a complicated layering sim

### Plant Progression (Temp Design)

The game should use the plant roster to create short-term goals, not only long-term restoration.

- Starting plant access should use a fixed early-session progression, not procedural variation from site to site
- Begin each new `Site` with only one basic starter option available, preferably `Straw Checkerboard`
- Use early `Site Task`s, onboarding rewards, or early `Task Reward Draft`s to unlock the rest of the fixed basic plant set for that site, especially `Wind Reed` and `Root Binder`
- After that fixed early unlock track, let further early rewards reveal `Shade Cactus`, `Dew Grass`, or `Salt Bean`, with `Salt Bean` prioritized on sites featuring meaningful salty-tile pockets
- Put `Thorn Shrub` and `Medicinal Sage` in the mid-scope reveal band so the player starts seeing more distinct patch identities
- Hold `Sand Willow` and `Sunfruit Vine` slightly later so they feel like tempting payoff plants rather than early traps
- Make at least one short-term objective in the opening phase plant-related, such as fixing a bare sand lane, establishing a windbreak, stabilizing a dry patch, or producing a first safe harvest

This keeps the early site loop readable:

- unlock or reveal the next plant option in the fixed early progression
- buy seeds or the related `Site Unlockable`
- place a small low-density combo
- protect and maintain it through the fragile growth phase
- survive the local weather check
- let the patch gain density and become more self-sustaining
- collect a visible output or fertility improvement
- reinvest into the next combo

### Combo Logic

This early set should already support a few clear placement stories:

- `Straw Checkerboard` + `Root Binder`: turn pure sand into a workable starter patch for later living cover
- `Straw Checkerboard` + `Wind Reed`: create a storm-facing starter barrier on exposed dunes
- `Dew Grass` + `Root Binder`: create a soft restoration bridge that upgrades dry land into living soil
- `Wind Reed` + `Shade Cactus`: create a survivable edge or protected work pocket
- `Wind Reed` + `Thorn Shrub`: build a perimeter that both protects and slowly pays back
- `Shade Cactus` + `Medicinal Sage`: create a support pocket that feels good to stand and recover in
- `Root Binder` + `Salt Bean`: turn unstable salty land into a modest productive patch
- `Dew Grass` + `Salt Bean`: open a mild-salinity lane that still needs water-sensitive support
- `Root Binder` + `Sunfruit Vine`: set up a high-reward crop that feels earned
- `Root Binder` + `Sand Willow`: build a refuge core that can hold against repeated weather pressure
- `Wind Reed` + `Sunfruit Vine`: protect a risky output line from collapse
- `Shade Cactus` + `Sunfruit Vine`: reduce dehydration pressure on a greedy output plant
- `Sand Willow` + `Medicinal Sage`: create a late safe zone with strong recovery feel and support value

The game should not aim for perfect balance yet. It should aim for understandable combo moments where the player can feel "this plant supports that plant."

### Plant Growth And Spread

Plants should not appear at full strength when placed. The player is planting a starter foothold, not a finished ecosystem.

Core growth rules:

- The player can only plant the initial low-density state
- Low-density plants are fragile, weak, and easily pushed into high `growthPressure`
- If water, fertility, salinity suitability, protection, and weather conditions keep `growthPressure` low enough, density rises over time
- As density rises, the plant's own-tile effect and neighbor-tile effect become stronger
- A sufficiently healthy dense tile can spread into an orthogonally adjacent empty tile without direct player planting
- Natural spread should only happen if the target tile is empty, fertile enough, and not an obviously wrong salinity match for the spreading plant

Shared-model checkerboard notes:

- `Straw Checkerboard` is a plant-authored starter cover used for hostile sand
- It should not produce output and should not naturally spread
- It should reuse the same plant trait fields as living plants for protection, setup, and soil-support effects
- It should begin at full `tilePlantDensity`, then gradually lose density over time through `constantWitherRate`
- It does not grow like normal living plants because `growable = false`, and its `growthPressure` stays at zero
- It is especially valuable on pure sand or near-barren tiles where normal living plants would struggle to establish immediately

This means density is doing several jobs at once:

- growth progress
- effect strength
- hazard resistance
- restoration momentum

The intended fantasy is that the player establishes life, protects it through its weakest phase, and then gradually earns a patch that can hold itself together and start reclaiming nearby ground.

### Plant Density Meter

The game should treat `tilePlantDensity` as one continuous meter rather than a separate ladder of named growth states.

For tuning:

- direct player planting creates a new tile at `20` density
- natural spread creates a new tile at `20` density
- trait strength should scale smoothly with `tilePlantDensity` rather than through fixed named stages

### Plant Density Scaling Rule

Each living plant should gain clearer value as density rises so growth feels meaningful rather than cosmetic. In the current design, plant contribution strength, output strength, and local support value should scale linearly with `tilePlantDensity` unless a later plant-specific exception is explicitly authored. `Straw Checkerboard` uses the same density meter and the same contribution scaling rule, but with `growable = false` and positive `constantWitherRate`.

- direct player planting creates a living plant tile at `20` density
- natural spread creates a living plant tile at `20` density
- `Straw Checkerboard` starts at `100` density and then steadily decays

Important traits for `Straw Checkerboard`:

- can be placed on pure sand or the least fertile empty tiles
- starts at maximum `tilePlantDensity` and then gradually loses density over time
- reuses the same plant trait fields as living plants, with `growthPressure` held at zero
- gives maximum wind and sand-movement reduction right after placement, even before living cover exists
- helps trap sand and fine particles instead of only resisting damage
- uses normal `fertilityImprovePower` and protection traits while its current density remains high
- strongly helps low-density living plants survive nearby
- has no direct output economy and no self-spread, so it remains a setup tool rather than a full solution

Important traits for the added plants:

- `Dew Grass`: low work bridge plant that smooths expansion into dry tiles without creating huge direct output
- `Thorn Shrub`: perimeter plant that mixes safety and modest economy so defense can feel productive
- `Medicinal Sage`: support-output plant that helps a patch feel inhabited and worth returning to
- `Sand Willow`: high-work anchor that creates a strong refuge core when the player commits enough support

### Plant Density And Degeneration

Plants should follow a simple visible survival model. They are not guaranteed to grow just because they were placed.

Primary player-facing read:

- density is the plant's main state on a tile
- the tile should show the current density value or density bar directly
- a lighter trend label should show whether that density is improving or collapsing

| Trend State | Meaning | Gameplay Result |
|---|---|---|
| `Growing` | The current density is increasing | Effects are improving and the patch is moving toward a safer state |
| `Holding` | The current density is broadly stable | The patch is surviving, but not rapidly improving |
| `Withering` | The current density is falling or close to collapse | Output and support value are weakening; the player has a short rescue window |
| `Dead` | The plant is lost | Seeds, labor, time, and money are wasted; the tile becomes exposed again and often worsens local erosion |

Severe events should not only kill plants outright. They should often reduce `Plant Density` first, creating partial fallback before full collapse.

Density results:

- high density: strong output, strong own-tile effect, strong neighbor support
- reduced density: lower output, weaker protection, weaker neighbor bonuses
- sparse density: little output, fragile protection, high risk of withering on the next bad weather window
- collapsed density: effective tile failure, usually followed by death or full replacement need

`tileHeat`, `tileWind`, `tileDust`, and healthy `tileMoisture` should strongly influence how much density is lost during a severe event:

- high local pressure: density can collapse quickly
- moderate local pressure: density falls, but the patch usually remains worth saving
- low local pressure: density loss is limited and the patch keeps most of its role value

This is one of the main rewards for good strategy. The player should be able to see that the same hazard which ruins an exposed patch only partially dents a low-pressure, well-protected patch.

Low-density plants should be the most vulnerable part of the lifecycle. This is where the player's maintenance and planning matter most. A patch that survives long enough to reach high density should be much better at enduring normal site pressure.

Density loss should come mainly from:

- wrong plant choice for the current tile exposure
- wrong plant choice for the current tile salinity
- poor adjacency or missing support plants
- water shortage or delayed maintenance
- severe heat or wind hitting a fragile patch
- sand burial left uncleared
- nearby dead tiles reopening the zone to exposure

Good choices reduce exposure and let a patch become stable. Bad choices create repeated density loss until the patch loses value or collapses.

Good maintenance in the early phase should include:

- watering or restoring water access on time
- giving the plant enough nearby protection
- reducing exposure through better placement or urgent field work
- preserving fertile support tiles and adjacent helper plants
- using salt-tolerant or salinity-reducing plants when the tile is not yet ready for greedy crops

The player should feel that they are nursing a fragile patch toward self-sufficiency. Once the patch is dense enough, it should require less rescue frequency and start paying back the effort with stronger protection, output, or spread.

### Local Collapse And Recovery

The game should support both upward and downward spirals.

Good spiral example:

- The player places `Wind Reed` and `Root Binder` in an exposed lane
- The patch survives a harsh weather window
- That survival protects a later `Salt Bean` or prepares the lane for a later `Sunfruit Vine`
- The new output funds more tech, supplies, or repairs
- The site becomes easier to expand safely

Bad spiral example:

- The player plants greedy output in an exposed lane
- Heat, wind, or erosion quickly cause rapid density loss
- `Plant Density` falls, so output drops and neighbor bonuses weaken
- A major event also burns stored supplies and worsens `Player Condition`
- The player earns less money and delays recovery purchases
- The patch withers or dies, reopening the zone and threatening nearby tiles

The key design goal is that failure should be painful but playable. A downward spiral must be reversible, but only through deliberate action such as:

- spending emergency money on water, repairs, or temporary shelter
- replacing a greedy plant with a safer plant
- clearing burial or restoring irrigation
- using contractors to save time during a crisis
- choosing to abandon a fringe patch to protect the core patch

This makes strategy matter. Good planning creates thriving momentum. Bad planning creates recoverable but expensive problems.

### Comeback Priorities After A Bad Event

After a severe hazard event, the player should not be able to restore everything immediately. A strong comeback decision usually means picking the highest-leverage recovery target first.

Comeback priorities:

- rebuild the core protective lane so more tiles do not unravel
- restore water or utility flow so the site can function again
- keep one salinity-rehab lane alive so later high-value planting is still possible
- rescue one output patch to restore money flow
- recover `Player Condition` so the worker can perform meaningful outdoor actions again
- accept a temporary setback on fringe tiles while saving the central patch

This prioritization is part of the intended strategy. The player should feel pressure to ask, "what gets me back into a stable position fastest?"

### Density And Natural Expansion

The game should let successful plant patches begin reclaiming nearby land on their own.

Natural expansion rules:

- only high-density tiles can attempt natural spread
- spread should check orthogonally adjacent tiles first
- the destination tile must be empty
- the destination tile must be fertile enough and otherwise suitable enough that the new starter plant is not immediately pushed into extreme `growthPressure`
- natural spread should create a new low-density tile, not a free mature tile

This keeps player agency intact. The player still decides where the first footholds go, which species are introduced, and which zones get support first. Growth and spread are the earned payoff for that earlier strategy.

Natural spread also improves the restoration fantasy:

- the player is not manually planting every final tile
- successful systems begin reproducing visible life
- dense safe patches gradually help reclaim nearby empty land

The most important balance rule is that natural spread should feel like a reward for a well-built patch, not a replacement for decision-making.

### Plant Resolution Rules

The system now needs one explicit runtime contract for how plants actually resolve each fixed simulation step. This section is the implementation-facing version of the plant rules described above.

#### Plant Runtime Data Contract

Each occupied `livingPlantLayer` tile should have these authoritative runtime values:

- `plantTypeId`
- `tilePlantDensity` in range `0-100`
- `growthPressure` in range `0-100`
- `tileHeat`
- `tileWind`
- `tileDust`
- `tileMoisture`
- `tileSoilFertility`
- `tileSoilSalinity`
- `tileSandBurial`

If the same tile also contains `groundCoverLayer`, that layer contributes through:

- `groundCoverTypeId`
- `tilePlantDensity`
- the `Straw Checkerboard` plant trait profile, evaluated through current `tilePlantDensity`

Important modeling rule:

- `plantTypeId`, `groundCoverTypeId`, and `structureTypeId` answer occupancy identity only
- `tilePlantDensity` and `growthPressure` belong to plant state, not to occupancy identity
- `Plant Trend` belongs to player-facing display and should be derived from density direction and pressure, not stored as a separate authoritative plant state
- `deviceIntegrity` and `deviceEfficiency` belong to structure state, not to occupancy identity
- `Straw Checkerboard` is a ground-cover occupant that reuses plant trait fields, starts at full `tilePlantDensity`, and uses `growable = false` plus a positive `constantWitherRate`
- `tileHeat`, `tileWind`, and `tileDust` are resolved local weather meters rebuilt each fixed simulation step after site weather and current local support are combined
- the game should not use a separate `tileSoilStability` meter or a separately stored `tilePlantStress` value

Each plant definition should also provide these plant-side values. These are not new runtime state categories. Core simulation should use the end-of-document meter-relationship chapter as the canonical meter reference.

Placement:

| Field | Range | Meaning |
|---|---|---|
| `plantHeightClass` | `Low`, `Medium`, or `Tall` | How much vertical space the plant occupies for structure-sharing validation |

Behavior profile:

| Field | Range | Meaning |
|---|---|---|
| `growable` | Boolean | Whether favorable conditions are allowed to increase `tilePlantDensity` |
| `constantWitherRate` | `0-100` | Fixed density-loss rate applied every fixed simulation step; normal plants use `0`, while `Straw Checkerboard` uses a positive value |

Resistance profile:

| Field | Range | Meaning |
|---|---|---|
| `saltTolerance` | `0-100` | How strongly the plant resists salinity-based density-cap reduction |
| `heatTolerance` | `0-100` | How much ambient heat the plant can endure before heat starts raising `growthPressure` |
| `windResistance` | `0-100` | How much ambient wind and erosion pressure the plant can endure before wind starts raising `growthPressure` |
| `dustTolerance` | `0-100` | How much dust intensity and burial pressure the plant can endure before dust starts raising `growthPressure` |

Ecological contribution profile:

| Field | Range | Meaning |
|---|---|---|
| `auraSize` | Small integer `0-2` | Maximum Manhattan-distance reach of this object's positive contribution values; `0` means own tile only |
| `windProtectionPower` | `0-100` | How strongly this plant reduces nearby `tileWind` |
| `heatProtectionPower` | `0-100` | How strongly this plant reduces nearby `tileHeat` |
| `dustProtectionPower` | `0-100` | How strongly this plant reduces nearby `tileDust` |
| `fertilityImprovePower` | `0-100` | How strongly this plant gradually improves `tileSoilFertility` while healthy and how strongly it pushes back against erosion-driven fertility loss in the current design |
| `salinityReductionPower` | `0-100` | How strongly this plant gradually reduces `tileSoilSalinity` while healthy |

These contribution traits are plant-side inputs into the shared object contribution meters. `auraSize` belongs to the same contribution family and determines how far those contribution values can reach, while current `tilePlantDensity` scales how much of each contribution is currently active.

Expansion and output profile:

| Field | Range | Meaning |
|---|---|---|
| `spreadReadiness` | `0-100` | Minimum low-pressure readiness needed before this plant may spread reliably |
| `spreadChance` | `0-100` | Relative chance to create one new starter tile during an ecology pulse |
| `outputDependency` | `0-100` | How badly output collapses as `tilePlantDensity` falls |

Note:

- the current set tables can keep using design language such as `Low`, `Medium`, and `High`

Ground-cover definitions should not define a separate trait family. In the current design, `Straw Checkerboard` should use the same trait fields as plants, especially non-zero `windProtectionPower`, `dustProtectionPower`, and `fertilityImprovePower`, set `growable = false`, set `constantWitherRate` to a positive value, set `auraSize` as needed for local setup support, set spread-related fields to `0`, and use the shared runtime meter `tilePlantDensity`, which starts full and steadily loses density while present.

#### Object Contribution And Resolved Local Weather Build Rules

Shared object contribution meters should be rebuilt every fixed simulation step from nearby plants, devices, and terrain shelter. Then `tileHeat`, `tileWind`, and `tileDust` should be rebuilt from site weather plus those current local contribution meters and kept within their normal meter ranges.

Use this contribution pattern:

| Source | Reach | `windProtectionPower` | `heatProtectionPower` | `dustProtectionPower` | Rule |
|---|---|---|---|---|---|
| Own-tile living plant or ground cover | Own tile always | Add current local contribution | Add current local contribution | Add current local contribution | Scale by current `tilePlantDensity` and the relevant shared plant contribution values |
| Neighbor tile within plant `auraSize` | Manhattan distance `<= auraSize` | Add small to medium contribution | Add small to medium contribution | Add small to medium contribution | Only positive plant contribution values reach neighbors; keep neighbor effect weaker than own-tile effect in the current design |
| Nearby device or rock shelter | Device footprint or authored shelter range | Add medium to strong contribution | Add medium to strong contribution | Add light to medium contribution | Scale by device power, `deviceEfficiency`, or authored rock-shelter value |

Rules:

- plant-side neighborhood reach should come from shared `auraSize`, not from separate per-contribution neighbor-aura fields
- `auraSize = 0` means own tile only
- `auraSize = 1` means own tile plus the `4` orthogonal neighbors
- `auraSize = 2` means own tile plus all tiles within Manhattan distance `2`
- if a plant has no positive contribution values, `auraSize` has no gameplay effect
- if a tile has no living plant and no `Straw Checkerboard`, nearby support may still create useful object contribution meters and resolved local weather reduction, but the tile remains empty until planted or spread into
- rock protection should usually be low-radius and local, just enough to make lee-side placement readable
- `Straw Checkerboard` uses the same own-tile and neighbor contribution logic as other plant-authored occupants, scaled by current `tilePlantDensity` and `auraSize`
- `Drip Irrigator` is not an object-contribution source for weather protection; it applies irrigation directly to `tileMoisture`

This pattern keeps local support readable:

- `windProtectionPower` is the shared local wind-protection meter built from nearby objects
- `heatProtectionPower` is the shared local heat-protection meter built from nearby objects
- `dustProtectionPower` is the shared local dust-protection meter built from nearby objects
- resolved local weather reads those shared object contribution meters instead of reading object names directly

#### Soil Meter Interaction Rules

Every fixed simulation step, each plantable `Ground` tile should update its soil meters from weather, occupancy, and support.

Use these relationship rules instead of locked formulas:

- `Straw Checkerboard` uses `tilePlantDensity` as its remaining surface-cover strength
- while its `tilePlantDensity` is high, `Straw Checkerboard` provides stronger protection and stronger temporary setup value
- `constantWitherRate` should reduce `tilePlantDensity` on `Straw Checkerboard` every fixed simulation step

Moisture rule:

- `tileHeat` and `tileWind` increase moisture loss
- `watering` and direct irrigation such as `Drip Irrigator` increase `tileMoisture`
- local support should already be reflected inside resolved `tileHeat` and `tileWind`, so terrain moisture reads those resolved meters instead of reading raw support values again
- higher `tileSoilFertility` raises the tile's maximum `tileMoisture` capacity and slows moisture reduction, but does not directly add moisture
- `tileMoisture` should stay within its normal meter range

Fertility rule:

- `tileWind` and `tileDust` create erosion pressure
- local support should already be reflected inside resolved `tileWind` and `tileDust`, so terrain fertility reads those resolved meters instead of reading raw support values again
- `tileSandBurial` adds additional fertility loss if left uncleared
- `fertilityImprovePower` is the full soil-building trait: on healthy tiles and on nearby tiles inside plant `auraSize`, it raises `tileSoilFertility` and also fights part of the fertility loss caused by erosion and burial
- `Straw Checkerboard` uses the same `fertilityImprovePower` logic while present; because its `tilePlantDensity` falls through `constantWitherRate`, its total soil-building effect and its pushback against erosion-driven fertility loss both fade automatically over time
- when `tilePlantDensity` is exhausted, clear the checkerboard from the tile
- `tileSoilFertility` should stay within its normal meter range

Interpretation:

- `tileSoilFertility` is the long-term land quality meter
- higher fertility directly improves plant growth speed, raises the tile's maximum moisture capacity, and slows moisture reduction without directly adding moisture
- erosion means direct `tileSoilFertility` reduction caused by wind-and-sand exposure; there is no separate hidden stability meter
- high `tileHeat` should not directly cause erosion, but it can indirectly make erosion more likely by draining moisture, increasing plant `growthPressure`, and weakening cover if the player ignores it
- local protection sources such as plants, `Wind Fence`, and rock shelter lower resolved `tileWind` and `tileDust`, which then reduces wind-and-sand-driven `tileSoilFertility` loss
- the game should not use a separate erosion-control trait; `fertilityImprovePower` should handle both long-term soil improvement and the pushback against erosion-driven fertility loss
- `Straw Checkerboard` should begin as strong protection, then gradually lose that protection and its other density-scaled contribution strength as `tilePlantDensity` falls
- uncleared burial can permanently set back a tile over time by converting some temporary burial pressure into fertility loss

#### Salinity Density-Cap Rules

Salinity should not directly add plant damage. Instead, it should limit how dense a plant can become on that tile.

Use this runtime interpretation:

- salt-tolerant plants should still reach strong density on salty tiles
- low-tolerance plants should survive more weakly there, plateau earlier, and feel like the wrong long-term choice
- this makes salinity a strategic placement and rehabilitation problem rather than an immediate death sentence

Rule:

- if `tileSoilSalinity` is comfortably within a plant's `saltTolerance`, that plant should keep its normal density ceiling
- if `tileSoilSalinity` rises beyond the plant's `saltTolerance`, that plant's usable density ceiling should fall
- the reduction should be strong enough to create real planting choices, but not so extreme that a tile becomes an instant dead end

Interpretation:

- if salinity stays within tolerance, the plant keeps its normal density ceiling
- if salinity rises above tolerance, the plant's effective maximum density falls
- a higher `saltTolerance` does not remove salinity from the system; it simply preserves more usable density on difficult tiles

#### Density Gain And Density Loss Rules

`tilePlantDensity` should change every fixed simulation step, not only during long pulses.

Use this logic:

- derive water readiness from current `tileMoisture`
- derive soil readiness from `tileSoilFertility` plus the current burial situation
- derive wind and dust exposure from `tileWind`, `tileDust`, and the plant's own resistances
- derive heat exposure from `tileHeat`, `tileMoisture`, and the plant's `heatTolerance`
- combine these grouped pressures into `growthPressure`

A living plant grows best when `growthPressure` is low. Local support should matter only by lowering that pressure, not by bypassing it with a separate direct growth bonus.

#### Growth Pressure Grouping Rule

- `waterContribution` should represent moisture shortage from current `tileMoisture`
- `soilContribution` should represent poor fertility plus burial-related land pressure
- `windContribution` should represent raw exposure from `tileWind` and `tileDust` after plant resistances are considered
- `heatContribution` should represent heat exposure from `tileHeat` after plant heat resistance and moisture relief are considered
- if tuning later adds or changes sub-factors, engineering may rebalance which internal sub-terms feed which grouped channel, but the player-facing grouped channels should remain stable unless there is a strong usability reason to change them

Density resolution rule:

- low `growthPressure` should increase `tilePlantDensity`
- if `growable` is `false`, favorable conditions should not create positive density gain
- high `growthPressure` should reduce `tilePlantDensity`
- `constantWitherRate` should apply steady density loss every fixed simulation step
- if the plant is currently above its `salinityDensityCap`, density should fall until it returns to what that tile can support
- low-density plants should be the most fragile stage
- dense established plants should hold more steadily under normal pressure
- peak-phase events should increase plant danger mainly by pushing the weather meters higher, not by bypassing the meter model with a separate direct plant-damage rule

Placement rule:

- direct planting should create a new living plant tile in a fragile low-density state
- natural spread should also create a new living plant tile in a fragile low-density state

#### Plant Trend Display Rules

`Plant Trend` should be derived from current density plus the current direction of change after each fixed simulation step. It is a derived state, not a separate plant trait or authoritative runtime meter.

Use these rules:

| `Plant Trend` label | Meaning |
|---|---|
| `Empty` | No living plant is currently on the tile. |
| `Growing` | The plant is gaining density because current `growthPressure` is low enough. |
| `Holding` | The plant is broadly stable, with density neither rising nor collapsing quickly. |
| `Withering` | The plant is losing density because current pressure, burial, or tile mismatch is too high. |
| `Dead` | The plant has collapsed and is removed from the tile. |

Death cleanup rule:

- when a plant reaches `Dead`, clear `plantTypeId`, reset `tilePlantDensity`, apply a small fertility setback, and add some burial setback to the tile

This cleanup creates the intended punishment for bad decisions without making the whole site unrecoverable.

#### Output Degradation Rule

Output should degrade before a plant dies so the player feels pressure early.

Rule:

- plants with output should pay back more when `tilePlantDensity` is high
- high `growthPressure` should reduce output indirectly by pushing `tilePlantDensity` down before the plant dies
- plants with stronger `outputDependency` should lose output faster as `tilePlantDensity` falls
- if the tile is clearly `Withering`, output should stop

This makes greedy output plants visibly stop paying back before they fully collapse.

#### Natural Spread Resolution Rule

Natural spread should resolve only on the `ecologyPulse`, not every fixed step.

Each eligible source tile may attempt at most one spread per pulse.

A tile is eligible only if all of these are true:

- the plant definition allows natural spread
- the source tile is dense, healthy, and not currently being overwhelmed by pressure
- the source tile is not badly buried
- the source tile has enough water, fertility, and protection to count as locally stable
- the current salinity situation still allows strong density for that plant
- the current extreme hazard phase is not `Peak`

Candidate target rules:

- target must be orthogonally adjacent
- target must be `tilePlantable`
- target must have no living plant
- target must have no `structureLayer`
- target must have enough local fertility, moisture, and protection to support a fragile new plant
- target must offer at least a workable salinity match for the spreading plant

Selection and success rules:

- gather all valid candidate targets
- prefer the target with the best current combination of fertility, moisture, protection, low burial, and salinity suitability
- use `spreadChance` to decide whether the spread attempt succeeds this pulse
- on success, place the new tile in a fragile low-density state

Special rule:

- `Straw Checkerboard` never uses natural spread because it is a `groundCoverLayer` preparation object, not a living plant

#### Salinity Resolution Rule

The game should treat salinity as one extra local land-quality pressure, not as a full soil-chemistry simulation.

Simple runtime rule:

- each tile keeps its current `tileSoilSalinity`
- salinity should begin as an authored tile condition, not as a dynamically rebounding chemistry system
- healthy plants with salinity-mitigation traits may reduce current salinity gradually over time
- harsh events do not need to manipulate salinity directly
- salinity is therefore mainly a rehabilitation layer: bad at the start, better after deliberate restoration

Beneficial-change rule:

- healthy established plants with `salinityReductionPower` should gradually reduce `tileSoilSalinity` over time on their own tile and on nearby tiles inside their contribution reach
- badly stressed or collapsing plants should not rehabilitate salinity effectively
- `tileSoilSalinity` should stay within its normal meter range

Design intent:

- salt-tolerant plants let the player occupy difficult tiles earlier
- salt-reducing plants let the player improve those tiles for later, more demanding species
- low-tolerance plants on salty tiles should plateau at weak density rather than immediately taking direct salinity damage
- once a tile has been rehabilitated, the game should not constantly threaten to undo that progress through hidden salinity rebound

#### Straw Checkerboard Shared Plant Configuration

`Straw Checkerboard` should be authored through the same shared plant trait model:

- it uses `groundCoverTypeId` for layer occupancy so it can prepare land beneath a later living plant
- it uses the same plant-side contribution traits as other plant-authored occupants, especially meaningful `windProtectionPower`, `dustProtectionPower`, and `fertilityImprovePower`
- set `growable = false`
- set `constantWitherRate` to a positive value
- set `auraSize` as needed for setup reach
- on placement, start it at full `tilePlantDensity`
- its `growthPressure` should stay at zero
- it can exist on the same tile as one living plant type
- it does not produce output
- it does not naturally spread
- it does not regrow through the living-plant density ladder because `growable` is `false`
- `constantWitherRate` steadily reduces `tilePlantDensity` over time through the same generic density logic used by other plant-authored occupants
- as `tilePlantDensity` falls, its `windProtectionPower`, `dustProtectionPower`, and `fertilityImprovePower` contributions fade automatically because those values scale through current density
- when `tilePlantDensity` reaches `0`, clear the checkerboard from the tile

Setup target:

- treat `Straw Checkerboard` as a front-loaded setup layer that makes hostile sand workable, then gradually lose temporary protection and soil-support strength as its current density falls

### Ecology Verification Questions

The plant system should answer these questions early:

- Is placing plants for adjacency fun, or only optimal?
- Can players quickly understand why one layout survives and another fails?
- Do output plants feel more exciting when they depend on support plants?
- Does a wider 10-entry mixed-role roster create more fun combo discovery without becoming noisy or overwhelming?
- Does the player feel satisfaction when a fragile patch becomes stable because of combo placement?
- Do new plant unlocks create "one more task" momentum during the site instead of feeling like slow background progress?
- Does growing a patch from low density into self-sustaining cover feel rewarding enough to justify the maintenance effort?
- Does natural spread feel earned and readable rather than automatic noise?
- Does tile salinity create readable "right plant for the right ground" decisions without adding too much mental load?

### Tile Effects

Plants affect nearby tiles in readable ways. Effects may include:

- Wind reduction
- Heat mitigation
- Soil improvement
- Salinity reduction

Effect strength should scale with `Plant Density`. A low-density tile should offer limited support, while a medium-density or high-density tile should provide much stronger local value.

The player should be able to understand why a placement matters from tile conditions and nearby state.

### Stability And Erosion

Here, erosion means `tileSoilFertility` reduction from wind-and-sand exposure. Wind is the main driver, airborne sand makes it worse, and heat only contributes indirectly by drying tiles or weakening plant cover. A planted tile is therefore not automatically safe. Young or high-pressure growth can still lose soil quality when its local protection is too weak.

Successful restoration requires:

- The right plant in the right place
- The right plant for the tile's current salinity
- Enough water or support
- Protection from severe exposure
- Enough soil improvement and exposure protection from plants, ground cover, terrain shelter, or devices
- Follow-up maintenance

This makes restoration feel earned rather than decorative.

### Regional Ecology Effects

As sites become stable, their plant systems contribute to nearby `Site`s through reduced hazard intensity, cleaner starting land conditions, and stronger starting support pockets. This is the main expression of the game's regional restoration fantasy.

## 12. Economy, Task Board, And Progression

### Contract Board

The `Contract Board` is the main in-session progression driver. It is available only while the player is inside an active `Site` session, and it tracks the current site's active faction-published `Site Task` pool. It should not be used from the `Regional Map` when the player is still deciding where to deploy next.

Every `Site Task` should be published by one of three participating factions operating inside the same restoration program. The publishing faction must be visible on the board because it affects more than flavor: it changes reputation gain, assistant availability, random-event pools, and which long-term tech branches become easier to pursue.

| Faction | Board identity | Strategic pressure | Assistant |
|---|---|---|---|
| `Village Committee` | Local coordination, labor, and practical logistics | More tempo-driven jobs, repairs, deliveries, and fast deadline chains | `Workforce Support` |
| `Forestry Bureau of Autonomous Region` | Ecological stabilization, nursery access, and recovery planning | More plant-survival, anti-erosion, replanting, and water-sensitive jobs | `Plant-Water Support` |
| `Autonomous Region Agricultural University` | Research, field science, and experimental operations | More device, survey, precision, and forecasting jobs | `Device Upgrade Support` |

This faction layer should make the `Contract Board` more strategic and replayable. The player's question is no longer only "what pays best right now?" It is also "which relationship, assistant, and tech route do I want before the next pressure spike?"

### Rough Faction Profiles

These are rough faction identities for the current build. They are not final content counts or final balance, but they should be strong enough to guide onboarding, task generation, rewards, and tech-branch direction.

#### `Village Committee`

Core fantasy:

- local people and local logistics helping the player get practical work done fast
- less theory, more labor, repair, hauling, and field improvisation
- feels grounded, scrappy, and immediately useful

Player promise:

- "If I back this faction, I get momentum, cleanup power, and faster recovery from messy situations"

Onboarding focus:

- first faction introduced in the campaign
- teaches basic operations, direct labor, route clearing, repair flow, and task acceptance
- should be the faction that first unlocks the `Contract Board`

Basic tutorial task types:

- repair a damaged structure
- clear a buried route
- haul or deliver one material bundle
- stabilize one camp-adjacent work zone

Normal task bias:

- repairs
- hauling
- camp support
- route clearing
- fast delivery
- emergency labor tasks

Assistant:

- `Workforce Support`
- immediate sample reward should feel concrete: a temporary labor crew, fast build window, or instant cleanup push the player must exploit now

Random-event flavor:

- volunteer push
- local supply convoy
- community repair day
- labor shortage
- transport delay

Tech-branch rough themes:

- faster repairs
- lower hauling friction
- better contractor efficiency
- stronger deadline recovery
- more forgiving task-chain tempo

Weakness / tradeoff direction:

- excellent at getting work done now
- weaker at precision planning and long-term ecological efficiency than the other two factions

#### `Forestry Bureau of Autonomous Region`

Core fantasy:

- official restoration support focused on plant survival, water stability, and erosion control
- slower than pure labor tempo, but much stronger at making the site hold together
- feels disciplined, protective, and resilient

Player promise:

- "If I back this faction, my plants live longer, my recovery is cleaner, and harsh sites become more manageable"

Onboarding focus:

- second faction introduced in the campaign
- teaches plant-side basics after the player already understands labor basics
- should be the first faction that makes the player think about replanting, mulch, survival lanes, and water-sensitive recovery

Basic tutorial task types:

- replant a damaged patch
- secure one water-support lane
- protect one seedling cluster
- reinforce one erosion pocket

Normal task bias:

- replanting
- erosion control
- irrigation support
- density rescue
- water-sensitive sustain tasks
- post-event ecological recovery

Assistant:

- `Plant-Water Support`
- immediate sample reward should feel alive: saplings, mulch, water delivery, or temporary plant-survival support the player uses right away

Random-event flavor:

- sapling allocation
- erosion-control campaign
- emergency irrigation dispatch
- nursery shortage
- water rationing

Tech-branch rough themes:

- seedling survival
- irrigation efficiency
- density protection
- erosion control
- better post-event recovery

Weakness / tradeoff direction:

- strongest resilience branch
- usually slower and less explosive than `Village Committee`, less precise and less device-efficient than the university

#### `Autonomous Region Agricultural University`

Core fantasy:

- research-backed field support that improves decision quality, devices, and precise execution
- more setup-heavy and less immediately forgiving, but capable of high efficiency once the player understands the site
- feels technical, analytical, and high-upside

Player promise:

- "If I back this faction, I get better information, targeted infrastructure, and stronger optimization once my basics are stable"

Onboarding focus:

- third faction introduced in the campaign
- teaches devices, surveys, calibration, and stronger forecast-driven planning
- should only arrive after the player already understands labor basics and plant-side basics

Basic tutorial task types:

- deploy one `Precision Irrigation Line`
- place one `Solar Shade Rack`
- operate one `Checkerboard Deployment Machine`
- scan one risk zone
- maintain one technical support line through a weather window

Normal task bias:

- precision irrigation deployment
- solar-shade support placement
- checkerboard mechanized deployment
- surveys
- calibration
- device optimization
- forecast-driven sustain jobs
- high-precision experiment tasks

Assistant:

- `Device Upgrade Support`
- immediate sample reward should feel technical: one temporary upgrade module, improved forecast window, or boosted device efficiency the player must capitalize on now

Forecast identity:

- the university should be the main source of advanced weather forecasting
- its upgrades should progressively improve `Heat`, `Wind`, and `Dust` forecast detail rather than jumping straight to perfect certainty
- its information advantage should help the player prepare the right response before a hazard window, not erase hazard tension completely

Random-event flavor:

- field trial grant
- research shipment
- survey bonus
- calibration drift
- failed experiment

Device identity in current scope:

- `Precision Irrigation Line`
- `Solar Shade Rack`
- `Checkerboard Deployment Machine`
- in the current design, these should be university-gated `Site Unlockables`, university task rewards, or university branch unlocks rather than neutral equipment shared equally across factions
- the player should feel that the university's advantage is not abstract science alone, but visible field infrastructure that changes one patch's survival, efficiency, or preparation quality right now

Tech-branch rough themes:

- precision irrigation efficiency
- solar-shade microclimate support
- faster checkerboard deployment
- better sensors
- stronger device efficiency
- improved forecast precision
- visible `Heat`, `Wind`, and `Dust` weather meters
- clearer main-threat forecast diagnosis
- better draft quality control
- plant-device synergy

Later demo / full-game expansion direction:

- deeper university water-tech can expand into `Deep Well Pump`, brackish-groundwater use, pretreatment, and salt-reduction treatment
- realistic treatment directions include membrane-based desalination, electrodialysis, and other brackish-water processing methods
- this branch is intentionally out of current scope because it adds too much simulation, maintenance, and balance work for the first playable build

Weakness / tradeoff direction:

- strongest optimization branch
- usually asks for more setup and knowledge before the reward is felt
- can feel weaker than the other factions if the player is still barely surviving

`Site Task` goals mostly fall into three categories:

- Restore: establish target plant cover, reduce erosion, or stabilize designated tiles.
- Sustain: keep a site viable for a period under weather pressure.
- Deliver: provide requested ecological output, plant stock, or site performance results.

`Site Task`s can include optional bonus objectives for extra funding or special rewards.

Each `Site Task` can reward money, `Reputation`, `Faction Reputation`, or a mix of these. This allows jobs to pull the player toward different priorities: short-term site survival, aggressive local scaling, or long-term faction progression.

The `Contract Board` should be one of the main replayability engines. Variation should come from:

- Different mixes of restore, sustain, and deliver tasks
- Different site targeting
- Different faction publishers, assistant hooks, and relief implications
- Different deadlines and risk profiles
- Optional objectives that reward aggressive or cautious play
- Reward packages that support different strategies
- Variable payback values so the best short-term money choice is not always the best long-term ecological or faction-alignment choice
- Different mixes of cash payout, fixed `Reputation`, and fixed `Faction Reputation` across tasks

The intent is to create the same kind of run-to-run strategic divergence seen in strong sandbox strategy games: the player adapts to the board they get instead of repeating one solved build order every campaign.

The task board is the main structured motivation layer of a site run. It should not be the only source of momentum. The faster short-term layer should come from the combination of `Site Task`s, `Site Unlockables`, and `Run Modifier`s.

### Site Tasks

`Site Task`s are short 5 to 10 minute objectives generated from the current `Site` state. They appear in a live task pool inside the `Contract Board`. Their job is to keep the player moving from one reachable payoff to the next while the site is still unrestored.

In the current faction model, `Site Task`s should feel like field assignments drawn from a live board rather than timeless side chores. The player should always be able to see which `Faction` published the task, whether they still have free acceptance slots, and what kind of long-term relationship or assistant support the task advances.

Onboarding-specific board staging, featured-faction sites, `Tutorial Task Set` behavior, and early accepted-task-cap rules are defined in Chapter 13. The rules below describe the general task-board model that the game should settle into once those onboarding gates are cleared.

Key rules:

- The `Contract Board` shows a limited concurrent pool of available `Site Task`s
- This `Task Pool Size` can be upgraded through the `Persistent Tech Tree`
- Fully unlocked board composition should start with `2` visible tasks per active `Faction`; with `3` factions this means `6` visible tasks on the normal starter board
- The fully unlocked board should also have an `Accepted Task Cap` of `3`
- Every `Site Task` should show its publishing `Faction`
- The task pool should refresh on its own periodic cadence
- The player must explicitly accept a task to commit one of their acceptance slots
- Accepted tasks move into an active list and do not rotate out on refresh
- Unaccepted tasks remain on the board and may rotate out when the pool refreshes
- The game should not allow free task abandonment, because accepting a task is itself the strategic commitment
- If an escape hatch is needed, use one `Emergency Withdrawal` per site session at most; it should reduce only the current site's already-earned trust with that task's publisher and should never subtract previously banked reputation from earlier sites
- Faction-published tasks should bias reward drafts, `Faction Reputation` gain, and `Faction Assistant` availability
- Completing a `Site Task` should always grant its guaranteed `Faction Reputation` payout from the publishing `Faction`
- That `Faction Reputation` payout should be guaranteed on completion, but its amount should scale by task tier or level and task type rather than being chosen from the reward draft
- A good task choice should depend on current tech build, short-term reward need, active faction events, expected assistant value, current forecast risk, and how many acceptance slots are already committed
- Tasks should often emerge from live site problems such as a regressing plant cluster, buried irrigation line, failing windbreak, or vulnerable harvest patch
- When a refresh happens, some tasks can rotate out and new ones can appear
- The newly generated tasks may draw from current site context such as terrain, weather, weak zones, current plants, available tech, and currently accepted tasks
- Each refresh should also generate a small number of `Task Chain`s when possible
- If a chain step is left unaccepted on the board, it may rotate out on refresh and break the remaining chain opportunity
- The player should feel encouraged to complete accepted tasks quickly, because finished tasks free acceptance slots and let the next commitment matter
- Because tasks differ in difficulty and reward shape, players can naturally self-select the easier ones or the ones rewarding what they currently want
- Every `Site Task` should have a `Task Tier`
- Higher `Task Tier`s should spawn less often, be harder to complete, and produce stronger `Task Reward Draft` quality
- The highest-tier tasks should feel like jackpot spawns: rare enough to be exciting when they appear in a refresh, and hard enough that completing them feels memorable
- Jackpot-tier tasks should often grant or offer a `Run Modifier`, not just a bigger resource reward
- Tasks should feel local and practical, not abstract checklist filler
- Completing a `Site Task` should first award its guaranteed `Faction Reputation` gain, with the amount based on task tier or level, then reveal one `Task Reward Draft`
- Completing a `Site Task` should reveal one `Task Reward Draft`
- A `Task Reward Draft` should usually contain `3` or more options
- A `Task Reward Draft` should not be required to contain a `Site Unlockable`; some drafts should instead focus on modifiers, money, resources, or mixed tactical bundles
- Draft options can include:
- a `Site Unlockable` such as a plant, device, tool, or other site-scoped option
- a `Run Modifier`
- a `Task Reward Package` containing money, resources, or a mixed tactical bundle
- if the chosen option is a `Site Unlockable`, it becomes available on the current site and may still require money to purchase or use
- if the chosen option is a `Run Modifier`, it activates immediately for the current site session only
- if the player leaves, fails, or restarts the current site session, all active `Run Modifier`s from that session are lost
- `Site Unlockable` availability should primarily depend on `Site Task` completion plus `Persistent Tech Tree` prerequisites, but the site should also offer a limited direct-purchase fallback for tech-eligible unlockables at very high money cost

### Task Tiers (Temp Design)

This tier model is temporary design scaffolding and should be refined later through balance testing.

| Tier | Spawn Rate | Challenge | Reward Draft Quality | Reward Feel |
|---|---|---|---|---|
| `Level 1 Routine` | Common | Straightforward, low-risk local work | Basic `3`-option draft quality | Small but useful choices |
| `Level 2 Standard` | Common | Moderate field effort or timing | Solid `3`-option draft quality | Useful unlock, modifier, or resource choices |
| `Level 3 Priority` | Uncommon | Demands stronger planning or site setup | Better draft quality and better option mix | Clearly valuable mixed choices |
| `Level 4 Elite` | Rare | Difficult, often weather-sensitive or layout-sensitive | High-quality draft with stronger unlockables, premium bundles, or modifier access | Large upside with stronger strategic pivots |
| `Level 5 Jackpot` | Very rare | Hard to spawn, hard to complete, and risky under site conditions | Top-end draft quality with exciting site-changing choices | Outstanding choices plus strong `Run Modifier` potential intended to create a real excitement spike |

Reward scaling should feel obvious. A higher-tier task should not only be numerically better; it should look and feel more important when it appears in the pool.

### Jackpot To Run Modifier

The strongest jackpot outcomes should not only give bigger rewards. They should sometimes change the rules of the current site run in a way that makes the player want to pivot their strategy immediately.

Example `Run Modifier` directions:

- All plants gain greatly increased storm resistance for the rest of the current site session
- Water consumption is globally reduced on this site
- Wind direction becomes more stable and forecastable
- The next several `Site Unlockable` purchases are free or heavily discounted
- Irrigation efficiency is dramatically increased
- Contractor output is boosted during harsh-weather windows

Important rule:

- `Run Modifier`s last only for the current site session
- restarting, failing, or leaving that site clears them

The emotional target is not just "nice, bigger reward." It is "holy shit, this changes my whole run." That kind of spike creates stronger memory, stronger identity, and much higher excitement when a jackpot-tier task appears.

Run modifiers must avoid creating one obviously best outcome. A good modifier should feel powerful, but also situational, synergistic, or commitment-based rather than universally dominant.

Anti-dominant-strategy rules for modifiers:

- Strong modifiers should push a style, not solve every problem
- Modifier value should depend on site conditions, current tech, and player choices
- Some modifiers should be incredible in one run and merely solid in another
- Commitment bonuses are good; universal auto-picks are bad
- Players should be excited to see different modifiers, not only fish for one best one

Typical `Site Task` examples:

- Plant a short windbreak on an exposed edge
- Stabilize a high-erosion patch
- Deliver one small output bundle
- Build one support device
- Survive an incoming heat window with target shelter and hydration levels
- Harvest a first batch from a newly functional crop patch
- Reinforce a zone that is beginning to degrade

Typical rewards:

- Reveal a `Task Reward Draft` with `3` or more options
- Let the player choose 1 reward from unlockables, modifiers, or resource-focused bundles
- Always grant the guaranteed publisher `Faction Reputation` payout on completion before the choice is made; the amount should scale with task tier or level
- If the chosen result is a `Site Unlockable`, reveal it for current-site purchase or use
- If the chosen result is a `Run Modifier`, activate it immediately for the current site session
- One-time delivery drops or contractor offers when appropriate

The design goal is that the player can finish a `Site Task`, automatically gain trust with its publisher, receive a clear reward draft, and make one meaningful tactical choice right away. That single choice should still create strong tension: reveal a new plant or device, activate a site-wide modifier, or take immediate money and resources.

The `Task Reward Draft` should scale clearly with `Task Tier`. Higher-tier tasks should feel distinctly better, not just slightly larger.

### Task Chaining

`Site Task`s should not feel fully isolated. Each task-pool refresh should try to generate a small number of `Task Chain`s so some tasks are linked on purpose rather than only by coincidence.

Example chain directions:

- Stabilize a zone, then gain bonus speed on planting tasks in that zone
- Finish a shelter reinforcement task, then gain better odds or bonuses on storm-survival tasks
- Complete several tasks during one harsh-weather period, then receive a combo reward or boosted draft quality
- Finish a restoration preparation task, then get stronger output or ecology rewards on the follow-up planting task

Task-chain rules:

- each refresh should generate a small number of chains based on `Task Pool Size`
- chain tasks should be visibly linked in the `Contract Board`
- each task inside a chain should still give a normal task reward for its tier
- completing the full chain should grant an extra bonus reward on top of the normal task rewards
- that chain-completion bonus can be a stronger `Task Reward Draft`, a premium resource bundle, a fixed extra `Reputation` or `Faction Reputation` payout, or better `Run Modifier` odds

Task chaining creates momentum. The player stops thinking "one task" and starts thinking "if I finish this now, the next two tasks get better, the whole chain pays extra, and I free one of my three acceptance slots." That is exactly the kind of pressure that turns "one more task" into a longer, satisfying session.

### Funding Loop

All money-related actions are performed through the `Field Phone`. This keeps the economy diegetic and personal: the player checks jobs, spends money, sells output, and hires labor through the same in-world tool they carry into the field.

The main economic inputs are:

- `Site Task` rewards
- Field output from stable plants
- Selling approved goods through the `Field Phone`
- Selling surplus electricity through the `Field Phone` after local utility demand is already covered

The main economic outputs are:

- Buying water
- Buying solar or support devices
- Buying food or medicine
- Hiring contractors
- Purchasing seeds, parts, or specialized tools
- Purchasing revealed `Site Unlockables` on the current site

Task rewards should vary enough to change decision-making between runs. Some campaigns should tempt the player with better short-term cash, while others reward plant progression, utility access, or safer expansion. Payback randomness should reshape priorities, not invalidate planning.

Sell-value hierarchy:

- crafted value-added goods should sell for the highest prices
- normal harvested goods should sit in the middle
- exported surplus electricity should sit clearly below harvest value and mainly function as supplemental income from a strong solar setup

Money is the short-term tactical currency. It is primarily for surviving and scaling the current `Site`. If the player spends money on a revealed `Site Unlockable` or on a premium direct-purchase unlockable, that money is not available for water, labor, or emergency recovery on the same site.

`Site Task`s should always grant guaranteed publisher `Faction Reputation` on completion, with the amount scaling by task tier or level, then offer some mix of `Site Unlockables`, `Run Modifier`s, money, or resources through deliberate `Task Reward Draft` choice. Not every draft needs to surface an unlockable. Their main job is still to create momentum, reveal fitting local options, and point the player toward useful next actions. Direct cash should come mainly from tasks, selling output, and other larger economic flows.

### Site Unlock And Modifier Loop

The main per-site engagement loop should be:

- Complete a `Site Task`
- Gain the task's guaranteed publisher `Faction Reputation`, with the amount based on task tier or level
- Receive a `Task Reward Draft`, typically with `3` or more choices
- Choose `1` reward option from whatever that draft offers, which may include unlockables, modifiers, or resource-focused bundles
- If the chosen result is a `Site Unlockable`, spend money to purchase or use it when needed
- If the wanted unlockable did not appear from tasks, optionally buy it from the direct-purchase list at very high money cost
- If the chosen result is a `Run Modifier`, let it reshape the current site session immediately
- If the chosen result is a resource bundle, use it to stabilize or accelerate the site right away
- Use that new option to increase output, site safety, or ecological stability
- Earn more money through better survival, better production, and larger economic flows
- Complete more local tasks that reveal the next set of desirable reward choices

This loop should be one of the strongest "one more step" drivers in the game. It gives the player a simple and repeatable reason to keep going during a site run.

The important design principle is that site progression should not only offer passive stats. It should frequently reveal something the player actively wants to use right away, such as:

- A new plant family or plant variant
- A new device or support structure
- A stronger irrigation or shelter option
- A better tile treatment tool
- A more efficient harvesting or hauling method

That way, task completion naturally creates the next target: the player now wants to purchase the newly chosen unlockable, place the new plant, test the new tool, use the new structure, or exploit the new modifier window.

To avoid frustration, the player should always have multiple visible `Site Task`s in the pool, and each completed task should present a meaningful but limited draft rather than a single forced result. This keeps local site progression task-driven without becoming overly rigid.

The mixed reward draft creates the planning layer. A player who is short on cash may choose money, a player preparing for expansion may choose resources, and a player chasing a stronger local build may choose a plant, device, or modifier instead. The faction relationship gain happens automatically, so the player is not asked to choose between strategic identity and immediate usefulness.

Rare high-tier tasks add a third emotional layer: jackpot anticipation. When the pool refreshes, the player should feel that there is always a chance of seeing something unusually valuable and exciting.

Rare jackpot modifiers should also help shape eventual `Site Commendation` outcomes. A site finished under strong storm-defense, low-water efficiency, or rapid economic scaling should later be recognized for that style of success rather than feeling like just another generic completion.

### Short-Term Reward Cadence

If the game needs to feel more compelling and hard to put down, it should constantly offer the player small reachable targets with fast feedback. The right model is not random reward spam, but a layered cadence of short, medium, and long goals.

Short-term targets should usually be visible within the next few minutes of play. In this design, they should come mainly from four sources:

- The current `Site Task` that will reveal the next `Task Reward Draft`
- The current revealed `Site Unlockable` the player is trying to afford or use
- The current refreshable pool of visible `Site Task`s
- The next visible ecology, survival, or zone threshold on the current `Site`

Good examples include:

- Finish one windbreak line
- Stabilize one high-risk tile cluster
- Survive the next heat window
- Rescue one withering patch before it dies
- Clear one buried support line before the next storm
- Complete one `Site Task`
- Harvest one output patch
- Earn enough money to purchase a newly revealed `Site Unlockable`
- Reach the next site ecology threshold

Recommended short-term reward sources:

- Newly revealed `Task Reward Draft`s
- Newly revealed or newly discounted chosen `Site Unlockables`
- Chosen `Task Reward Package`s containing money, resources, or mixed tactical bundles
- Rare high-tier `Site Task`s appearing in the refreshed pool
- Temporary discounts or purchase windows for site unlocks
- Immediate morale or safety improvement when a plant cluster becomes functional

The design goal is for the player to almost always have one obvious next step that feels achievable and worthwhile. As long as the current `Site` is not fully restored, the game should keep generating fresh small targets from the site's current state, forecast, task pool, and ecology thresholds.

The task-pool refresh should also create anticipation between completions. Even if the player is not ready for the hardest current task, they should feel some excitement about checking the next refresh in case a valuable high-tier opportunity appears. The emotional pressure should come from wanting to free acceptance slots and improve the board, not from fearing arbitrary expiration on already-accepted work.

### Cognitive Load Guardrails

This game has many overlapping systems: tasks, chains, reward drafts, unlockables, survival meters, weather, economy, modifiers, and regional progression. That depth is a strength only if the player can parse the current decision space quickly.

Core cognitive-load rules:

- The player should never need to process every system at once
- The main solution to overload should be `Concept Unlock`s
- In onboarding, a system the player cannot yet act on should usually remain actually locked

At any given moment, the player should mainly understand:

- what is immediately dangerous
- what task is easiest or most valuable right now
- how many acceptance slots are already committed
- what newly revealed unlockable, modifier, or reward choice matters next
- what larger site or campaign goal they are currently pushing toward

If the design cannot present that clearly, depth will feel like clutter instead of mastery.

### Target Ladder Structure

To keep players engaged without relying on pure randomness, the game should present goals at multiple layers at the same time:

- Immediate target: a 1 to 3 minute task such as finishing one tile cluster, placing one missing support plant, buying one needed supply, or surviving the next danger window
- Short target: a 5 to 10 minute task such as finishing a windbreak line, completing one `Site Task` to reveal a reward draft, choosing one local unlock or modifier, or harvesting a patch
- Site phase target: a 10 to 20 minute task such as stabilizing a zone, securing food independence, making the camp storm-safe, or hitting the next ecology threshold
- Campaign target: a longer objective such as restoring a linked corridor, raising `Reputation` to the next tier, unlocking a key `Persistent Tech Tree` node, or opening a new strategic site

The player should usually see all four layers at once. This creates the feeling that there is always something close to completion, even when the overall campaign is long.

### Milestone Structure

To support this cadence, larger goals should have milestone support, but the game should not depend entirely on hand-authored decomposition of every big objective. Much of the small-target flow should emerge naturally from `Site Task`s, `Site Unlockable` desires, modifier opportunities, and visible site thresholds.

Useful milestone layers:

- Phase milestone rewards: each major site phase should have 2 to 4 small sub-objectives with immediate payouts
- `Site Task` completion rewards: the primary 5 to 10 minute target-and-reward layer, centered on revealing a `Task Reward Draft` and choosing `1` unlock, modifier, or resource reward rather than just taking raw cash
- Zone restoration thresholds: when a local area crosses a restoration threshold, it grants a burst reward
- Daily field goals: each morning can surface a few lightweight optional objectives tied to current weather and site state
- Plant chain milestones: first windbreak, first stable food patch, first storm-proof cluster, first medicinal harvest
- Survival recovery milestones: reaching safe water, safe shelter, or stable energy before a forecasted event

This structure supports the "one more task" feeling because the player is often very close to another payout, unlock, or visible site improvement.

### Site Restoration Phase Targets

Each `Site` restoration should be broken into phases, and each phase should refresh the target pool instead of simply asking for one giant completion bar.

Recommended site phases:

- Emergency foothold: secure water, shelter, and a first safe work zone
- Defensive setup: establish windbreaks, erosion control, and enough resilience to survive bad weather
- Functional recovery: create stable plant clusters, basic output, and more reliable camp operations
- Expansion and optimization: connect restored zones, improve efficiency, and pursue higher-value tasks
- Restoration certification: reach the site's `Fully Grown Tile` threshold needed for the site to count as completed

Each phase should introduce new small targets that fit the current condition of the `Site`, so progress feels like a sequence of satisfying local wins rather than one long flat grind.

Extreme hazard events should serve as phase punctuation in some site runs. A site might feel manageable for a while, then suddenly demand a high-stakes survival response that tests whether the player's current phase of development is truly robust.

### Campaign Target Layer

The campaign should use the same philosophy at a larger scale. A restored `Site` should not only close a chapter; it should create fresh nearby targets and new strategic reasons to keep going.

Recommended campaign-level target sources:

- Restore linked site chains or ecological corridors
- Hit government program milestones for `Reputation`
- Unlock region-wide bonuses by restoring clusters of adjacent sites
- Respond to temporary regional opportunities such as funding pushes, policy drives, or rare weather windows
- Use remaining `Campaign Clock` time efficiently to complete as many sites as possible

This keeps the player moving between local urgency and larger ambition.

### Other Engagement Suggestions

To make the game more compelling without feeling manipulative, these systems would fit well:

- Streaked ecological feedback: let connected restorations create visible chain reactions, such as a whole edge becoming calmer after one more plant line is finished
- Commitment bonuses: some combinations of `Site Unlockables`, modifiers, and task choices should reward leaning harder into one style rather than always broadening out
- `Site Commendation`s: reward successful site completions with a government prize that names the achievement reason, such as zero-collapse restore, low-water restore, or storm-proof restore, with optional `Reputation`, prestige, or future bonuses
- Optional post-restore tasks are removed from the current design to avoid adding revisit complexity before the regional loop is fully understood

### Reputation Loop

`Reputation` is separate from money. In the current faction model, campaign trust now has two linked layers:

- `Reputation`: total campaign standing with the restoration program, earned progressively from official work and never spent
- `Faction Reputation`: cumulative standing with each individual `Faction`, earned mainly from that faction's `Site Task`s, faction events, and aligned follow-through

`Reputation` should function as thresholded trust, not consumable currency. Reaching a threshold unlocks access to new tech levels and support tiers; choosing a tech node does not reduce current reputation. `Faction Reputation` should behave the same way inside each branch.

For clarity, `Faction Reputation` from `Site Task`s should usually be a guaranteed completion payout tied to the task's publisher, with the amount scaling by task tier or level, while the post-task draft should stay focused on immediate tactical rewards.

If a current-site penalty such as `Emergency Withdrawal` exists, it should only reduce trust earned during the current site session before that session's gains are folded into the persistent totals. It should never spend or erase trust already banked from previous sites.

This keeps campaign progression distinct from site survival, preserves the satisfaction of steady institutional growth, and turns task choice into a strategic identity decision rather than only a payout decision.

### Loadout

Before entering a site, the player chooses a `Loadout`. In the current design, the loadout should not be bought freely before deployment. Instead, it is assembled on the `Regional Map` from `Regional Support Output` provided by nearby stabilized sites.

Loadout rules:

- only nearby stabilized sites may contribute loadout support for a new deployment
- "nearby" should mean directly adjacent sites on the `Regional Map` in the current design
- each eligible nearby site contributes a limited exportable package every few regional days
- the player chooses which available nearby-site support to bring into the new run
- nearby-site support packages and passive modifiers should be available before deployment
- `Persistent Tech Tree` upgrades may increase loadout capacity or improve how efficiently nearby support is used
- on-site money spending still exists after deployment through the `Field Phone`, but pre-deployment loadout should come from nearby support rather than open purchase

In the current design, a loadout can include:

- Starting food and recovery supplies
- Seed stock
- Basic tools
- Utility devices
- Material bundles such as parts, repair kits, or cover rolls
- Small starting protection bonuses from nearby-site spillover

Important water rule:

- water is not a generic regional output from plant productivity
- the normal water source remains on-site buying or direct site features such as an oasis or lake
- future special-case oasis sites may grant unique water-related support, but this should not be part of the generic support model
- each new deployment should begin with a small fixed water amount so the opening minutes remain fair
- that fixed water amount is baseline deployment support, not `Regional Support Output`
- if the player needs more water, they must either earn money and buy it on-site or extract it from an oasis or lake when the site allows that

Loadout size and quality depend on the `Persistent Tech Tree` and nearby support. This gives the campaign a meaningful preparation phase before each deployment and makes regional site order strategically important.

### Regional Support Output

`Regional Support Output` is the system that makes restored nearby sites matter before the player enters the next site.

#### Long-Term Support Category List

Longer-term, nearby-site support can include categories such as:

- Seed Support
- Material Support
- Camp Support
- Forecast Support
- Logistics Support
- Recovery Support
- Ecology Support
- Emergency Reserve

Scope note:

- for the first playable, only activate `Resource Loadout` output plus small ecological spillover bonuses
- ecological spillover should currently focus on wind protection, heat protection, and fertility support

#### Regional Support Rules

Use these temporary rules:

- only stabilized sites adjacent to the target site contribute support
- each contributing site should produce a small exportable support package every `3` regional days
- each contributing site may provide `Resource Loadout Output` for pre-deployment assembly
- each contributing site may also provide passive ecological spillover on the new site, especially wind protection, heat protection, and fertility support
- these bonuses should be noticeable but deliberately small; they should help the player start better, not solve the new site

#### Composition-Based Output Rule

Do not force a site into only one dominant support type. A restored site should export support based on the full composition of what the player built there.

Interpretation:

- a site's support output should depend on its certified completion state plus its plant composition
- only medium-density and high-density plant tiles should contribute meaningfully to regional output
- different plant roles should contribute to different support channels at the same time
- this means one site can export some resources, some wind protection, some heat protection, and some fertility support together

#### Temporary Productivity Rule

For implementation, use a simple tunable rule rather than final balance math.

Recommended temporary model:

- compute a `siteYieldScore` from the restored site's healthy plant yield
- multiply that score by temporary tuning parameters to derive exported support strength
- tune those parameters later through testing

Engineering direction:

- each contributing plant tile adds a small `siteYield` contribution based on plant type and current density
- sum those contributions into one `siteYieldScore`
- derive support channels from that total using role-weight multipliers rather than one dominant-site label

Recommended channel examples:

- `resourceLoadoutOutput = siteYieldScore * resourceLoadoutParam`
- `regionalWindProtection = siteYieldScore * protectionParam * protectionCompositionFactor`
- `regionalHeatProtection = siteYieldScore * antiDehydrationParam * antiDehydrationCompositionFactor`
- `regionalFertilitySupport = siteYieldScore * fertilizeParam * fertilizeCompositionFactor`

This keeps the system simple:

- output scales with how successful the site is
- plant composition still matters
- tuning remains easy because the parameters are explicit and temporary

#### New-Site Advantage Limits

Regional support should create a meaningful edge, but it must not trivialize new-site survival.

Use these limits:

- `Resource Loadout Output` should improve the opening phase, not replace the early survival scramble
- wind and heat protection bonuses should feel like a small safety cushion, not full hazard immunity
- fertility spillover should help one early patch establish more reliably, not make barren land fully solved
- stronger harsh sites should still require preparation, adaptation, and good local strategy even if nearby support exists

#### Site Output Modifiers

Some sites can be made strategically higher-priority by giving them a persistent `Site Output Modifier`.

Modifier directions:

- increase exported wind protection by `x%`
- increase exported heat protection by `x%`
- increase exported fertility support by `x%`
- increase one specific resource output by `x%`
- increase regional support effect range by `x%`

Stacking note:

- the number of `Site Output Modifier`s on a site should be tunable
- most sites should have `0-1` meaningful modifier in the current design
- especially strategic or rare sites can have more, but the system should avoid stacking so many traits that the next site becomes solved automatically

#### Session-Shaping Synergy Rule

Nearby-site bonus traits should actively influence how the next site session is played.

Interpretation:

- if a nearby stabilized site exports increased wind protection, the player should feel encouraged to lean harder into wind-protection plants or layouts on the new site
- if a nearby stabilized site exports increased fertility support, the player should feel encouraged to invest earlier into fertilize-focused expansion
- if a nearby stabilized site exports a boosted resource category, the player should feel pushed toward using that stronger opening more aggressively

This is desirable. The point is not only to make the next site slightly easier. The point is to make certain nearby sites strategically important because their bonus traits reshape the best opening approach on adjacent harsh sites.

Design intent:

- these modifiers make certain sites more strategically valuable than others
- they encourage players to think about regional operation order
- a player may restore easier nearby sites first to build up support before attempting a much harsher site
- a site with a strong modifier can become a high-priority strategic target even if its immediate local rewards are not the biggest
- bonus traits should create meaningful synergy with plant composition and opening strategy, not just flat background bonuses

### Global Progression

What persists globally across the campaign:

- `Reputation`
- `Faction Reputation`
- `Persistent Tech Tree` unlocks and chosen faction-tech nodes
- Plant unlocks and plant upgrades
- Gear unlocks
- `Regional Support Output` from stabilized sites

What mostly resets when entering a new site:

- Local funds
- Local consumables
- Temporary labor
- Immediate camp layout
- Most local stockpiles
- The current site's unrevealed unlock pool, revealed `Site Unlockables`, and active `Run Modifier`s

This creates a clear distinction between campaign progress and site-level survival.

### Replayability Progression Model

High replay value depends on making progression choices meaningfully different across campaigns. The game should support:

- Different early-game loadout priorities
- Different regional expansion orders
- Different plant-focused versus infrastructure-focused runs
- Different faction allegiances, assistant timings, and harsh-event relief profiles
- Different risk tolerance based on task-board offers and weather patterns
- Different support-network shapes based on which sites stabilize first
- Different `Site Unlockable` draws, modifier combinations, and site-specific upgrade paths
- Different cash-versus-trust task economies across runs

### Persistent Tech Tree

The `Persistent Tech Tree` is the long-term campaign progression layer. It lasts for the whole campaign and shapes the player's durable capabilities and strategic identity.

In the current faction model, this layer is split into three named branches under one shared program umbrella. It is unlocked through cumulative `Reputation` thresholds plus branch-specific `Faction Reputation` requirements, not money.

| Faction | Strategic identity | Assistant | Example branch themes |
|---|---|---|---|
| `Village Committee` | Tempo, labor flexibility, emergency cleanup, task throughput | `Workforce Support` | extra contractor capacity, faster repairs, lower hauling friction, deadline forgiveness, stronger chain completion |
| `Forestry Bureau of Autonomous Region` | Ecological resilience, conservative expansion, harsh-event endurance | `Plant-Water Support` | stronger seedling survival, irrigation efficiency, erosion control, density protection, post-event replanting |
| `Autonomous Region Agricultural University` | Precision, research, forecasting, device-led optimization | `Device Upgrade Support` | precision irrigation, solar-shade support, checkerboard mechanization, better sensors, stronger device efficiency |

Unlock rule:

- `Reputation` unlocks tech levels progressively across the campaign
- each newly reached `Reputation` threshold grants a limited number of `Faction Tech Picks` for that level
- each node also requires a matching `Faction Reputation` amount inside its branch
- choosing a node does not consume either reputation value; the meaningful scarcity is which picks the player commits to over the campaign
- higher levels should demand materially more `Reputation` and `Faction Reputation`, so high-end nodes belong to players who consistently invest in that relationship

This model preserves the feeling of steady institutional growth while still forcing build choice. The player earns trust progressively, but cannot take every powerful node in the same campaign.

It focuses on:

- Worker protection
- Plant unlocks
- Plant upgrades
- Better starting loadout options
- `Task Pool Size` increases and better `Site Task` quality
- Irrigation tools
- Sensors and forecasting
- Shelter efficiency
- Crafting support
- Site utility devices

Branch emphasis should be readable:

- `Village Committee` leans toward work tempo, delivery flow, emergency repair, and pragmatic field logistics
- `Forestry Bureau of Autonomous Region` leans toward plant survival, water efficiency, anti-erosion defense, and event recovery
- `Autonomous Region Agricultural University` leans toward visible technical infrastructure such as precision irrigation, solar-backed microclimate support, checkerboard deployment, sensors, and precision planning

This tree should improve planning and resilience without erasing the desert's danger. It should also create meaningful long-horizon tradeoffs. The player should not unlock everything quickly enough for every campaign to converge into the same build.

### Site Unlockables And Run Modifiers

`Site Unlockables` are the site-only progression layer. Every time the player enters a `Site`, a fresh random pool of local options is generated. That pool is built only from content already enabled by the `Persistent Tech Tree`, and it resets when leaving, failing, or restarting the site.

This system should work like a curated random reward pool, not a fully fixed tree and not pure chaos. Each site should offer a different subset of plants, devices, tools, and other temporary options, which means the player's short-term priorities and local build style naturally change from site to site.

The main acquisition path should be task rewards, but task rewards should not be the only path. If the player wants a specific local unlockable and the relevant task drafts do not offer it, the site should also expose a limited direct-purchase fallback for tech-eligible unlockables at very high money cost.

Faction tech choices should bias these local offers rather than replacing them. A `Village Committee` build should surface more labor and tempo tools, an `Autonomous Region Agricultural University` build should surface more device and precision options, and a `Forestry Bureau of Autonomous Region` build should surface more plant and water-resilience options.

Task-driven and money-fallback access rules:

- completing a `Site Task` reveals a `Task Reward Draft`, typically with `3` or more options
- the options can include `Site Unlockables`, `Run Modifier`s, or resource-focused rewards
- not every `Task Reward Draft` should contain a `Site Unlockable`
- if the player chooses a `Site Unlockable`, it becomes available on the current site and may still require money to purchase or use
- the site should also maintain a small direct-purchase list of tech-eligible `Site Unlockables`
- direct-purchase prices should be deliberately very high so task-earned unlocks remain the efficient path
- the purpose of direct purchase is to prevent reward RNG from completely blocking a desired local build, not to replace task-driven progression
- if the player chooses a `Run Modifier`, it activates immediately and lasts only for the current site session
- if the player restarts, fails, or leaves that site, active `Run Modifier`s are lost and a fresh site reward pool is generated on the next attempt

Because revealed unlockables still compete with water, food, tools, devices, and contractor spending, local progression decisions remain real survival tradeoffs instead of free progression.

The design goals of this system are:

- Make each site feel tactically different even within the same campaign
- Add adaptation pressure to local terrain and weather
- Prevent site play from becoming fully scripted
- Increase replay value without deleting long-term campaign progress
- Stay flexible during early development so new unlockable categories can be added later without breaking the model

`Site Unlockables` should favor temporary or site-scoped options such as:

- New plants or temporary access to plant families suited to the current site
- New devices and support structures
- New tools or field actions
- Local work-efficiency improvements
- Temporary planting bonuses
- Faster construction or hauling
- Better tile treatment efficiency
- Local irrigation or sensor improvements

`Run Modifier`s should favor session-wide state changes such as:

- Better selling price on the current site
- Better worker efficiency
- Faster plant growth
- Lower water consumption
- Better storm resistance
- Better contractor productivity

Together, the `Persistent Tech Tree`, `Site Unlockables`, and `Run Modifier`s should support both long-term campaign identity and short-term site improvisation. Persistent tech defines what kind of restoration worker the player becomes. Site unlocks and modifiers define how that worker adapts during the current site session.

### Site Commendation

When the player successfully restores a `Site`, the government should award a `Site Commendation`. This turns success style into explicit recognition rather than leaving it as only an internal flavor label.

`Site Commendation` rules:

- every successfully restored `Site` should grant one commendation
- the commendation should contain a title plus a cited reason for the award
- the cited reason should reflect how the site was restored, not just that it was restored
- commendations should be recorded in campaign history so the player can look back at what they achieved

Commendation reason directions:

- `Storm-Proof Recovery`: restored while surviving major hazard pressure with low collapse
- `Low-Water Restoration`: restored with unusually efficient water use
- `Rapid Stabilization`: restored quickly relative to site difficulty
- `Clean Recovery`: restored with low plant death or low restart pressure
- `Output-Led Recovery`: restored while also sustaining strong economic output

Design intent:

- the player should feel formally recognized, not just numerically rewarded
- the commendation should make site completion feel memorable
- modifiers, unlock choices, task priorities, and recovery style should all be able to influence which commendation reason is earned
- commendations can later become a source of prestige, `Reputation`, or secondary campaign bonuses if early testing shows that extra reward is useful

### Random Events

Random events add situational variety on top of the core simulation. They should be grounded in the setting and tied to survival, logistics, weather, or policy rather than combat spectacle.

In the current faction model, a large share of random events should be faction-scoped rather than fully neutral. The board should feel different when the `Village Committee` is pushing practical labor opportunities, the `Autonomous Region Agricultural University` is pushing research goals, or the `Forestry Bureau of Autonomous Region` is reacting to ecological damage.

| Faction | Favorable event direction | Tense event direction | Likely gameplay effect |
|---|---|---|---|
| `Village Committee` | Volunteer push, local delivery convoy, community repair day | labor shortage, road delay, local budget diversion | changes workforce cost, repair tempo, and short-deadline task density |
| `Forestry Bureau of Autonomous Region` | sapling allocation, emergency irrigation dispatch, erosion-control campaign | nursery shortage, water rationing, failed planting batch | changes plant stock, water support, and harsh-event recovery options |
| `Autonomous Region Agricultural University` | field trial grant, research shipment, survey bonus | failed experiment, calibration drift, academic audit | changes device quality, forecast precision, and research-task value |

Neutral event categories can still include:

- Sudden equipment failure
- Temporary funding opportunity
- Unexpected weather shift
- Emergency water shortage
- Neighboring site support disruption

Events should create pivots and memorable stories, but they must remain readable and fair. High `Faction Reputation` should skew event rolls toward stronger favorable follow-through from that faction, while low reputation should still create opportunities but with weaker support or harder asks. Their purpose is to create unique campaign texture, not to override strategy.

### Harsh-Event Faction Relief

After a harsh event enters `Aftermath`, factions should evaluate whether to offer help based on current `Faction Reputation`, recent task history, and the site's current damage profile.

Rules:

- each of the three factions rolls separately for a possible `Aftermath Relief Offer`
- higher `Faction Reputation` improves offer chance, response speed, package strength, and subsidy level
- relief should be a strategic choice, not an automatic bailout; accepting one offer can weaken or lock out another during the same aftermath window
- relief should solve part of the recovery knot, not erase it

Relief directions:

| Faction | Relief style |
|---|---|
| `Village Committee` | temporary work crew, debris clearing, repair labor, emergency transport |
| `Forestry Bureau of Autonomous Region` | sapling batch, water cart delivery, mulch or erosion mats, density-rescue package |
| `Autonomous Region Agricultural University` | device recalibration, emergency diagnostics, short-term efficiency buff, survey-driven recovery advice |

This makes reputation matter even when the run is going badly. A player with stronger institutional relationships should recover from a severe event more cleanly than a player who ignored those ties.

## 13. Onboarding And Tutorial Flow

This chapter centralizes onboarding, tutorial gating, early board exposure, and learning-budget rules. Other chapters may reference these rules, but they should not redefine the full onboarding flow.

### Onboarding Philosophy

The first 30 minutes are critical. This design has high systemic depth, so early onboarding must be staged very carefully. The player should feel capable and curious, not flooded.

Core philosophy:

- The player should access the minimum number of gameplay concepts needed to play well right now
- New concepts should be unlocked one by one through actual play, not mainly through tutorial text
- A concept should usually unlock only after the player has used the previous concept successfully at least once
- The first three site games should function as a controlled onboarding arc
- The first site should not begin with the fully unlocked current rule set, even if later sites do
- During onboarding, "not yet relevant" should usually mean "not yet unlocked," not "already running but hidden somewhere"
- `Onboarding Task`s should introduce the biggest structural systems for the first time, while the `Persistent Tech Tree` may still introduce smaller new gameplay as long as the `Learning Budget` stays low

The onboarding goal is:

- not too slow, so the player feels momentum quickly
- not too complex, so the player understands the cause-and-effect of each layer before the next one arrives

The early campaign should teach the loop in pieces through real unlocks, not in one full-system dump.

### Task Board Tutorial Rules

The `Contract Board` should be part of onboarding, not something the player sees fully formed on the first minute of the first site.

Use these tutorial-stage board rules:

- the `Contract Board` itself should unlock only after the first faction `Onboarding Task`
- during onboarding `Site 1` to `Site 3`, the board should only publish tasks from the featured faction of that specific site
- multi-faction board comparison should begin on `Site 4`, after the player has learned all three faction styles in isolation
- during onboarding, board breadth and accepted-task count should be set first by onboarding stage, then later expanded further by `Persistent Tech Tree` upgrades
- each onboarding site's featured-faction board should use its own `Tutorial Task Set`
- while an onboarding `Tutorial Task Set` is active, that featured-faction board should not expire or refresh normally; it should stay stable until the player has completed all required basic task types in that set
- onboarding sites should keep `Accepted Task Cap = 2`
- the first full board should use all `3` factions, `6` visible tasks, and `Accepted Task Cap = 3`

These rules ensure the player learns what a faction board feels like before the game starts asking them to compare multiple factions at once.

### Early-Campaign Faction Unlock Arc

Use the first three site games as a controlled faction-unlock ladder, then use the fourth site as the first full-system proof site. Each of the first three sites should focus on one new faction and one new style of decision-making.

Recommended sequence:

1. `Campaign Start Baseline`
- Available concepts: movement, interaction, one danger meter, one immediate survival objective, basic camp setup, one basic plant or starter cover option, and one simple authored local objective
- Locked concepts: factions, assistants, faction tech branches, normal contract-board comparison, task chains, harsh-event relief, advanced forecasting, advanced plants and devices
- Player lesson: "I can survive and improve the site with a minimal toolset"

2. `Site 1: Village Committee Unlock`
- Use one authored `Onboarding Task` focused on practical labor, repair, hauling, or community coordination
- The `Contract Board` itself should unlock only after this first onboarding task is completed, during the same site game
- Unlocks on completion:
- `Village Committee` as the first permanent faction
- automatic `Village Committee` trust gain on future village tasks
- `Village Committee` branch visibility in the `Persistent Tech Tree`
- immediate one-time `Workforce Support` reward that should be used right away on the site
- `Contract Board Lite` with only `Village Committee` work
- a `Tutorial Task Set` of basic village tasks such as repair, hauling, and cleanup
- `Accepted Task Cap = 2`
- While this first village tutorial board is active, it should not refresh normally; it should remain stable until the player has completed every authored basic village task type once
- Player lesson: "This faction solves problems through labor tempo and practical field support"

3. `Site 2: Forestry Bureau of Autonomous Region Unlock`
- Start the site with `Village Committee` already known, then introduce one authored `Onboarding Task` about replanting, water support, erosion control, or seedling rescue
- Unlocks on completion:
- `Forestry Bureau of Autonomous Region` as the second permanent faction
- its tech branch visibility in the `Persistent Tech Tree`
- immediate one-time `Plant-Water Support` reward that should be used right away
- a forestry-only `Contract Board` slice with a forestry-flavored `Tutorial Task Set` covering the basic plant-side task types the player must learn before normal refresh behavior resumes
- `Accepted Task Cap = 2`
- first clear introduction to ecology-first recovery logic
- Player lesson: "This faction is about plant survival, water stability, and long-term resilience"

4. `Site 3: Autonomous Region Agricultural University Unlock`
- Start the site with the first two factions already known, then introduce one authored `Onboarding Task` built around survey work, calibration, devices, or field science
- Unlocks on completion:
- `Autonomous Region Agricultural University` as the third permanent faction
- its tech branch visibility in the `Persistent Tech Tree`
- immediate one-time `Device Upgrade Support` reward that should be used right away
- a university-only `Contract Board` slice and `Tutorial Task Set` that stays focused on university-style jobs for this site
- previously unlocked factions remain part of long-term progression, but the board still withholds cross-faction comparison here so the player can learn the university's style cleanly
- `Accepted Task Cap = 2`
- first fuller forecast/device-planning gameplay once the university forecast concept is introduced
- Player lesson: "This faction improves precision, devices, and planning quality"

5. `Site 4: Full Board Proof`
- Start the site with all `3` factions already unlocked
- The `Contract Board` now uses the full presentation with all `3` factions active, normal `6` visible tasks, and `Accepted Task Cap = 3`
- This is the first site where the player should make real cross-faction comparisons between labor, plant-water, and device-oriented tasks
- Harsh events should arrive in escalating waves so the player gets a full picture of preparation, response, recovery, and renewed commitment under pressure
- Player lesson: "The full game is about choosing between faction paths while repeated site pressure keeps changing what matters most"

6. `Post-Onboarding Normal Campaign`
- After the fourth site, the game should shift to the normal contract-board model
- All three factions can publish work
- Onboarding tasks are done; future complexity mainly comes from `Persistent Tech Tree` choices, stronger sites, and event pressure

This sequence matters because it gives the player a whole site to understand each faction's identity before the next one enters the decision space.

### Unlock Ownership Model

Use this rule consistently:

- `Onboarding Task`s should unlock the largest structural concepts for the first time
- the `Persistent Tech Tree` may both deepen existing concepts and introduce some new gameplay, but only in a tightly controlled amount
- campaign-start baseline should contain only the minimum verbs needed to survive and complete the first onboarding objective

#### Learning Budget Rule

Use a strict `Learning Budget` across the early and mid campaign:

- one site should usually award enough `Reputation` and `Faction Reputation` for only `1` major tech unlock or `2` minor tech unlocks
- a "major" unlock means a new thing the player must actively learn to use, such as a new device family, a new plant family, task chains, or a new assistant behavior rule
- a "minor" unlock means a numeric or quality improvement on a concept the player already knows
- if a site's rewards would unlock too many new ideas at once, reduce the reputation payout or gate some nodes to later tiers
- the goal is not to prevent the tech tree from introducing new things; the goal is to make sure the player only needs to learn a few new things per site game

#### Available At Campaign Start

- Movement and interaction
- Core survival pressure and one readable danger meter
- Basic camp setup such as water access, rest, and one simple repair or placement action
- One basic plant or starter ground-cover option, with the rest of the basic plant set unlocked during the site session
- One authored local objective or onboarding objective
- Minimal phone usage for essential survival and objective tracking

#### Unlocked By `Onboarding Task`

- The `Contract Board` itself first unlocks through the first faction onboarding task
- The first appearance of each `Faction`
- The first appearance of each faction's task style
- The first immediate sample of each faction's assistant reward
- Faction availability on future contract boards
- Faction branch visibility in the `Persistent Tech Tree`
- Early board complexity gates such as faction count on the board and accepted-task count during the first four onboarding sites
- Early `Tutorial Task Set`s that force the player to complete each basic task type once before normal board refresh begins
- The first meaningful understanding of what makes one faction different from another

#### Unlocked By `Persistent Tech Tree`

- New plants, upgraded plants, and broader plant families after the baseline starter option
- New devices, upgraded devices, and better device efficiency
- Better forecasts, sensors, and planning precision after the university concepts exist
- Better irrigation, seedling survival, erosion control, and recovery depth after forestry concepts exist
- Bigger `Task Pool Size`
- Accepted-task capacity upgrades beyond the onboarding baseline if later desired
- Better reward-draft quality, larger draft size, and stronger task-quality control
- Task chains and more advanced board interactions
- Stronger or more flexible faction-assistant usage after the player already understands what the assistant does
- Loadout improvements, worker protection, shelter efficiency, and other long-horizon resilience upgrades

This separation is important, but it should not be treated as an absolute ban. It is acceptable for the `Persistent Tech Tree` to unlock a new gameplay element for the first time. The real rule is that the player must not unlock too many unfamiliar things at once. Reputation pacing, tier gates, and faction requirements should keep each site's post-run learning load small enough that the next site becomes a chance to practice one or two fresh ideas rather than a flood of new systems.

## 14. Failure, Restart, And Recovery

### Site Failure

The player loses a site when the worker's survival state collapses and the current operation can no longer sustain life or productive work. In practice, this means the site can no longer provide enough shelter, protection, recovery, or economic viability to continue.

Failure should feel painful, but not like a campaign deletion.

### Site Retry After Failure

If the player fails a site session, the site's state on the `Regional Map` should not change into a separate category. It remains an unresolved, not-yet-completed `Site`. Starting it again is simply another attempt on the same unresolved site.

Retrying after failure means:

- The local run starts over
- Nearby stabilized sites can provide limited extraction of money or supplies
- Extracted support is finite and can run out
- A fresh site unlock pool is generated for the new attempt
- All active `Run Modifier`s from the failed session are lost
- The player must rebuild local momentum

This reinforces regional interdependence and makes overextending into risky sites a meaningful decision without adding a separate `Regional Map` state just for failed attempts.

### Recovery Philosophy

The design target is setbacks rather than total campaign reset. The player should recover through planning, support routing, and better preparation, not by losing all global progress.

## 15. Content Taxonomy

### Plant Content

| Role | Purpose | Current Placeholder |
|---|---|---|
| Sand-fix starter | Makes bare sand workable for later restoration | `Straw Checkerboard` |
| Protection | Shields tiles, young plants, or work zones | `Wind Reed`, `Root Binder`, `Thorn Shrub`, `Sand Willow` |
| Anti-dehydration | Slows water loss and lowers local heat stress | `Wind Reed`, `Shade Cactus`, `Dew Grass`, `Sunfruit Vine`, `Sand Willow` |
| Fertilize | Improves soil quality and future plant performance | `Root Binder`, `Salt Bean`, `Dew Grass`, `Straw Checkerboard` setup support |
| Salinity rehab | Reduces salty-tile density caps over time so later plants can use the tile better | `Salt Bean`, `Root Binder`, `Dew Grass`, `Sand Willow` |
| Output | Provides food or limited sellable yield | `Salt Bean`, `Sunfruit Vine`, `Thorn Shrub`, `Medicinal Sage` |
| Worker support | Improves comfort, recovery feel, or safe rest value | `Shade Cactus`, `Medicinal Sage`, `Sand Willow` dense pocket support |

Section 11's current plant set is the current working content target. The game should stay at a hand-picked 10-entry roster, including one plant-derived starter material, until combo readability, progression fun, and role overlap are proven in playtests.

### Device Content

In this document, `device` means a player-built support utility or camp structure used to help site survival, restoration, forecasting, or logistics.

| Family | Role |
|---|---|
| Irrigation | Delivers water or supports planted tiles |
| Shelter | Improves recovery, safety, and storm resilience |
| Sensors | Improves weather forecasting and site awareness |
| Workshop | Enables crafting and equipment upkeep |
| Solar Utility | Supports powered site functions without fuel logistics |
| Planting Support | Increases planting speed, survival rate, or nearby performance |

There is no separate trade terminal in v1. Buying, selling, and hiring are handled through the `Field Phone`.

### Task Content

| Goal Type | Typical Ask |
|---|---|
| Restore | Plant cover, reduce erosion, stabilize terrain |
| Sustain | Maintain viability through a weather window |
| Deliver | Produce requested output or milestone results |

## 16. Presentation Direction

### Visual Style

The game uses simplified models with a painterly visual treatment. The desert palette is intentionally desaturated to communicate dryness, exposure, and exhaustion. Restored plants should appear more saturated and visually comforting to signal safety, hope, and progress.

### Environmental Feeling

Heat, sandstorms, and extreme hazard events must be felt, not just represented through numbers. The presentation should use:

- Haze and shimmer for heat
- Visibility loss during sandstorms
- Strong wind direction cues
- Character animation strain under exposure
- Audio changes tied to shelter, windbreaks, and open terrain

During extreme hazard events, presentation should escalate hard enough to make the player feel real danger and uncertainty:

- Fiercer music or percussion-heavy survival scoring
- More aggressive wind and sand audio
- Stronger screen-space motion and visibility disruption
- Sharper contrast between exposed and sheltered spaces
- Clear visual violence in plant motion, cloth, dust, and debris
- Noticeable relief in sound and image when the event finally breaks

The emotional goal is for the player to feel fear, focus, and then release. Surviving a major hazard event should feel like a major accomplishment, with atmosphere and sound doing as much work as systems and numbers.

### Density-Based Soundscape

Plant density should strongly affect the local soundscape around the player. The audio should make progress emotionally tangible before the player even checks numbers.

Sparse or low-density areas should sound harsh and exposed:

- stronger abrasive wind
- more sand hiss and blowing grit
- emptier desert ambience
- less sense of shelter or life

Mixed or recovering areas should sound transitional:

- wind is still present, but slightly softened
- occasional plant rustle appears
- the space feels contested rather than fully safe

Dense restored pockets should sound alive and protected:

- softer wind bed
- leaf and grass movement
- occasional birds or other life cues
- a calmer, more peaceful ambience than the surrounding desert

The contrast matters. Even if the outer boundary of the `Site` is still harsh, a dense planted pocket should sound like a small oasis the player created through strategy and maintenance.

This audio transition should reinforce the mechanical reward:

- dense pockets feel safer
- `Energy` and `Morale` recovery feels faster there
- the player hears that their planning worked

### Readability Goals

The visual style must still support strategy readability:

- Tile states are distinguishable
- Plant roles are recognizable
- Hazard intensity is legible
- Important support devices stand out
- The player can quickly identify safe versus dangerous areas

## 17. V1 Boundaries

These are explicit non-goals for the first version:

- No combat
- No bandits or hostile factions
- No persistent colony population simulation
- No deep genetics or breeding system
- No fuel-based logistics economy
- No fully open-world seamless map
- No highly realistic policy simulation

## 18. Minimum Playable Prototype

This chapter defines the smallest prototype that is still worth building. It should use as little feature breadth as possible, but it must still preserve the full loop sketch the game is trying to sell: survival pressure, faction-published tasks, guaranteed faction reputation, small tech growth, harsh-event recovery, and a sense that the next site plays differently because of the player's choices.

Prototype-specific scope language, temporary content counts, and first-playable constraints should be centralized in this chapter. Earlier chapters should define the core game model in neutral system terms.

The key scope rule is:

- cut breadth inside each system before cutting away an entire stage of the loop

If the prototype removes too many loop stages, it may become cheaper to build but it will stop proving the actual game.

### Prototype Goal

The minimum playable prototype should answer one question:

- Is the loop of survival pressure, faction task choice, reputation growth, small tech unlocks, escalating harsh-event response, and next-site adaptation fun enough to justify the larger game?

If this version is not fun, adding more content volume later will not fix the foundation.

### What The Prototype Must Prove

The minimum playable prototype must prove these beats in one connected arc:

- the player lands on a hostile site and immediately feels survival pressure
- the player unlocks the `Contract Board` through play, not through menu exposition
- tasks become the main motivation layer for moment-to-moment play
- each completed task gives both immediate reward and fixed publisher `Faction Reputation`
- different factions feel meaningfully different in tasks, assistant support, and upgrade direction
- the player earns a small amount of `Reputation` and chooses a tech unlock between sites
- harsh environments begin early, stay readable, and escalate in severity from site to site
- faction help after harsh events is better when reputation is higher
- the fourth site finally shows the full `3`-faction prototype board in action under repeated pressure

That is the smallest version that still shows the intended game loop rather than only a fragment of it.

### Scope Principles

When cutting for prototype scope, use these priorities:

- keep all loop stages, but shrink the content inside them
- prefer authored content over procedural generation
- keep all `3` prototype factions, but give each only the minimum content needed to express its style
- prefer one small set of authored harsh environments used well over a broad unfinished event catalog
- prefer a small plant roster with clear jobs over a broad but muddy roster
- prefer one tiny between-site upgrade choice over a full campaign meta layer
- keep the `Learning Budget` strict so each site introduces only a small amount of new knowledge

### Minimum Prototype Arc

The minimum playable prototype should be a short authored `4`-site sequence in fixed order:

1. `Site 1`: survive the opening minutes, complete the `Village Committee` onboarding task, unlock the `Contract Board`, and learn labor-first task play under a light harsh-environment warning
2. Finish `Site 1`, earn a small tech unlock, and enter `Site 2` with a visible difference in capability
3. `Site 2`: unlock the `Forestry Bureau of Autonomous Region`, learn plant-water task play, and face a stronger harsh event with meaningful aftermath
4. Recover from the event, receive faction relief based on reputation, finish the site, and buy another small unlock
5. `Site 3`: unlock the `Autonomous Region Agricultural University`, keep the board focused on university-style work, and learn the device / forecast side of play under another higher-severity hazard
6. Finish `Site 3`, buy another small unlock, and enter `Site 4`
7. `Site 4`: use the full `3`-faction board for the first time while surviving escalating waves of harsh events
8. Finish `Site 4` with the complete prototype loop demonstrated end to end

This is still small, but it preserves the whole shape of the intended game.

### Prototype Content Recommendation

For the first minimum playable build, implement the following.

#### Campaign Structure

- `4` authored sites in fixed order
- no procedural regional map
- no off-screen site simulation
- no site revisits
- `1` simple between-site progression step after each site

The prototype does not need open campaign routing. It only needs enough between-site structure to make upgrades matter.

#### Factions

Keep all `3` factions, but cut each one down to a tiny playable slice:

- `Village Committee`
- `Forestry Bureau of Autonomous Region`
- `Autonomous Region Agricultural University`

Each faction only needs:

- `1` authored `Onboarding Task`
- `1` small `Tutorial Task Set`
- `2-3` repeatable normal task templates after onboarding
- `1` assistant support package
- `2` tech nodes in its faction branch

This is the minimum that still lets the player compare factions instead of only hearing about them.

#### Contract Board And Tasks

- the `Contract Board` unlocks during `Site 1` through the `Village Committee` onboarding task
- while a faction is still in its tutorial phase, its board slice should use authored tasks and should not refresh until all required basic task types are completed once
- after tutorial sets are cleared, task replacement can be simple and does not need a complex timer economy
- during onboarding `Site 1` to `Site 3`, the board should only show tasks from the featured faction of that specific site
- site board plan should be:
- `Site 1`: `Village Committee` only, `Accepted Task Cap = 2`
- `Site 2`: `Forestry Bureau of Autonomous Region` only, `Accepted Task Cap = 2`
- `Site 3`: `Autonomous Region Agricultural University` only, `Accepted Task Cap = 2`
- `Site 4`: full prototype board with all `3` factions, `6` visible tasks, and `Accepted Task Cap = 3`
- accepted tasks do not expire
- unaccepted tasks may be replaced by simple refresh rules after tutorial sets are cleared
- no free task abandonment in the prototype
- every completed task grants:
- guaranteed publisher `Faction Reputation`
- one immediate reward package or a small reward draft
- `Faction Reputation` amount is fixed by the task definition, but scales by task level or tier
- no `Task Chain`s in the minimum playable build
- no jackpot-tier tasks in the minimum playable build
- no `Run Modifier`s in the minimum playable build

This keeps the board strategic without requiring the full late-game board economy.

#### Survival Model

- keep survival readable and narrow
- recommended minimum player meters:
- `Hydration`
- `Energy`
- one simple camp rest / shelter function
- one simple water refill / purchase loop
- `Nourishment` and broader morale interactions beyond work-efficiency impact can be deferred or represented indirectly

The player should feel pressure, but not have to manage many overlapping bars in the first prototype.

#### Ecology And Restoration

- use only `4` plants in the minimum playable build
- recommended set:
- `Straw Checkerboard`
- `Wind Reed`
- `Root Binder`
- `Salt Bean`
- keep plant states simple but visible:
- newly placed and fragile
- established and protective
- regressing / withering
- do not require the full plant roster or deep adjacency modeling

This is enough to prove that restoration choices change survival and site stability.

#### Devices

Use only the smallest device set needed to express the later factions:

- for the prototype, the main technical device family should belong to `Autonomous Region Agricultural University`
- recommended university prototype set:
- `Precision Irrigation Line`
- `Solar Shade Rack`
- optional simplified `Checkerboard Deployment Machine` as a university action, assistant, or site unlockable
- `Forestry Bureau of Autonomous Region` does not need its own separate device family in the prototype; it can stay defined by plants, recovery logic, and water-sensitive ecology tasks
- deeper groundwater pumping, brackish-water use, and salt-treatment branches can be deferred to demo or full-game scope
- advanced device families, deeper calibration systems, and broad utility categories can still be deferred

The university must exist in play, but it does not need a full technical sandbox.

#### Weather And Harsh Event

- keep baseline heat, wind, and dust pressure in simplified form
- use a small authored harsh-environment set rather than a large random catalog
- recommended prototype hazard set:
- low / high temperature spike
- `Sandstorm`
- one additional authored pressure package if needed for variety, such as pest or insect pressure
- do not implement a separate `Emergency Field Action` system in the prototype
- harsh-environment severity should scale by site:
- `Site 1`: low severity introduction
- `Site 2`: medium severity event with real aftermath
- `Site 3`: higher severity event that pressures the new university tools
- `Site 4`: `2-3` escalating waves, each harder than the last
- each major event or wave must still include:
- warning time
- visible peak danger
- partial damage
- a real aftermath recovery problem

The prototype does not need a huge event catalog. It needs a readable escalation ladder.

#### Faction Relief And Assistants

These are part of the loop and should stay in the prototype, but in reduced form:

- each faction only needs `1` assistant support package
- after major harsh events, each unlocked faction may offer one simple relief package
- relief quality should depend on reputation band with that faction
- use a small banded model such as low / medium / high reputation rather than a complex formula
- full random faction event pools can be deferred

This preserves the key promise that reputation matters during crisis recovery.

#### Economy

- money must exist and matter
- direct task payouts should be the main cash source
- the player only needs a very small shop list such as water and a few field supplies
- a broad sell economy, contractor market, and trade simulation can be deferred

#### Progression

- keep a tiny between-site `Persistent Tech Tree`
- the player should usually unlock only `1` major node or `2` minor nodes after a site
- tech can introduce new things, but the `Learning Budget` must stay low
- recommended minimum node count:
- `1` neutral starter node
- `2` `Village Committee` nodes
- `2` `Forestry Bureau of Autonomous Region` nodes
- `2` `Autonomous Region Agricultural University` nodes

This is enough to make reputation and site completion feel forward-looking without building a full long-form metagame.

### What Should Be In Prototype Vs Deferred

#### Keep In The Minimum Playable Prototype

- the `4`-site authored prototype arc
- all `3` factions in minimal form
- the first `Contract Board` unlock in play
- guaranteed `Faction Reputation` on every completed task
- small faction tech branches
- escalating harsh environments from `Site 1` to `Site 4`
- simple faction relief based on reputation
- one assistant sample per faction

#### Simplify Hard

- task generation
- reward drafts
- plant roster
- device roster
- economy breadth
- forecast precision model
- presentation depth

#### Safely Defer

- `Task Chain`s
- jackpot-tier tasks
- `Run Modifier`s
- full random faction event pools
- commendations
- regional support output
- neighboring-site loadout assembly
- off-screen site simulation
- site revisits
- full procedural map generation
- the full final plant roster
- advanced sensor and calibration gameplay
- complex inventory and trade systems
- broad contractor simulation beyond simple assistant packages

### What The Minimum Prototype Is Not Trying To Prove

The minimum playable prototype is not required to prove:

- final replayability across many runs
- final campaign pacing beyond the first `4` sites
- full tech-tree breadth
- final economy tuning
- final content volume
- full random event diversity
- large-scale procedural variety

It is only trying to prove that the whole loop sketch works at small scale.

### Definition Of Done

The minimum playable prototype is successful if a playtester can:

- understand the immediate survival problem on site entry
- complete the `Village Committee` onboarding task
- unlock and use the `Contract Board`
- learn one faction at a time across the first `3` sites without feeling overloaded
- complete tasks for all `3` factions and understand their differences
- receive both immediate reward and guaranteed `Faction Reputation` from task completion
- buy small tech upgrades between sites
- survive escalating harsh-environment pressure across the `4`-site arc
- notice that higher faction reputation improves the recovery help offered after major events
- reach `Site 4` and use the fully unlocked prototype board

If those beats work, the project has a real prototype foundation. If they do not, the correct response is to improve this small loop before adding more systems.

## 19. Implementation Readiness Checklist

This checklist is here to help inspect the whole loop before moving into code-structure planning. It separates what is already concrete enough for implementation from what still needs a tighter contract.

### Current Scope Assessment

- Verdict: the current build is strong enough to build and playtest
- Main strength: the game already has a real pressure loop built from survival, hazards, strategic planting, short-term tasks, emergency rescue, and regional preparation
- Main strength: the current build has multiple reward layers, including site stabilization, task rewards, modifiers, commendations, and nearby-site support
- Main strength: the regional order layer now adds meaningful campaign strategy rather than just map traversal
- Main risk: cognitive load is still high, so onboarding and information flow will matter a lot
- Main risk: task generation, nearby-site support, and modifier tuning could become either too weak to matter or strong enough to flatten difficulty
- Main risk: some progression and meta systems are conceptually solid but still not numerically or structurally defined enough for direct implementation

### Defined Enough For Current-Scope Coding

- [x] Core gameplay loop and campaign loop
- [x] Runtime simulation cadence and update order
- [x] Tile layers, occupancy rules, and terrain fertility baseline
- [x] Player condition, environmental pressure, and hazard philosophy
- [x] Hazard and weather runtime model
- [x] Harsh-event field-work runtime model
- [x] Resource and inventory model
- [x] Device roster
- [x] Plant roster, density ladder, degeneration, and spread
- [x] Contract board direction and site-task reward model
- [x] Persistent progression direction
- [x] Site unlockables, run modifiers, and commendation direction
- [x] Loadout direction and nearby-site support direction

### Partially Defined And Needs One More Contract Pass

- [ ] `Site Task` runtime schema
- [ ] Exact task refresh timing
- [ ] Exact task completion detection rules
- [ ] Exact reward-draft generation weights and anti-duplication rules
- [ ] Exact `Task Chain` generation rules
- [ ] Site certification and completion thresholds
- [ ] Exact `Loadout` assembly contract
- [ ] Exact regional support accrual and export timing
- [ ] Exact `Regional Support Output` formula using `siteYieldScore` and composition factors
- [ ] Exact `Site Output Modifier` generation and stacking rules
- [ ] Failure trigger and restart-recovery contract
- [ ] Off-screen site simulation contract
- [ ] Economy numeric contract for prices, payouts, and contractor costs

### Still Missing As Formal Sections

- [ ] Save data boundaries
- [ ] Explicit off-screen simulation section with formulas instead of high-level description
- [ ] Explicit failure and restart numeric section instead of mostly design intent

### Suggested Next Definition Order

- [ ] `Site Task` runtime schema
- [ ] Site completion and certification contract
- [ ] `Loadout` assembly contract
- [ ] `Regional Support Output` numeric contract
- [ ] Failure and restart contract
- [ ] Off-screen simulation contract
- [ ] Save data boundaries

### Current Scope Health Check

- [x] The current build already has enough tension to be fun if pacing and information flow are handled well
- [x] The plant game is no longer passive because hazards, density loss, risky harsh-event work, and comeback decisions all matter
- [x] The regional layer is strong enough to support meaningful site-order strategy
- [ ] The current build is not yet safe for large-scale implementation without the missing contracts above

## 20. Acceptance Scenarios

These scenarios define whether the design is behaving correctly in current-build reviews and later production reviews.

### New Site

The player can choose a `Loadout`, enter a `Site`, understand immediate survival pressure, receive only the currently unlocked gameplay concepts, see an initial readable pool of `Site Task`s appropriate to their current onboarding step, and complete an initial task that reveals a clear `Task Reward Draft` without relying on hidden rules.

### First 5 Minutes Check

In the first 5 minutes, a new player should only need to understand one survival need, one accepted task, one immediate reward choice, and one automatic trust gain. If the player is being asked to compare multiple factions, multiple acceptance slots, refresh logic, chain logic, and weather detail all at once, onboarding is too front-loaded.

### First 30 Minutes Check

The first 30 minutes should create momentum without flooding the player. A new player should understand the survival basics, complete simple tasks, make at least one readable reward-draft choice, survive one moderate challenge, and feel interested in continuing rather than overwhelmed.

### Weather Pressure

A forecast warns about a heat wave or sandstorm. The player can respond through timing, hydration, shelter, devices, or work prioritization, and the result feels fair.

### Extreme Hazard Event Check

At least some site runs should contain genuinely fierce environmental events that make failure feel possible for a limited time. During those windows, music, rendering, audio, and survival pressure should all escalate together so the player feels immersed, excited, and uncertain about the outcome. The player should also be forced into meaningful protective choices rather than only waiting passively. If the player survives, the post-event release should feel clearly rewarding and memorable. Survival should not mean zero damage; a bad event should often leave behind reduced `Plant Density`, damaged supplies, lower output, and a real comeback problem. However, strong local protection should materially reduce the damage. Testers should be able to see that good strategic placement changes the result from catastrophic collapse to manageable loss.

### Harsh-Event Field Work Check

During a severe sandstorm or heat event, the player should be able to perform at least one meaningful normal field action such as burial clearing, watering, or repair that visibly saves or stabilizes part of the site. The player should feel that a specific patch lived because they intervened under pressure, not only because pre-existing passive bonuses absorbed the damage.

### Restoration Readability

Plant placement creates visible local effects on erosion, wind, heat, moisture, soil fertility, or salinity rehab. Even with only the current plant set, the player should be able to understand how the land is changing because of their decisions and why one small combo is working better than another.

### Plant Degeneration Check

If the player places the wrong plant in the wrong zone, that mistake should become visible through density loss, weaker effects, withering, or death. If the player places the right support plants and support devices first, lowering local pressure enough, the same zone should be noticeably more survivable and productive.

### Plant Growth Check

The player should only be able to place fragile low-density starter plants. Over time, a low-pressure living patch should visibly grow into stronger density states, gain stronger traits, and eventually become capable of limited natural spread into nearby empty fertile and salinity-suitable tiles. A high-pressure patch should stall, stay vulnerable, or regress instead.

### Bare Sand Conversion Check

On pure sand or the least fertile tiles, the player should have a viable opening move through `Straw Checkerboard`. It should immediately calm the surface enough to matter with maximum initial density, then gradually lose temporary protection and soil-support strength as its density falls rather than acting as a permanent all-purpose answer by itself.

### Restored Pocket Reward Check

When the player builds a local cluster of mostly medium-density or high-density tiles, the area should feel meaningfully different from exposed desert. It should sound calmer and more alive, provide faster `Energy` and `Morale` recovery, and communicate that the player's strategy has created a small protected haven inside the harsh site.

### Protection Reward Check

When two similar patches face the same severe weather, the better-protected patch should clearly perform better. It should keep more `Plant Density`, lose less output, retain more neighbor protection, and require less recovery work afterward. Players should feel that strategy earned survival, not that they got lucky.

### Plant Fun Check

Within one early-to-mid `Site` run, the player should be able to use the 10-entry current plant set to build at least one defensive combo, one support or recovery combo, and one output combo. Those combos should feel different, readable, and worth chasing even before the site is fully restored.

### Recovery Spiral Check

A failing patch should create a meaningful recovery problem rather than a silent number loss. The player should be able to spend time, money, labor, or safer replanting choices to stop a local downward spiral, but the rescue should cost enough that prevention still feels smarter than repair.

### Post-Event Triage Check

After a severe hazard event, the player should usually have more than one urgent problem but enough clarity to choose the best comeback path. The strongest recovery choice should often involve prioritizing one of the following over the others for a short time:

- restoring core protection
- restoring water or storage function
- saving one remaining output patch
- recovering `Player Condition`

### Economy Viability

`Site Task`s, phone-based buying and selling, water purchases, solar-backed support, optional `Site Unlockable` purchases, field output, and contractors create a working economy without combat or fuel systems.

### Short-Term Reward Check

During normal site play, the player should almost always have at least one nearby achievable target that can quickly pay out in a local unlock opportunity, a modifier opportunity, money, resources, safety, and fixed faction trust.

### Cognitive Load Check

During active play, the player should be able to identify the most important immediate danger, the most relevant current opportunity, and the next meaningful choice without feeling buried under every system at once. If testers describe the game as confusing because too many layers demand attention simultaneously, the presentation and unlock pacing are wrong.

### Target Ladder Check

While a `Site` is still unrestored, the player should consistently have immediate targets, short targets, a current site phase target, and at least one campaign-level target visible or easily discoverable.

### Site Task Check

During a normal site run, the player should be able to see a limited but refreshed pool of `Site Task`s, understand each task's publishing `Faction`, commit to no more than `3` accepted tasks at once in the current design, finish them in roughly 5 to 10 minutes, and feel that task rewards meaningfully feed the next local decision. Completing a task should always grant its guaranteed `Faction Reputation` payout, with the amount scaling by task tier or level, and most `Site Task`s should then reveal a `Task Reward Draft` with `3` or more options. Those options should be able to include `Site Unlockables`, `Run Modifier`s, and `Task Reward Package`s containing money, resources, or mixed tactical bundles. `Task Pool Size` and `Accepted Task Cap` upgrades in the `Persistent Tech Tree` should noticeably change how many short-term opportunities the player can pursue at once.

### Task Tier Excitement Check

Task tiers should be clearly readable, and higher-tier tasks should feel meaningfully rarer and more valuable. When a jackpot-tier task appears in a refreshed pool, the player should immediately recognize it as special, feel excited by the opportunity, and believe the reward is worth the added difficulty and risk.

### Run Modifier Check

At least some jackpot-tier task outcomes should grant `Run Modifier`s that feel powerful enough to change the player's strategy for the rest of the current site session. The correct player reaction should be closer to "this changes my whole run" than "this is just a slightly bigger reward."

### Modifier Balance Check

Run modifiers should feel exciting and powerful without collapsing replayability into one best outcome. If players consistently fish for a single modifier because it dominates too many site conditions, modifier design or spawn rules need revision.

### Task Chain Check

Task pools should sometimes generate clear `Task Chain`s with visibly linked tasks. Each task in the chain should pay normal rewards, but completing the whole chain should grant an extra reward worth planning around. Players should be able to notice and exploit those chains rather than experiencing the task pool as a flat list of disconnected errands.

### Site Commendation Check

When the player successfully restores a `Site`, they should receive a `Site Commendation` that feels specific and earned. The commendation title and cited reason should reflect how the site was restored, not just that the player reached the finish line. Testers should be able to tell why they earned a storm-proof, low-water, rapid, or clean recovery commendation.

### Failure And Recovery

When a site session collapses, the player can retry that unresolved site by drawing limited support from nearby sites, losing time and momentum but not all campaign progress. The site's regional-map state should remain unchanged until it is actually completed, and the spent time should still matter because the player is racing the `Campaign Clock`.

### Regional Progress

Finishing one site produces visible support or ecological benefits for nearby sites and moves the campaign closer to regional recovery.

That regional progress should also be visible directly on the `Regional Map`. A completed site's node should immediately change presentation so the player can recognize that it is now a finished support source rather than another unresolved target.

### Replayability Check

Multiple fresh campaigns produce noticeably different strategic stories because of changes in `Regional Map` layout, generated site terrain, weather patterns, faction mixes, task-board offers, event timing, reward structures, and tech path incentives.

### Tech Divergence Check

Two campaigns with different faction-tech paths should produce different viable play styles rather than converging into the same optimal plan by the midgame. Within a campaign, different sites should also feel tactically different because their site unlock pools, modifier opportunities, publisher mixes, and local reward choices differ.

### Tech-Money Loop Check

On a typical `Site`, the player should be able to follow a satisfying loop of completing a task, seeing a `Task Reward Draft`, choosing `1` reward, using that choice to reveal an unlock, activate a modifier, or gain immediate resources, then converting that payoff into better protection, output, or momentum that points toward the next draft.

### Currency Separation Check

Money should clearly function as the current-site tactical currency for survival, hiring, buying, and revealed `Site Unlockable` purchases. `Reputation` and `Faction Reputation` should clearly function as progressive campaign trust values used for faction-tech progression and better institutional support.

### Site Reset Check

Leaving, failing, or restarting a site should reset its site unlock pool, clear its active `Run Modifier`s, and preserve all intended `Persistent Tech Tree` progress.

## 21. Production Notes

- Use role-based plant families first, then replace with more specific species later if realism becomes a production goal.
- Do not expand the plant roster beyond the current set until 2-role plants, neighbor effects, and support-dependent output are clearly fun in testing.
- Keep the first pressure model small. Start with a few readable forces such as heat, wind abrasion, burial, erosion, and salinity pressure before adding more exotic hazards.
- Validate density growth early. The game should prove that fragile starter plants can be nursed into self-sustaining patches and that limited natural spread feels like an earned payoff.
- Validate density-based reward feedback early. Dense restored pockets should sound calmer, feel safer, and provide noticeably better `Energy` and `Morale` recovery than exposed desert.
- Validate the bare-sand opener early. `Straw Checkerboard` should begin strong enough to make pure sand playable, then visibly lose protection and other density-scaled contribution strength as its current density falls.
- Validate site play before expanding narrative or world flavor.
- Keep contractor behavior simple until the core survival and restoration loop is proven.
- Make forecasting, tile feedback, and support extraction readable early, because they are central to the game's identity.
- Plants must not become fire-and-forget timers. Validate that harsh conditions, degeneration, and recovery decisions create strategy every few minutes of site play.
- Validate harsh-event work pressure early. Extreme weather should include risky, limited, hands-on burial clearing, repair, or rescue work so the player feels they fought back and saved something tangible.
- Validate partial degradation, not only full destruction. Reduced `Plant Density`, damaged supplies, weakened output, and worsened `Player Condition` should create the current build's most interesting comeback decisions.
- Treat replayability as a core production requirement, not post-launch polish. Procedural map generation, task-board variety, event variety, and meaningful tech branching should be validated in early milestones.
- Validate the dual-layer progression structure early. The `Persistent Tech Tree` must feel campaign-defining, while site unlocks and modifiers must feel fresh on each site without overwhelming the player.
- Validate generated `Site Task`s early. The 5 to 10 minute task loop should feel practical, varied, and tightly connected to reward-draft reveal, local unlock purchasing, modifier pivots, and restoration progress.
- Validate task-tier excitement early. High-tier task spawn rates, readability, and reward strength should be tuned so jackpot appearances feel rare and thrilling rather than noisy or disappointing.
- Validate jackpot modifiers, task chains, and `Site Commendation`s early. These systems should create memorable site-run pivots and meaningful recognition, not just sit as decorative bonuses.
- Validate extreme hazard events early. The harshest environmental moments should become emotional peaks driven by survival pressure, music, and rendering, not just bigger damage numbers.
- Validate onboarding and cognitive load early. The first session must teach the loop in layers without overwhelming the player.
- Keep money, `Reputation`, and `Faction Reputation` clearly separated in reward and progression design.


## 22. Meter Relationship

This meter-relationship chapter defines how worker, terrain, plant, and weather meters connect to each other. It is not the final tuning math. Its purpose is to make the runtime system readable before exact coefficients are balanced, and to act as the implementation-facing reference for program structure.

Design rule:

- meters should affect each other through explicit relationships, not hidden outcome shortcuts
- tables in this chapter should list only core meters and core plant-side values; math-stage helper variables and presentation diagnosis outputs belong outside this chapter
- site weather should first resolve into local `tileHeat`, `tileWind`, and `tileDust`
- resolved local weather then changes terrain pressure and plant pressure
- terrain state changes plant growth, density cap, and recovery potential
- plants, `Straw Checkerboard`, devices, and terrain shelter directly change resolved local weather and terrain quality over time
- visible outcomes such as density loss, recovery, spread, or collapse should emerge from meter changes

### Meter Groups

This summary should include only core runtime meters and the core plant-side values that directly participate in those meter relationships.

| Group | Meters / values | Role |
|---|---|---|
| Event meters | `eventHeatPressure`, `eventWindPressure`, `eventDustPressure` | Site-wide harsh-event channel meters. They represent current event force and feed the weather layer rather than acting as weather themselves. |
| Weather meters | `weatherHeat`, `weatherWind`, `weatherDust` | Site-wide ambient weather outputs after baseline site conditions and current event meters are combined. |
| Resolved local weather meters | `tileHeat`, `tileWind`, `tileDust` | Per-tile weather result after site weather, local support, and shelter are combined. They bridge site weather and final terrain or plant pressure. |
| Worker state meters | `playerHealth`, `playerHydration`, `playerNourishment`, `playerEnergyCap`, `playerEnergy`, `playerMorale`, `playerWorkEfficiency` | Core worker survival and performance meters. They represent the worker's current physical and mental condition plus the resolved action-energy modifier used during site play. |
| Persistent terrain soil meters | `tileSoilFertility`, `tileMoisture`, `tileSoilSalinity` | Long-lived or short-lived land condition on plantable `Ground`. These meters determine what can grow well. |
| Temporary tile pressure | `tileSandBurial` | Recoverable sand overlay created mainly by sandstorms. If ignored, it can create lasting fertility loss. |
| Plant meters | `tilePlantDensity`, `growthPressure`, `salinityDensityCap` | Shared plant-side runtime meters. `tilePlantDensity` tracks current plant presence, `growthPressure` governs growth-capable plants, and `salinityDensityCap` is the plant-side density ceiling created by salty ground. |
| Plant behavior values | `growable`, `constantWitherRate` | Core plant-side behavior values. `growable` decides whether favorable conditions may increase density, and `constantWitherRate` applies steady density loss to plant density when defined. |
| Plant resistance values | `saltTolerance`, `heatTolerance`, `windResistance`, `dustTolerance` | Plant-definition values that turn salinity, heat, wind, and dust pressure into species-specific density limits and pressure resistance. |
| Object contribution meters | `auraSize`, `windProtectionPower`, `heatProtectionPower`, `dustProtectionPower`, `fertilityImprovePower`, `salinityReductionPower` | Shared local contribution values and meters. Plants, devices, and terrain shelter may feed them; object definition sets local contribution ceilings and reach, while `tilePlantDensity` scales plant-side output. |

### Event Meter Relationships

| Meter | Impacted by | Impact to | Notes |
|---|---|---|---|
| `eventHeatPressure` | Current event archetype, current event phase, `eventTier` | `weatherHeat` | Event-side heat channel. When no harsh event is active, this should sit at or near `0`. |
| `eventWindPressure` | Current event archetype, current event phase, `eventTier` | `weatherWind` | Event-side wind channel. |
| `eventDustPressure` | Current event archetype, current event phase, `eventTier` | `weatherDust` | Event-side dust channel. |

### Weather Meter Relationships

| Meter | Impacted by | Impact to | Notes |
|---|---|---|---|
| `weatherHeat` | Daily heat curve, site heat bias, `eventHeatPressure` | `tileHeat` | Site-wide heat should resolve into local heat before it affects terrain, plants, or worker meters. |
| `weatherWind` | Daily wind curve, site wind bias, gust variation, `eventWindPressure` | `tileWind` | Site-wide wind should resolve into local wind before it affects terrain, plants, or worker meters. |
| `weatherDust` | Site dust bias, current wind, recent-event aftermath, `eventDustPressure` | `tileDust` | Site-wide dust pressure should resolve into local dust before it affects terrain, plants, or worker meters. |

### Resolved Local Weather Meter Relationships

| Meter | Impacted by | Impact to | Notes |
|---|---|---|---|
| `tileHeat` | `weatherHeat`, `heatProtectionPower` | `playerHydration`, `playerHealth`, `playerWorkEfficiency`, `tileMoisture`, `growthPressure` | Final local heat pressure. It should hurt worker output through `playerWorkEfficiency`, not by directly draining `playerEnergy`. |
| `tileWind` | `weatherWind`, `windProtectionPower` | `playerWorkEfficiency`, `tileMoisture`, `tileSoilFertility`, `growthPressure` | Final local wind exposure. It should hurt worker output through `playerWorkEfficiency`, not by directly draining `playerEnergy`. |
| `tileDust` | `weatherDust`, `dustProtectionPower` | `playerHealth`, `playerWorkEfficiency`, `tileSandBurial`, `tileSoilFertility`, `growthPressure` | Final local dust exposure. It should hurt worker output through `playerWorkEfficiency`, not by directly draining `playerEnergy`. |

### Worker State Meter Relationships

| Meter | Impacted by | Impact to | Notes |
|---|---|---|---|
| `playerHealth` | Rest and shelter recovery, medicine, harsh outdoor exposure, dangerous field work, `tileHeat`, `tileDust`, low `playerHydration`, low `playerNourishment` | `playerEnergy`, `playerWorkEfficiency` | Slow physical-condition meter. Low health should make the worker less resilient and less efficient even if short-term energy is restored. |
| `playerHydration` | Drinking actions, water items, time, `tileHeat`, outdoor work | `playerEnergyCap` | Most urgent worker survival meter in hot conditions. Local heat should make hydration fall faster on exposed tiles, which then limits how much energy can actually be used. |
| `playerNourishment` | Eating actions, food items, time, outdoor work | `playerEnergyCap` | Slower-moving worker support meter for sustained efficiency and recovery. It should mainly limit the usable energy ceiling instead of directly draining `playerEnergy`. |
| `playerEnergyCap` | `playerHydration`, `playerNourishment` | `playerEnergy` | Usable energy ceiling. Low cap should throttle how much stored energy the worker may actually access. |
| `playerEnergy` | Work actions, rest recovery, `playerHealth`, `playerEnergyCap`, `playerWorkEfficiency` | None | Main short-term work-capacity meter and end-consumption meter. It should be clamped by `playerEnergyCap`, while low `playerWorkEfficiency` should raise the effective energy cost of each action and drain this meter faster. |
| `playerMorale` | Safe rest, dense-cover recovery pockets, harsh-event aftermath, current site setbacks and recovery progress | `playerWorkEfficiency` | Worker comfort and psychological stability meter. Low morale should make routine work more energy-expensive or less reliable rather than directly lowering the energy meter. |
| `playerWorkEfficiency` | `playerHealth`, `playerEnergy`, `playerEnergyCap`, `playerMorale`, `tileHeat`, `tileWind`, `tileDust`, workload strain | Action energy cost, `playerEnergy` | Resolved worker-output meter. This should be the main gameplay-facing meter for how expensive each action is. Worse local weather should mainly hurt the player by lowering this meter, which raises energy cost per action. |

### Terrain Meter Relationships

| Meter | Impacted by | Impact to | Notes |
|---|---|---|---|
| `tileMoisture` | Watering, direct irrigation such as `Drip Irrigator`, `tileHeat`, `tileWind`, `tileSoilFertility` | `growthPressure` | Stored water on the tile. Better moisture lowers plant pressure and softens heat stress. `tileSoilFertility` raises the maximum moisture capacity and slows moisture reduction, while watering and irrigation increase the current amount. |
| `tileSoilFertility` | `fertilityImprovePower`, `tileWind`, `tileDust`, `tileSandBurial` | `tileMoisture`, `growthPressure` | Long-term land quality meter. Better fertility improves plant performance, raises the tile's maximum moisture capacity, and slows moisture reduction, but does not directly add moisture by itself. |
| `tileSoilSalinity` | Authored starting salinity, `salinityReductionPower` | `salinityDensityCap` | Strategic placement and rehabilitation meter. |
| `tileSandBurial` | `tileDust`, burial-clearing actions | `tileSoilFertility`, `growthPressure` | Temporary overlay state rather than long-term soil quality. |

### Plant, Resistance, And Object Contribution Relationships

| Meter / value | Impacted by | Impact to | Notes |
|---|---|---|---|
| `growable` | Plant definition only | `tilePlantDensity` | Allows favorable conditions to create density gain. If `false`, low `growthPressure` does not produce positive growth. |
| `constantWitherRate` | Plant definition only | `tilePlantDensity` | Applies steady density loss every fixed simulation step. |
| `auraSize` | Object definition only | Reach of `windProtectionPower`, `heatProtectionPower`, `dustProtectionPower`, `fertilityImprovePower`, and `salinityReductionPower` | Shared contribution reach value. `0` means own tile only, and larger values extend contribution reach by Manhattan distance. |
| `windProtectionPower` | Object definition only, `tilePlantDensity`, `deviceEfficiency`, `auraSize` | `tileWind` | Shared local wind-protection meter. Any object with positive wind protection may feed it. Plant density scales plant-side output, while device efficiency scales device-side output. |
| `heatProtectionPower` | Object definition only, `tilePlantDensity`, `deviceEfficiency`, `auraSize` | `tileHeat` | Shared local heat-protection meter. Any object with positive heat protection may feed it. Plant density scales plant-side output, while device efficiency scales device-side output. |
| `dustProtectionPower` | Object definition only, `tilePlantDensity`, `deviceEfficiency`, `auraSize` | `tileDust` | Shared local dust-protection meter. Any object with positive dust protection may feed it. Plant density scales plant-side output, while device efficiency scales device-side output. |
| `fertilityImprovePower` | Object definition only, `tilePlantDensity`, `deviceEfficiency`, `auraSize` | `tileSoilFertility` | Shared local soil-building meter. Plant density scales plant-side output, while device efficiency scales device-side output when a device supports soil recovery. |
| `salinityReductionPower` | Object definition only, `tilePlantDensity`, `deviceEfficiency`, `auraSize` | `tileSoilSalinity` | Shared local salinity-reduction meter. Plant density scales plant-side output, while device efficiency scales device-side output when a device supports salinity recovery. |
| `saltTolerance` | Plant definition only | `salinityDensityCap` | Preserves usable density on salty ground. |
| `heatTolerance` | Plant definition only | `growthPressure` | Lowers heat-side growth pressure. |
| `windResistance` | Plant definition only | `growthPressure` | Lowers wind-side growth pressure. |
| `dustTolerance` | Plant definition only | `growthPressure` | Covers dust exposure and burial-side pressure. |
| `growthPressure` | `tileMoisture`, `tileSoilFertility`, `tileSandBurial`, `tileHeat`, `tileWind`, `tileDust`, `heatTolerance`, `windResistance`, `dustTolerance` | `tilePlantDensity` | Final plant pressure meter for growth-capable plants. |
| `salinityDensityCap` | `tileSoilSalinity`, `saltTolerance` | `tilePlantDensity` | Plant-side density ceiling on salty ground. |
| `tilePlantDensity` | `growthPressure`, `salinityDensityCap`, `growable`, `constantWitherRate` | `windProtectionPower`, `heatProtectionPower`, `dustProtectionPower`, `fertilityImprovePower`, `salinityReductionPower` | Current plant strength and effect scale. Higher density strengthens the plant-fed share of the shared object contribution meters first, while `auraSize` controls how far those plant-side contributions can reach. |

`Straw Checkerboard` should be authored through the same shared plant rows: start it at maximum `tilePlantDensity`, set `growable = false`, set a positive `constantWitherRate`, set `auraSize` as needed for setup reach, keep `growthPressure` at `0`, and rely on normal shared contribution values such as `fertilityImprovePower`.

### Main Causal Loop

Use this loop as the core mental model:

1. Event state updates `eventHeatPressure`, `eventWindPressure`, and `eventDustPressure`.
2. Weather updates `weatherHeat`, `weatherWind`, and `weatherDust` from baseline site conditions plus current event meters.
3. Site weather plus local plants, `Straw Checkerboard`, protective structures, and terrain shelter resolve `tileHeat`, `tileWind`, and `tileDust`.
4. Worker state updates `playerHealth`, `playerHydration`, `playerNourishment`, `playerEnergyCap`, `playerEnergy`, `playerMorale`, and `playerWorkEfficiency` from local weather, work, rest, supplies, medicine, and recovery context.
5. Resolved local weather, irrigation, and plant contribution values update terrain pressure: moisture drain, burial, fertility change, and salinity change.
6. Terrain state plus resolved local weather and plant resistance values feed `growthPressure`.
7. `growthPressure`, `salinityDensityCap`, `growable`, and `constantWitherRate` determine plant density change.
8. Healthy plants feed back into the tile by improving fertility, reducing salinity, holding soil against erosion-driven loss, adding shade, adding wind protection, or supporting moisture; non-growable plants such as `Straw Checkerboard` use the same shared contribution logic, but their current density steadily falls through `constantWitherRate`.
9. Damaged or dead plants reduce local support, which can expose nearby tiles and create a recoverable downward spiral.

