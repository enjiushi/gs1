# Refactor Todo

- [x] Remove the broad runtime compatibility path for aggregate `CampaignState` and `SiteRunState` so production runtime/campaign flow mutates split state directly instead of round-tripping through assembled aggregate state.
- [x] Replace the remaining compatibility-only pointer-shaped handle with explicit runtime-owned access where practical by preserving the live `SiteWorld` through `RuntimeInvocation` and split-state test fixtures instead of reconstructing active site runs with a dropped world pointer.
- [x] Keep the stronger `StateManager` default-resolver baseline as future work; the current first-owner-becomes-default registration rule remains the active GS1 behavior until a later task explicitly funds the broader resolver baseline pipeline.
- [x] Accept the remaining MSVC `C4324` alignment warnings for the intentional `alignas(64)` state-set wrappers; do not suppress them for now because they reflect an intentional layout rule rather than an accidental packing issue.
- [x] Sweep the remaining split-state test helpers so they preserve the live `SiteWorld` handle across fixture round-trips while the remaining aggregate helper surface stays test-only.
- [x] Refactor the last broad production compatibility cluster in `inventory`, `task_board`, and `action` so those systems use split state refs and flattened authoritative storage directly instead of legacy aggregate compatibility shapes.
- [x] Restore a clean compile baseline after the production refactor by updating runtime site-view flattening, split-ref conversions, and the remaining inventory/action/task-board callers to the current state-set contract.
- [ ] Remove the remaining narrow aggregate compatibility helpers once tests and view rebuilds stop depending on them.
  Target: `src/runtime/runtime_split_state_compat.h`.
- [ ] Stop assembling temporary aggregate `CampaignState` / `SiteRunState` values in non-production callers that still use the compatibility surface.
  Progress: `src/runtime/game_state_view.cpp` now rebuilds its cache directly from flat state sets without reassembling temporary aggregate site/campaign slices.
  Remaining target: `tests/system/source/system_test_fixtures.h` and runtime/system tests that still seed aggregate `site_run` state directly.
- [ ] Remove the last aggregate convenience bridges that only exist to adapt `SiteRunState` callers onto split inventory/action helpers after the test-side compatibility cleanup is finished.
  Target: `src/site/inventory_storage.cpp` `SiteRunState` adapter helpers and any now-redundant aggregate overloads.
