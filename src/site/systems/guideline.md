# src/site/systems/

Site-owned systems that subscribe to messages/events and mutate only the state they own.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `action_execution_system.h`: Action execution system interface.
- `action_execution_system.cpp`: Executes queued site actions using the canonical real-seconds-to-action-minutes conversion, derives action duration plus instant worker action costs from the previous-frame work-efficiency snapshot, scales authored worker meter costs from the worker tile's current local weather when the action starts including morale, reads recipe-authored craft duration and worker meter costs, emits follow-up gameplay messages, supports harvest actions for density-ready plants, and re-arms plant placement mode after successful item-based planting when matching seeds remain in the worker pack.
- `camp_durability_system.h`: Camp durability system interface.
- `camp_durability_system.cpp`: Applies camp wear/failure effects to camp durability state using content-authored wear-rate and service-threshold tuning.
- `craft_system.h`: Craft system interface.
- `craft_system.cpp`: Handles crafting progress, completion, and crafting-related state updates, including on-demand craft-context option projection plus idle-gated cached nearby-item refreshes for crafting stations.
- `device_maintenance_system.h`: Device maintenance system interface.
- `device_maintenance_system.cpp`: Applies repair/maintenance behavior for site devices and emits device-condition change messages when integrity or occupancy changes, including startup snapshots for task-tracking subscribers, now using sparse device-entity iteration instead of dense full-tile scans for per-frame wear updates.
- `device_support_system.h`: Device support system interface.
- `device_support_system.cpp`: Resolves support effects and dependencies for site devices using content-authored evaporation tuning, now by iterating sparse device entities instead of scanning the full tile grid.
- `device_weather_contribution_system.h`: Device weather contribution system interface.
- `device_weather_contribution_system.cpp`: Retains owner-specific per-tile device weather contributions across frames, invalidates dirty target tiles from device-state messages plus snapped 8-direction wind changes, and recomputes only the affected tiles by scanning nearby device anchors.
- `ecology_system.h`: Ecology system interface.
- `ecology_system.cpp`: Updates plants, burial, watering, harvest-density setbacks, broader ecology progression, and highway-target sand-cover accumulation for objective-driven site progress, including verbose plant-density change logs, temporary Site 1 starter-patch density probe logs now tagged as debug-level present logs, sparse active-ecology ECS iteration for occupied tiles, event-driven dirty handling for non-occupied/special-case updates, a separate highway/global pass, batched ecology summary messages for downstream consumers, the simplified growth-pressure computation that compares local weather and effective resistance on the same authored scale while keeping non-growable setup plants at zero weather pressure, and owner-emitted living-plant stability messages for task tracking.
- `economy_phone_system.h`: Economy phone system interface.
- `economy_phone_system.cpp`: Handles phone storefront interactions using content-authored seeded listings and delivery settings, persistent-campaign-cash-backed purchases, direct-purchase unlockable listing prices derived from authored internal cash-point values, plant-listing availability based on the two-starter plus one-by-one total-reputation unlock track, immediate delivery-crate routing for bought items, owner-confirmed buy/sell completion messages for onboarding-task progress, and task-reward money/unlockable reveal awards routed through message handling.
- `failure_recovery_system.h`: Failure recovery system interface.
- `failure_recovery_system.cpp`: Resolves failure fallout and recovery flow for the site.
- `inventory_system.h`: Inventory system interface.
- `inventory_system.cpp`: Applies item moves, uses, inventory ownership changes, explicit item-submission consumption for task hand-ins, site-start loadout seeding into the delivery crate, harvested-output insertion into the worker pack, immediate delivery-crate insertion with queued overflow retries, tracked worker-pack/device-storage panel open-close requests, owner-confirmed transfer/submit/use/craft completion messages for onboarding-task progress, and sparse device-entity iteration when ensuring device-backed storage containers.
- `local_weather_resolve_system.h`: Local weather resolve system interface.
- `local_weather_resolve_system.cpp`: Resolves final local weather each frame from base weather plus retained owner-specific plant/device contribution channels, now by iterating only the narrow tile ECS components it actually reads or owns instead of fan-out full-tile reads, marks projected plant tiles dirty when visible wind changes, emits startup plus live tile state-change messages for task tracking, and currently includes temporary Site 1 starter-patch local-weather probe logs tagged at debug level for verbose-only smoke output.
- `modifier_system.h`: Modifier system interface.
- `modifier_system.cpp`: Applies and expires site modifiers/effects using content-authored nearby-aura and run-modifier preset tables, including task-reward run-modifier awards plus imported campaign assistant and faction-tech global-modifier effects routed through message handling, and resolves terrain factor weights/biases from the active modifier totals with content-authored clamp and camp-comfort tuning.
- `phone_panel_system.h`: Phone panel system interface.
- `phone_panel_system.cpp`: Owns phone home/app-panel section state plus tracked phone open-close visibility and authoritative projected phone listings/task counts for the adapter, including completed and claimed history counts.
- `plant_weather_contribution_system.h`: Plant weather contribution system interface.
- `plant_weather_contribution_system.cpp`: Retains owner-specific per-tile plant weather contributions across frames, invalidates dirty target tiles from ecology messages plus snapped 8-direction wind changes, and recomputes only the affected tiles by scanning nearby plant/ground-cover sources, now deriving plant heat/wind/dust shelter from resistance/tolerance values for outward tiles only while leaving a plant's own footprint to rely on self resistance instead of self-fed local-weather shielding, scaling authored aura and wind ranges by plant footprint multiples, and applying authored nearby-protection ratios before writing outward shelter to neighboring tiles.
- `placement_validation_system.h`: Placement validation system interface.
- `placement_validation_system.cpp`: Validates structure placement requests against tile/world constraints.
- `site_completion_system.h`: Site completion system interface.
- `site_completion_system.cpp`: Determines site success/failure outcomes for the active site objective mode, including pure-survival time completion and green-wall hold/timer-pause evaluation.
- `site_flow_system.h`: Site flow system interface.
- `site_flow_system.cpp`: Coordinates major site flow transitions during a run, including worker motion and approach/interaction movement handling.
- `site_time_system.h`: Site time system interface.
- `site_time_system.cpp`: Owns fixed-step-derived site clock, day index, and day-phase advancement.
- `site_system_context.h`: Shared dependency bundle passed into site systems, including owner-scoped tile component read/write helpers used to keep systems within their declared ECS ownership boundaries.
- `task_board_system.h`: Task board system interface.
- `task_board_system.cpp`: Manages board listings, acceptance, task-instance resolution from single-value template fields with `0`/`None` procedural sentinels, per-task cash-point difficulty and reward-budget valuation from authored task timing/risk plus resolved item/structure choices, site-keyed onboarding-task seeding on site start, task-owned mirror caches fed only by owner-emitted buy/sell/transfer/submit/plant/craft/build and worker/tile/device/living-plant-stability state messages plus batched ecology tile summaries, duration accumulation over those subscribed conditions, message-driven living-plant non-withering checks, fixed single-option onboarding reward drafts, and site-start board reset plus tracking-cache initialization.
- `weather_event_system.h`: Weather event system interface.
- `weather_event_system.cpp`: Applies site-owned weather/event state changes, including authored default-weather baselines, temporary Site 1 startup weather probe logs tagged at debug level for verbose-only smoke output, start/peak/end timeline interpolation for additive event pressure that smooths back to baseline weather, and recurring one-sided highway-protection waves.
- `worker_condition_system.h`: Worker condition system interface.
- `worker_condition_system.cpp`: Resolves worker condition from previous-frame state plus accumulated frame deltas, recomputes derived energy-cap and work-efficiency snapshots, applies content-authored passive environment-driven meter loss with shelter softening across the requested health, hydration, and nourishment heat-wind-dust ratios, resolves hydration/nourishment-scaled background energy recovery plus max-weather-driven morale recovery/decline and resolved morale-support speed, and emits startup worker-meter snapshots for task tracking.
