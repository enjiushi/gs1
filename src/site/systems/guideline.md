# src/site/systems/

Site-owned systems that subscribe to messages/events and mutate only the state they own.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `action_execution_system.h`: Action execution system interface.
- `action_execution_system.cpp`: Executes queued site actions using the canonical real-seconds-to-action-minutes conversion, derives action duration plus instant worker action costs from the previous-frame work-efficiency snapshot, emits follow-up gameplay messages, and re-arms plant placement mode after successful item-based planting when matching seeds remain in the worker pack.
- `camp_durability_system.h`: Camp durability system interface.
- `camp_durability_system.cpp`: Applies camp wear/failure effects to camp durability state.
- `craft_system.h`: Craft system interface.
- `craft_system.cpp`: Handles crafting progress, completion, and crafting-related state updates.
- `device_maintenance_system.h`: Device maintenance system interface.
- `device_maintenance_system.cpp`: Applies repair/maintenance behavior for site devices.
- `device_support_system.h`: Device support system interface.
- `device_support_system.cpp`: Resolves support effects and dependencies for site devices.
- `ecology_system.h`: Ecology system interface.
- `ecology_system.cpp`: Updates plants, burial, watering, broader ecology progression, and highway-target sand-cover accumulation for objective-driven site progress, including the factor/weight/bias-based terrain meter computation that reads resolved tile contribution state.
- `economy_phone_system.h`: Economy phone system interface.
- `economy_phone_system.cpp`: Handles phone storefront interactions using content-authored seeded listings and delivery settings, persistent-campaign-cash-backed purchases, plant-listing availability based on the starter-plus-total-reputation unlock track, immediate delivery-crate routing for bought items, owner-confirmed buy/sell completion messages for onboarding-task progress, and task-reward money/unlockable reveal awards routed through message handling.
- `failure_recovery_system.h`: Failure recovery system interface.
- `failure_recovery_system.cpp`: Resolves failure fallout and recovery flow for the site.
- `inventory_system.h`: Inventory system interface.
- `inventory_system.cpp`: Applies item moves, uses, inventory ownership changes, site-start loadout seeding into the delivery crate, immediate delivery-crate insertion with queued overflow retries, tracked worker-pack/device-storage panel open-close requests, and owner-confirmed transfer/use/craft completion messages for onboarding-task progress.
- `local_weather_resolve_system.h`: Local weather resolve system interface.
- `local_weather_resolve_system.cpp`: Resolves local weather updates into owned site state, including resolved per-tile support channels for heat/wind/dust/fertility/salinity/irrigation, directional lee-side wind shelter with nonlinear range falloff, and plant-tile projection dirties when visible local wind changes.
- `modifier_system.h`: Modifier system interface.
- `modifier_system.cpp`: Applies and expires site modifiers/effects using content-authored nearby-aura and run-modifier preset tables, including task-reward run-modifier awards plus imported campaign assistant/global-modifier technology effects routed through message handling, and resolves terrain factor weights/biases from the active modifier totals.
- `phone_panel_system.h`: Phone panel system interface.
- `phone_panel_system.cpp`: Owns phone home/app-panel section state plus tracked phone open-close visibility and authoritative projected phone listings/task counts for the adapter, including completed and claimed history counts.
- `placement_validation_system.h`: Placement validation system interface.
- `placement_validation_system.cpp`: Validates structure placement requests against tile/world constraints.
- `site_completion_system.h`: Site completion system interface.
- `site_completion_system.cpp`: Determines site success/failure outcomes for the active site objective mode, including pure-survival time completion and green-wall hold/timer-pause evaluation.
- `site_flow_system.h`: Site flow system interface.
- `site_flow_system.cpp`: Coordinates major site flow transitions during a run, including worker motion and approach/interaction movement handling.
- `site_time_system.h`: Site time system interface.
- `site_time_system.cpp`: Owns fixed-step-derived site clock, day index, and day-phase advancement.
- `site_system_context.h`: Shared dependency bundle passed into site systems.
- `task_board_system.h`: Task board system interface.
- `task_board_system.cpp`: Manages board listings, acceptance, onboarding-task progress tracking, reward-draft seeding, reward-claim routing, claimed-history transitions, and reset flow, keeping the site-one onboarding override plus restoration task on dense-restoration sites.
- `weather_event_system.h`: Weather event system interface.
- `weather_event_system.cpp`: Applies incoming weather events to site-owned weather/event state, including start/peak/end timeline interpolation and recurring one-sided highway-protection waves.
- `worker_condition_system.h`: Worker condition system interface.
- `worker_condition_system.cpp`: Resolves worker condition from previous-frame state plus accumulated frame deltas, recomputes derived energy-cap and work-efficiency snapshots, and applies passive environment-driven meter loss with shelter softening.
