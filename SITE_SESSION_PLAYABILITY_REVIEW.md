# Site Session Playability Review

Checked: April 13, 2026

Purpose: summarize the current implementation state and define the next work that would make one active site session feel playable.

## Bottom Line

The current build proves the application flow and presentation bridge, but it is not yet a playable restoration session.

What works today:

- The game boots into the main menu.
- A campaign can be started.
- The regional map shows the authored four-site prototype chain.
- Site 1 can be selected and entered.
- The runtime enters `SITE_ACTIVE`.
- The visual smoke host receives a site bootstrap snapshot.
- The worker can move around the 12x12 site with live delta projection.

What is missing for playability:

- The player cannot perform restoration actions.
- No task-board choices exist during the site.
- No items, phone listings, purchases, or inventory decisions exist.
- Weather and harsh events do not create pressure.
- Ecology does not grow, degrade, or complete the site.
- Health, hydration, energy, money, tasks, completion, and weather remain static during normal play.
- The site cannot be completed or failed through normal player interaction.

The practical current state is an interactive site-scene slice, not a game loop.

## Live Smoke Snapshot

I launched the current visual smoke host and drove the live flow through:

1. `MAIN_MENU`
2. `START_NEW_CAMPAIGN`
3. `REGIONAL_MAP`
4. `SELECT_DEPLOYMENT_SITE` for Site 1
5. `START_SITE_ATTEMPT`
6. `SITE_ACTIVE`

Observed site state after entry:

- App state: `SITE_ACTIVE`
- Selected site: `1`
- Site archetype: `101`
- Grid: `12x12`
- Camp tile: `(4,4)`
- Worker tile: `(4,4)`
- Worker health: `100`
- Worker hydration: `100`
- Worker energy: `100`
- Money: `0`
- Active task count: `0`
- Site completion: `0%`
- Weather: heat `0`, wind `0`, dust `0`, event phase `NONE`
- Tiles: all default terrain, no plants, no structures, no ground cover, no sand burial

Movement check:

- The worker moved from `(4,4)` to about `(7.73,4)` through live site-control input.
- The worker facing updated to `90` degrees.
- HUD values stayed unchanged.

This confirms that presentation, input, fixed-step movement, and site delta projection are working, while gameplay systems are still dormant.

## Current Implementation Inventory

### App Flow

Implemented:

- Boot to main menu.
- Start prototype campaign.
- Regional map snapshot.
- Site selection overlay.
- Start site attempt.
- Site result overlay commands exist for completion/failure.
- Return to regional map command exists.

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
- `WeatherState`
- `EventState`
- `TaskBoardState`
- `ModifierState`
- `EconomyState`
- `ActionState`
- `SiteCounters`

Important limitation:

- Most state containers are populated with defaults and are not yet driven by systems.

### Runtime Loop

Implemented:

- Fixed-step accumulation.
- Campaign/site clock advancement.
- Day phase calculation.
- Worker movement from `GS1_HOST_EVENT_SITE_MOVE_DIRECTION`.
- Movement clamping to site bounds.
- Traversability check.
- Dirty projection for worker movement.
- Completion/failure command checks.

Important limitation:

- The fixed step does not run the planned site systems yet.
- The completion check depends on `fully_grown_tile_count`, but no implemented system increases it.
- The failure check depends on health reaching `0`, but no implemented system drains health.

### Site Systems

Headers exist for the intended systems:

- `SiteFlowSystem`
- `ActionExecutionSystem`
- `WeatherEventSystem`
- `WorkerConditionSystem`
- `EcologySystem`
- `TaskBoardSystem`
- `FailureRecoverySystem`
- Device, camp, inventory, economy, placement, modifier, and local-weather systems

Current status:

- These are scaffolds only. No `run(...)` implementations were found, and the runtime does not call them.

### Engine Projection

Implemented:

- Regional map snapshot/upsert/link commands.
- Site snapshot begin/end.
- Site tile upsert.
- Site worker update.
- Site camp update.
- Site weather update.
- HUD state.
- Site result ready.
- Smoke host live JSON for app state, regional map, site bootstrap, site state, HUD, and site result.
- Visual smoke viewer that renders main menu, regional map, active site placeholder, site tiles, camp, and worker.

Important limitation:

- Engine command enum values already exist for inventory slots, tasks, and phone listings, but the gameplay runtime does not currently emit those projections.
- Site state JSON only exposes the current projected slices; it does not expose tasks, phone listings, inventory, action progress, or detailed tile meters yet.

### Player Input

Implemented:

- UI actions for start campaign, select deployment site, start site attempt, clear deployment site selection, and return to regional map.
- Site movement input through host event.

Missing:

- No public host event or UI action exists for site gameplay actions such as plant, build, water, repair, clear burial, accept task, claim reward, buy item, sell item, hire contractor, or purchase site unlockable.

## Playability Gap

For one site session to feel playable, the player needs a loop where intent changes the world and the world pushes back.

The current build has:

- place: yes
- avatar movement: yes
- visible site state: partial
- goals: only an invisible completion threshold
- actions: no
- pressure: no
- economy: no
- rewards: no
- win/loss by play: no

Minimum playable loop needed:

1. Enter a site with a visible immediate goal.
2. Move to a tile.
3. Select a tile/action.
4. Spend time/resources to change that tile.
5. See tile, HUD, and task progress update.
6. Weather/time creates survival or restoration pressure.
7. Repeating the loop can complete or fail the site.

Until that loop exists, adding more regional-map, tech, or content breadth will not make the site session feel meaningfully more playable.

## Recommended Next Implementation Sequence

### 1. Add a Minimal Site Action Contract

Goal: let the host tell gameplay "perform this action on this tile."

Add only the smallest contract needed for the first playable loop:

- `StartSiteAction`
- action kind: `Plant`, `Water`, `ClearBurial`
- target tile
- optional subject id, such as plant id or item id
- optional cancel action only if needed for UX

Recommended first action:

- Plant starter ground cover or straw checkerboard on a target tile.

Why first:

- Movement already works.
- Tile projection already works.
- The player needs one direct action that visibly mutates the site.
- This unlocks meaningful tests for tile updates, HUD updates, and completion progress.

Avoid for this step:

- Full phone economy.
- Full task-board interactions.
- Full content loader.
- Full recipe system.

### 2. Seed Prototype Site 1 With Micro-Content

Goal: Site 1 should start with enough state to play a tiny restoration loop.

Add static prototype content first:

- One ground-cover or plant option.
- One starter item stack if the action consumes items.
- One camp storage or worker pack entry.
- One simple weather profile.
- One visible site goal.
- One or two terrain/sand-burial variations so tiles are not all identical.

Good first-site shape:

- Site 1 has a small bare-sand staging ground around camp.
- Several nearby tiles are plantable and easy.
- A few edge tiles have higher sand burial or lower moisture.
- The completion threshold remains small enough for fast smoke testing.

### 3. Implement `ActionExecutionSystem` First

Goal: convert player action intent into visible state changes.

First playable behavior:

- Validate target tile bounds.
- Validate plantable/traversable rules.
- Start an action timer.
- Consume energy and/or hydration.
- Optionally reserve one item.
- On completion, write `plant_type_id`, `ground_cover_type_id`, `plant_density`, or `sand_burial`.
- Mark changed tile and HUD dirty.

This should immediately prove:

- site action command path
- tile mutation
- action state
- HUD dirty updates
- live visual feedback

### 4. Implement a Tiny Ecology Completion Loop

Goal: give the player a way to finish the site through restoration.

First behavior:

- Run ecology on a short pulse.
- Increase plant density for planted/protected tiles.
- Count tiles at or above a simple "fully grown" threshold.
- Update `fully_grown_tile_count`.
- Mark affected tiles and HUD dirty.
- Let the existing completion command path fire when the threshold is reached.

This does not need full plant species simulation yet. It only needs enough growth/progress to make repeated planting and tending lead to completion.

### 5. Add Survival Pressure

Goal: make in-site time matter.

First behavior:

- Heat drains hydration.
- Movement or actions drain energy.
- Low hydration or energy eventually hurts health.
- Shelter/camp can reduce pressure near the camp tile.
- HUD updates whenever meters change.
- Health reaching `0` uses the existing failure path.

Keep tuning gentle for Site 1. The first site should teach pressure, not punish exploration.

### 6. Add One Weather Event

Goal: make the site feel alive and create a short-term decision.

First behavior:

- One scripted or deterministic mild heat/wind/dust event during Site 1.
- Event phases: warning, peak, aftermath.
- During peak, increase hydration drain or sand-burial pressure.
- Expose event phase through the existing weather projection.

This can be deterministic before task/reward randomness exists.

### 7. Add the Smallest Task Board

Goal: provide an explicit short-term goal and reward.

First behavior:

- Seed one visible task at site entry.
- Auto-accept it or add one accept command.
- Example: "Stabilize 3 nearby tiles."
- Progress when target tiles are planted or reach density threshold.
- Reward money or one item bundle.
- Project task state through `SITE_TASK_UPSERT`.

This should come after actions/ecology, because tasks need real gameplay events to observe.

### 8. Add a Tiny Phone/Economy Slice

Goal: make money immediately usable.

First behavior:

- One buy listing, such as water or starter seedlings.
- One command to buy listing.
- Money decreases.
- Item appears in inventory/camp storage.
- Project phone listing and inventory slot updates.

This should come after at least one task reward, so the player can earn and spend inside the same site.

### 9. Extend to the Four-Site Prototype Arc

Goal: prove that completing one site changes what comes next.

After Site 1 is playable:

- Let completion reveal Site 2 through the existing campaign path.
- Add a slightly harsher Site 2 profile.
- Add one between-site reward or tech choice only after the site loop itself works.
- Repeat for Sites 3 and 4 when the first two sites can be completed by play.

## Suggested Immediate Sprint

The best next sprint is not "build all systems." It is:

1. Add `StartSiteAction` command/host contract.
2. Add one prototype plant or ground-cover definition.
3. Seed Site 1 with starter inventory and a tiny completion target.
4. Implement planting as the first `ActionExecutionSystem` behavior.
5. Implement simple growth/progress and completion.
6. Add a smoke test that starts Site 1, performs the action, observes tile mutation, and reaches completion with accelerated timings.

This would turn the current slice from "walk around an empty site" into "restore a few tiles and finish the site."

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

## Risks To Watch

- Do not build the full task board before there are real action/ecology events for tasks to track.
- Do not expand regional/campaign complexity before Site 1 has a satisfying action-pressure-progress loop.
- Keep prototype content static until the shape is fun; a loader/generator can come later.
- Keep new command payloads POD-like and small, matching the existing command style.
- Every gameplay state mutation should mark the right projection dirty, or the visual smoke UI will hide working gameplay.

## Decision Recommendation

Make Site 1 playable before broadening anything else.

The current foundation is good: app state, map flow, site entry, movement, projection, and live smoke UI are in place. The next valuable work is the first complete gameplay loop inside `SITE_ACTIVE`: action command, tile mutation, meter cost, growth/progress, and completion.
