# ECS Refactor Plan

This document is a working draft for refactoring the active site runtime toward an ECS-first data model.
It is intentionally not final yet. The goal is to record the current direction, the reasons behind it,
the migration phases, and the open questions that still need decisions before implementation starts.

## Goals

- Make active site gameplay data ECS-first instead of split between ECS entities and vector-backed compatibility state.
- Keep fast random access for tiles and inventory slots without losing sparse ECS iteration where it matters.
- Reduce tiny scalar components by regrouping fields into larger owner-scoped components.
- Remove gameplay-owned projection/view cache data from low-level runtime storage.
- Make inventory and craft scale through direct tile -> occupier -> storage -> slot -> item-stack traversal.

## Current Problems

- Tile, worker, and device data are spread across many tiny ECS components in `src/site/site_world_components.h`.
- Inventory is hybrid: item stacks and containers are ECS entities, but `InventoryState` still owns container metadata,
  slot arrays, worker-pack exported slot views, and pending-delivery compatibility state.
- Craft nearby-item lookup currently walks all storage containers and filters them, instead of starting from nearby tiles.
- Several site and campaign systems still use vectors/state-set slices for large collections that could become entities.
- Site-world fast lookup currently depends partly on helper indexing such as `device_entities_by_tile_index`.

## Current Direction

### 1. Tile Grid

- Keep one tile entity per tile.
- Add an explicit tile-entity grid stored as a flat array indexed by `y * width + x`.
- The tile grid is the canonical random-access path for tile lookups.
- Systems needing sparse iteration should still query archetypes/tags/components through Flecs.

### 2. Occupier Model

- Replace separate plant/device occupancy concepts with one generalized occupier model.
- A tile with no occupier should have no tile-side occupier ref component.
- A plant, ground cover, or device occupier entity should carry one occupier component that describes its footprint and anchor.
- Each occupied tile should carry one tile-side occupier ref component that points back to the occupier entity.

Proposed shape:

```cpp
enum class OccupierKind : uint8_t
{
    Plant = 1,
    GroundCover = 2,
    Device = 3
};

struct OccupierComponent
{
    OccupierKind kind;
    uint8_t footprint_kind;          // 1x1, 2x2 for now
    uint64_t anchor_tile_entity_id;  // top-left or canonical anchor tile
};

struct TileOccupierRefComponent
{
    uint64_t occupier_entity_id;
    uint64_t anchor_tile_entity_id;
    uint16_t footprint_index;        // which tile within the footprint this tile is
};
```

- `TileOccupantTag` is redundant if occupied tiles carry `TileOccupierRefComponent`.
- For 2x2 occupancy, all four tile entities should carry the same occupier entity id plus the same anchor tile entity id.

### 3. Tile Component Grouping

- Group by owner first, then by change frequency.
- Avoid mixing fields owned by different systems into one component even if their update frequency is similar.

Proposed tile components:

- `TileStaticComponent`
  - terrain id
  - packed static flags such as traversable, plantable, reserved_by_structure
- `TileSoilComponent`
  - fertility
  - moisture
  - salinity
  - sand burial
- `TileExcavationComponent`
  - excavation depth
- `TileLocalWeatherComponent`
  - heat
  - wind
  - dust
- `TilePlantContributionComponent`
  - plant-side weather and support contribution channels
- `TileDeviceContributionComponent`
  - device-side weather and support contribution channels
- `TileOccupierRefComponent`
  - occupier entity id
  - anchor tile entity id
  - footprint index

Current leaning:

- Move plant density and growth pressure off the tile and onto the plant occupier entity.
- Keep tile components focused on tile-owned state such as terrain, soil, excavation, and resolved local weather.

### 4. Plant Entities

- Plants should become first-class occupier entities.
- Plant simulation state should live on the plant entity instead of the tile.
- Tile stores occupancy/reference data plus tile-owned environmental state.

Possible plant entity grouping:

- `PlantStaticComponent`
  - plant id
  - footprint type
- `PlantPlacementComponent`
  - anchor tile entity id
  - occupied tile count or footprint kind
- `PlantSimComponent`
  - density
  - growth pressure
- `PlantWeatherSourceComponent`
  - any runtime data needed for contribution recompute

Ground cover can follow the same occupier pattern, though it may stay simpler than plants if its runtime data stays small.

Reasoning:

- Density is a property of the plant instance, even if it is deduced from occupied-tile environment.
- Growth pressure is also plant-side runtime state if the computation depends on plant tolerance/resistance, density,
  and occupied-tile environmental inputs.

### 5. Device Entities

Devices should be sparse ECS entities queried directly by device systems.

Proposed grouping:

- `DeviceStaticComponent`
  - structure id
  - anchor tile entity id
  - footprint kind
- `DeviceConditionComponent`
  - integrity
  - efficiency
- `DeviceResourceComponent`
  - stored water and any later mutable runtime resources

Proposed tags/components:

- `StorageComponent` for storage-capable entities
- `CraftStationTag`
- `FixedIntegrityTag`

This removes the need for a bool-like `DeviceFixedIntegrity` data component.

### 6. Worker Entity

Proposed grouping:

- `WorkerTransformComponent`
  - tile coord
  - local tile offset
  - facing
- `WorkerNeedsComponent`
  - health
  - hydration
  - nourishment
  - energy cap
  - energy
  - morale
- `WorkerDerivedComponent`
  - work efficiency
  - sheltered state

Open question:

- Whether sheltered should remain in derived worker state or become fully recomputed/transient.

### 7. Inventory Model

Target direction:

- Storage-capable thing is an ECS entity with `StorageComponent`.
- Item stack is an ECS entity.
- Slot lookup is not stored as projection/view data.
- Slot lookup becomes explicit runtime-owned 2D array storage keyed by storage entity id.

Proposed shape:

- The storage entity id is the storage id.
- Storage size data belongs to ECS on that storage entity.
- Runtime storage owns one slot grid per storage entity id.
- Each slot stores either `0` or an item-stack entity id.
- A storage entity id resolves directly to the slot grid that backs that storage.
- Craft and inventory traversal reads slot grids directly instead of scanning exported slot views.

Notes:

- This is still array-backed for fast random access.
- The array is runtime storage for slot references, not UI projection data.
- `container_entity_id` becomes redundant if the storage-capable ECS entity id itself is the storage id.
- First-pass assumption: one storage-capable entity owns at most one storage grid.
- The current `InventoryState` should shrink dramatically or disappear once the hybrid bridge is removed.

Proposed minimum runtime bookkeeping:

- `storage entity id -> slot grid`
- pending delivery queue or equivalent deferred insertion work
- revision counters only if they still materially simplify cache invalidation

Proposed ECS-side storage component:

```cpp
struct StorageComponent
{
    uint16_t width;
    uint16_t height;
};
```

### 8. Craft Search Path

Desired path:

1. Search nearby tile entities.
2. Read `TileOccupierRefComponent`.
3. If the occupier is a device, resolve the device entity id.
4. Check device components/tags such as `StorageComponent` and `CraftStationTag`.
5. Use that device entity id as the storage id when `StorageComponent` is present.
6. Read the slot grid for that storage entity id.
7. For each non-empty slot, add the item-stack entity id to the candidate input set.

This should replace the current "collect all containers, then filter by tile distance" approach.

Caching direction:

- Do not rescan nearby storage every frame.
- Build craft-relevant cache state on site initialization.
- Update the cache incrementally from storage/item lifecycle messages.
- Worker-pack inventory is a special additive source while the worker is actively near the crafting device.

### 9. State Conversion Rule

Collections with many elements should generally move toward ECS entities or ECS-adjacent indexed runtime storage.

Recommended conversion priority:

Highest priority:

- tile occupancy/runtime simulation data
- device runtime data
- plant runtime data
- inventory containers and slot mappings
- inventory item stacks
- craft nearby-item/device caches

Medium priority:

- contractor work orders
- task board task instances
- active site modifiers
- phone listings

Lower priority for now:

- campaign map site lists
- faction progress
- loadout planner slots
- host/exported view caches

Reasoning:

- Site runtime collections are hot and touched per frame or per frequent gameplay action.
- Campaign collections are colder and do not benefit as much from immediate ECS migration.

## Tag Guidance

Use tags when presence itself is the important signal and the `true` set is sparse.

Good candidates:

- `CraftStationTag`
- `FixedIntegrityTag`
- dirty tags used to collect sparse pending work

Do not use standalone tag pairs just to model ordinary static booleans on dense tile archetypes.

Examples of data that should stay packed inside a larger component:

- traversable
- plantable
- reserved_by_structure

## Dirty Ecology

Current dirty ecology exists to batch ecology-driven tile changes from event/message handlers before those updates are
emitted outward in batched messages.

Current behavior:

- a changed tile gets `DirtyEcologyMask`
- a changed tile gets `DirtyEcologyTag`
- a later pass queries only dirty tiles
- the batch is emitted
- the dirty tag and mask are removed

This is not inherently wrong. The open question is whether the same batching behavior should survive in the new model,
or whether downstream consumers can move to direct ECS reads and reduce the need for explicit dirty batching.

Current leaning:

- keep the concept of sparse dirty work queues
- re-evaluate whether the outward projection-style batching still needs to exist after projection cleanup

### 11. Storage Tags

- If storage lives directly on the storage-capable entity, `StorageContainerTag` becomes redundant beside `StorageComponent`.
- `StorageItemTag` currently appears to have no meaningful query-driven use.
- Current leaning: remove `StorageItemTag` unless a real sparse-query use appears during the refactor.

### 12. Ecology Split

- The current ecology system does too much work.
- It should be split into at least two systems:

1. `TileEcologySystem`
   - owns tile soil/ecology values such as fertility, moisture, salinity, and sand burial
   - resolves tile-side environmental deltas
2. `PlantSimulationSystem`
   - owns plant entity simulation values such as density and growth pressure
   - reads occupied-tile environmental inputs
   - clears occupancy when plant state says the plant is gone

Possible later follow-up:

- `OccupancySyncSystem`
  - keeps tile occupier refs and occupier entity placement state consistent after placement, death, or removal

## Current Open Questions

- Whether we need any occupier specialization at all beyond the shared `OccupierComponent`.
- How much of the current split runtime/state-set compatibility layer should survive for testing and ABI export.
- Whether task board, modifiers, and economy should become ECS entities immediately or in a later phase.
- Whether dirty ecology batching remains necessary after host/projection cleanup.

## Migration Phases

### Phase 1: SiteWorld Foundations

- Add explicit flat tile-entity grid storage.
- Introduce grouped tile/device/worker components beside the old ones.
- Add generalized occupier component.
- Add device capability data/tags such as `StorageComponent`, `CraftStationTag`, `FixedIntegrityTag`.

### Phase 2: Tile Occupancy Rewrite

- Move tile occupancy logic to `TileOccupierRefComponent`.
- Remove `TileOccupantTag` once all call sites use occupier presence instead.
- Replace helper lookups such as `device_entities_by_tile_index` with tile-grid + occupier access.

### Phase 3: Inventory Rewrite

- Introduce storage-id keyed slot-grid runtime storage.
- Stop maintaining projection slot mirrors in low-level inventory runtime code.
- Remove `StorageContainerState::slot_item_instance_ids` and related bridge code once replacement is live.
- Rework inventory actions to operate through owner entity + `storage_id` + slot grid + item stack entity ids.

### Phase 4: Craft Rewrite

- Rewrite nearby search to start from nearby tiles.
- Resolve nearby device occupiers through the tile occupier component.
- Use `StorageComponent` and `CraftStationTag` instead of broad container scans.
- Replace full rebuild scans with message-driven incremental cache maintenance.
- Remove craft compatibility caches that only exist because inventory is hybrid.

### Phase 5: Plant/Device Entity Promotion

- Decide and implement whether plant simulation fields move from tile components to plant entities.
- Normalize 1x1 and 2x2 occupier footprint handling around anchor tile plus occupied-tile references.

### Phase 6: Broader Collection Migration

- Evaluate ECS conversion of task instances, work orders, active modifiers, and phone listings.
- Keep campaign/runtime export data out of the critical-path refactor until site runtime is stable.

## Suggested Immediate Next Step

Before code changes start, finalize these three design decisions:

1. Exact `TileOccupierRefComponent` payload:
   final fields for occupier id, anchor tile, and any footprint helpers.
2. Exact plant entity schema:
   what the minimal plant-side runtime state set should be.
3. Exact inventory slot storage:
   final runtime structure for `storage_id -> slot grid -> item stack entity id`.

Once those three are locked, implementation can begin with Phase 1.
