# GS1 System Test Coverage

This document tracks the per-system coverage expected from the source-authored
system tests under `tests/system/source/` and the asset-authored regression
cases under `tests/system/assets/`.

Cross-system runtime regression scenarios also live under `tests/runtime/`.
Those tests are not replacements for per-system coverage; they verify the
runtime message/projection seams where several independently owned systems are
expected to coordinate through queued `GameMessage` flow.

It complements [SYSTEM_TEST_MANUAL.md](E:/gs1/SYSTEM_TEST_MANUAL.md):

- The manual explains how the framework works.
- This document explains what each system is expected to verify.

## Shared Test Pattern

Most source tests use the shared helpers in
[tests/system/source/system_test_fixtures.h](E:/gs1/tests/system/source/system_test_fixtures.h).

Those helpers intentionally support three setup styles:

- Prototype campaign setup via `CampaignFactory` for campaign-flow coverage.
- Minimal site-run setup for isolated site-system tests.
- Direct ECS mutation for tile, device, and worker components so ECS-backed
  systems can be exercised without bypassing ownership rules in production code.

The ECS-oriented helper pattern is:

- Build a `SiteRunState` with a real `SiteWorld`.
- Seed tile, device, or worker state through `SiteWorld` mutators or direct
  component mutation helpers.
- Run the target system.
- Assert on owned state, emitted `GameMessage` flow, and projection-dirty flags.

## Coverage Policy

Each system should have at least one source-authored test registration.

Coverage is split into two categories:

- Implemented behavior coverage:
  Verifies logic that exists in code today.
- Placeholder or forward-looking coverage:
  Documents gameplay intent from the design/system docs when the system is still
  a no-op or thinner than the design. These tests should currently pass by
  asserting the present no-op or limited behavior while making the missing
  expectation explicit in this document.

Asset-authored regression coverage is especially useful when:

- the setup is mostly data variation
- we want a compact matrix of valid and invalid cases
- we want bug reproductions to stay cheap to add after future fixes

Runtime message-chain coverage is especially useful when:

- several systems must cooperate through subscribed message dispatch
- the order of fixed-step updates, projection flushing, and queued-message
  dispatch matters
- we want to verify the engine-facing message stream in addition to owned state

## Runtime Message-Chain Coverage

The runtime regression layer currently verifies:

- `InventoryItemUseRequested -> WorkerMeterDeltaRequested -> WorkerMetersChanged`
  updates inventory state, mutates worker hydration, and emits the expected
  worker/HUD projection output.
- `EcologySystem::run -> RestorationProgressChanged -> TaskBoardSystem ->
  SiteAttemptEnded -> CampaignFlowSystem` completes a site-one restoration
  chain, transitions the app to `SITE_RESULT`, reveals the adjacent site, and
  emits site-result engine messages.

## Campaign Systems

### `campaign_flow`

Implemented behavior coverage should verify:

- Opening the main menu updates app state.
- Starting a new campaign creates a prototype campaign, resets active site run,
  rebuilds map caches, and syncs `campaign.app_state`.
- Selecting an available site updates `selected_site_id` and emits
  `DeploymentSiteSelectionChanged`.
- Re-selecting the same site is idempotent.
- Selecting a missing or unavailable site returns the correct status.
- Clearing selection resets the selection and emits
  `DeploymentSiteSelectionChanged` with `selected_site_id = 0`.
- Starting a site attempt for an available site increments attempt count,
  creates a site run, sets `active_site_id`, switches to `SITE_ACTIVE`, and
  emits `SiteRunStarted`.
- Returning to the regional map clears the active site run and restores map
  state.
- Ending a site attempt with `COMPLETED` marks the site completed, reveals
  adjacent locked sites, records newly revealed count on the result state, and
  moves to `SITE_RESULT`.
- Completed-site resolution queues both campaign-reputation and featured-faction
  reputation award requests when the site metadata defines those rewards.
- Ending a site attempt with `FAILED` moves to `SITE_RESULT` without unlocking
  adjacent sites.
- Fixed-step `run()` advances campaign time and decreases remaining days only
  after full-day thresholds.

### `loadout_planner`

Implemented behavior coverage should verify:

- The system rebuilds selection/loadout state from
  `SelectDeploymentSite`, `ClearDeploymentSiteSelection`, and
  `DeploymentSiteSelectionChanged`.
- A non-zero selected site updates `selected_target_site_id`.
- Selecting a site rebuilds adjacent completed-site support into exported items,
  nearby aura ids, support quota, and the deployment loadout.
- A zero selected site clears `selected_target_site_id`.
- Unrelated messages are ignored.

### `technology`

Implemented behavior coverage should verify:

- Faction reputation awards never decrease a faction's lifetime total reputation.
- Campaign-wide total reputation unlocks the three prototype plant tiers without being spent.
- Neutral base-tech tiers are gated only by authored total-reputation requirements.
- Each faction exposes three prototype enhancement tiers gated by authored faction-reputation requirements.
- Reaching a total-reputation tier threshold unlocks that tier's authored plant pair without spending reputation.
- Base technologies become claimable as soon as the matching total-reputation tier threshold is met; they do not require previous-tier purchases.
- Enhancements require both the paired base tech purchase and the matching faction-reputation tier.
- Prototype technologies cover placeholder base-tech and enhancement modifier/mechanism effects with authored per-node cash costs.
- Purchased tech state is stored as node ownership, not as unspent pick inventory.

### `regional_support`

Current placeholder coverage should verify:

- The system has no gameplay implementation yet.
- Test coverage records current no-op expectations only.

Future design-driven coverage should include:

- Regional support package availability and export logic.
- Support-package effects flowing into campaign and site setup.

## Site Systems

### `site_flow`

Implemented behavior coverage should verify:

- Site time advances by the fixed site-step minute amount.
- Day index and day phase update across dawn/day/dusk/night boundaries.
- Worker movement ignores missing input, zero-length input, and out-of-bounds
  targets.
- Worker movement respects traversability.
- Successful movement updates worker tile position, facing, and worker
  projection dirty flags.

### `site_completion`

Implemented behavior coverage should verify:

- The system emits `SiteAttemptEnded(COMPLETED)` once the fully-grown tile count
  meets the site threshold.
- `Pure Survival` emits `SiteAttemptEnded(COMPLETED)` once the authored time
  limit expires.
- `Green Wall Connection` completes only after a connected planted path is
  established and the target protection band stays stable for the authored hold
  duration.
- `Green Wall Connection` resets its hold countdown if target-band sand rises
  again, and its main objective timer pauses while the hold countdown is active.
- It does not emit when the run is not active, when the threshold is zero, when
  progress is below threshold, or when a pending site transition message already
  exists.

### `failure_recovery`

Implemented behavior coverage should verify:

- The system emits `SiteAttemptEnded(FAILED)` when worker health reaches zero.
- It does not emit when the run is inactive, when worker health is still above
  zero, when no worker entity exists, or when a pending transition message is
  already queued.

### `action_execution`

Implemented behavior coverage should verify:

- Invalid, build, and repair action requests fail with the current
  `InvalidState` behavior.
- Out-of-bounds targets fail with `InvalidTarget`.
- Starting a new action while another action is active fails with `Busy`.
- Water and burial-clear actions begin immediately, emit `SiteActionStarted`,
  emit worker meter costs, and populate action state.
- Action worker meter costs scale from the worker's current local heat, wind,
  and dust using the authored weather-to-meter action coefficients, including
  zero-baseline morale scaling that uses the strongest weather channel instead
  of summing all three.
- Plant actions request a placement reservation first instead of starting
  immediately.
- `PlacementReservationAccepted` transitions a waiting plant action into active
  execution and emits the expected start/cost messages.
- `PlacementReservationRejected` fails the waiting action and clears action
  state.
- Cancelling the current action emits `SiteActionFailed(Cancelled)`, releases any
  reservation, and clears action state.
- Running the system advances action timers.
- Completing an action emits `SiteActionCompleted` plus the correct follow-up
  gameplay fact message:
  `SiteGroundCoverPlaced`, `SiteTileWatered`, or `SiteTileBurialCleared`.

### `placement_validation`

Implemented behavior coverage should verify:

- `SiteRunStarted` prunes reservation records from other runs.
- Out-of-bounds placement requests are rejected.
- Ground-cover placement rejects blocked terrain, structure-reserved tiles,
  occupied tiles, and already-reserved tiles.
- Structure placement rejects non-traversable tiles and occupied structure
  slots.
- Valid requests emit `PlacementReservationAccepted` and allocate a reservation
  token.
- Releasing by action id or reservation token frees the reservation so the tile
  can be accepted again.

### `ecology`

Implemented behavior coverage should verify:

- Ground-cover placement updates occupancy and density, clears plant occupants,
  emits `TileEcologyChanged`, and marks the tile projection dirty.
- Planting updates plant occupancy, clears ground cover, sets density, and emits
  `TileEcologyChanged`.
- Watering only affects occupied tiles and increases density without exceeding
  the max.
- Burial clearing reduces sand burial without going below zero and emits the
  correct change mask.
- Passive `run()` growth increases occupied-tile density over time.
- `run()` counts fully grown tiles and emits `RestorationProgressChanged` when
  that count changes.
- Tiles without occupants do not grow.

### `inventory`

Implemented behavior coverage should verify:

- `SiteRunStarted` seeds the site-one starter inventory only when the inventory
  is still empty.
- Non-site-one runs do not receive the site-one starter inventory.
- Delivery requests try to place their stacks into the delivery crate
  immediately and queue only the overflow that cannot fit yet.
- Queued delivery overflow is retried as soon as delivery-crate space opens.
- `run()` resizes slot vectors to match configured slot counts.
- Using an item validates slot existence, occupancy, item id, and quantity.
- Supported item uses emit `WorkerMeterDeltaRequested`.
- Consuming the full stack clears the slot.
- Transfers reject invalid source/destination pairs and mismatched item merges.
- Valid transfers move or merge stacks and dirty inventory projection state.

### `economy_phone`

Implemented behavior coverage should verify:

- `SiteRunStarted` seeds site-one money, listings, and unlockables.
- Non-site-one runs are reset to an empty/default economy state.
- Buying from a buy listing spends money and decrements limited quantity.
- Successful buy-item purchases route inventory through the delivery crate
  immediately instead of waiting on a timed arrival.
- Selling to a sell listing increases money and decrements limited quantity.
- Contractor hire spends money and decrements available work units.
- Unlockable purchase removes the unlockable from direct-purchase availability
  and reveals it in the owned list.
- Missing listings return `NOT_FOUND`.
- Wrong listing kinds or insufficient funds/quantity return invalid-state
  failures.

### `task_board`

Implemented behavior coverage should verify:

- `SiteRunStarted` seeds the site-one task list from the restoration task plus
  the current onboarding pool for buy, sell, transfer, plant, craft, and
  consume actions.
- Non-site-one runs clear the board and keep it empty.
- Accepting a visible task moves it to the accepted list.
- Accepting a task above the cap or re-accepting a non-visible task has no
  effect.
- `RestorationProgressChanged` updates progress amounts.
- Visible-progress updates alone do not auto-complete a task before the player
  accepts it.
- Hitting the task target completes the task, adds it to `completed_task_ids`,
  removes it from `accepted_task_ids`, and queues the guaranteed publisher
  faction-reputation award.
- Successful onboarding action messages such as purchase and transfer completion
  advance the matching accepted onboarding task.
- Site-start task seeding also instantiates the visible reward draft options for
  the task.
- Claiming a completed-task reward marks the chosen option selected and routes
  the reward effect through the owning gameplay system for money, unlockable,
  and run-modifier prototype rewards.
- Item-delivery task rewards route through the delivery crate immediately and
  use the pending queue only for overflow that does not currently fit.
- Claiming a reward moves the task into the claimed/history bucket instead of
  leaving it in the live completed list.

### `weather_event`

Implemented behavior coverage should verify:

- `SiteRunStarted` seeds the site-one event/weather state only when no event is
  active.
- Other sites or already-active event states are ignored.
- `run()` interpolates event pressure from `startTime` to `peakTime`, holds for
  `peakDuration`, and then interpolates back down to `endTime`.
- Passing `endTime` clears the active event, zeroes pressures, and returns site
  weather to its baseline track.
- Projection dirty flags are raised when the weather/event state changes.

### `local_weather_resolve`

Implemented behavior coverage should verify:

- The system subscribes to `TileEcologyChanged`, `SiteDevicePlaced`,
  `SiteDeviceBroken`, and `SiteDeviceRepaired`.
- Ecology and shelter-device changes queue local-weather neighborhood refreshes
  through `process_message()`.
- `run()` writes per-tile local heat, wind, and dust from site weather plus
  occupant-density/cover adjustments.
- Covered tiles reduce wind and dust relative to exposed tiles.
- Exposed tiles receive the exposed dust bias.

### `worker_condition`

Implemented behavior coverage should verify:

- The system subscribes only to `WorkerMeterDeltaRequested`.
- The first observed worker update emits a full-mask `WorkerMetersChanged`.
- Requested meter deltas are applied with clamping.
- Passive `run()` drains hydration, nourishment, and energy over time, and
  moves morale up or down from the current max local-weather pressure.
- Positive resolved morale support should add to the final background morale
  speed after the weather-driven morale result is computed.
- Zero hydration or nourishment starts health decay.
- When nothing changes, no message is emitted.
- Projection dirty flags are raised when a worker update is emitted.

### `device_support`

Implemented behavior coverage should verify:

- The system currently subscribes to no messages and `process_message()` is a
  no-op.
- Devices derive efficiency from integrity.
- Higher tile heat increases water evaporation over time.
- Devices without a structure id collapse to zero efficiency and zero stored
  water.
- Non-positive step times do not mutate state.

### `device_maintenance`

Implemented behavior coverage should verify:

- The system subscribes to `SiteDevicePlaced` and `SiteDeviceRepaired`.
- `SiteDevicePlaced` seeds placed-device footprint tiles with the new structure
  and full integrity.
- `SiteDeviceRepaired` restores the existing structure footprint to full
  integrity.
- Weather intensity and sand burial reduce device integrity over time.
- Integrity clamps at zero and one.
- Devices without a structure id are ignored.
- Non-positive step times do not mutate state.

### `camp_durability`

Implemented behavior coverage should verify:

- `SiteRunStarted` resets camp durability and service flags to the expected
  defaults.
- `run()` applies wear from baseline weather and event intensity.
- Crossing the protection, delivery, and shared-storage thresholds flips the
  corresponding service flags.
- Durability changes large enough to cross projection thresholds dirty the camp
  projection state.
- Switching site runs resets internal memory to the new run.

### `modifier`

Implemented behavior coverage should verify:

- `SiteRunStarted` clears modifier state, imports nearby aura modifiers from the
  campaign loadout planner, imports unlocked assistant packages plus purchased
  base-tech or enhancement modifiers, and resolves totals.
- `run()` recomputes totals from nearby auras, run modifiers, and camp comfort.
- Channel totals clamp to the configured range.
- Projection dirty flags are raised when resolved totals change.

## Empty Or Thin Systems

The following systems are currently placeholders or materially thinner than the
design docs:

- `technology`
- `regional_support`

The code tests should keep current behavior stable, while this document keeps
the larger intended coverage visible so these systems can expand without losing
track of the test plan.
