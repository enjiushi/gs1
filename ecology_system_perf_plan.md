# EcologySystem Performance Plan

## Goal

Reduce `EcologySystem` frame cost substantially in dense-cover scenarios, including `Debug` builds, while preserving ECS ownership rules and keeping the system testable.

## Current Findings

- `EcologySystem` still runs as a full tile-grid pass over a `32x32` site every fixed step.
- The recent refactor removed the biggest ownership violation:
  `EcologySystem` now reads/writes ecology-owned components directly instead of round-tripping full `TileData`.
- Even after that fix, the system is still expensive because the core model is:
  full-grid scan + per-tile ecology math + per-tile message emission.
- In dense-cover, almost every tile is occupied, so the fast path for empty tiles barely helps.
- `TileEcologyChanged` fan-out is still high and can wake downstream systems many times per frame.

## Improvement Plan

### Phase 1: Cheap Wins

1. Add ECS query-based iteration for occupied ecology tiles.
   - Replace the main `for index in tile_count` occupied-tile path with a Flecs query over:
     `TileCoordComponent`, `TilePlantSlot`, `TileGroundCoverSlot`, `TilePlantDensity`, `TileSoilFertility`, `TileMoisture`, `TileSoilSalinity`, `TileSandBurial`, `TileGrowthPressure`, `TileHeat`, `TileWind`, `TileDust`, `TilePlantWeatherContribution`, `TileDeviceWeatherContribution`.
   - Keep a separate tiny pass only for logic that truly must touch every tile.
   - Expected value:
     avoids paying occupied-tile math on empty tiles and aligns with ECS iteration style.

2. Split the system into two passes by ownership/use case.
   - Pass A:
     occupied ecology entities only for growth/density/growth-pressure logic.
   - Pass B:
     full-grid terrain meter pass only for moisture/fertility/salinity if those must remain tile-wide.
   - Expected value:
     makes the hot path sparse even if some terrain logic stays dense.

3. Stop emitting one `TileEcologyChanged` message per changed tile when only density bucket changes matter.
   - Add message gating/coalescing inside the system tick.
   - Only emit when values cross a visible/report threshold or a subscribed gameplay threshold.
   - Expected value:
     lowers downstream task-board and projection churn.

4. Cache `find_plant_def` resolution by plant id within the tick.
   - A small stack/local table keyed by `PlantId` is enough.
   - Expected value:
     minor, but easy.

### Phase 2: Better ECS Shape

5. Introduce a sparse "active ecology" archetype/tag.
   - Examples:
     `ActiveEcologyTag`, `LivingPlantTag`, `GroundCoverTag`, `DirtyEcologyTag`.
   - Occupied/active tiles enter this archetype when planted/covered and leave when cleared.
   - Query this archetype directly in `EcologySystem`.
   - Expected value:
     turns the main pass from dense scan into sparse ECS iteration.

6. Track dirty ecology tiles explicitly.
   - Mark dirty when:
     occupancy changes, weather contribution changes meaningfully, weather changes materially, or a player action changes ecology state.
   - Only recalc stable terrain/growth values for dirty tiles.
   - Optionally also do a low-frequency background sweep for long-term drift.
   - Expected value:
     biggest practical win if ecology state changes slowly relative to frame rate.

7. Move tile report-threshold bookkeeping onto sparse active data.
   - `last_reported_tile_density` is currently indexed by all tiles.
   - Store threshold/report state alongside active ecology entities instead.
   - Expected value:
     helps support sparse processing cleanly.

### Phase 3: Redesign Options

8. Decouple terrain drift from per-frame simulation.
   - Moisture/fertility/salinity do not need 60 Hz in most gameplay cases.
   - Run them at a lower fixed cadence, for example every `0.25s`, while density/growth-pressure can stay faster if needed.
   - Expected value:
     major reduction with minimal gameplay impact if tuned well.

9. Separate ground cover from living plant simulation.
   - Ground cover often needs simpler rules than growable plants.
   - Give ground cover a cheaper path:
     fewer meters, no multi-tile wind averaging, simplified density progression, lower message frequency.
   - Expected value:
     strong benefit in dense-cover scenarios because those are dominated by non-plant occupancy.

10. Batch ecology output into a frame summary message.
   - Replace many `TileEcologyChanged` messages with:
     one batched message containing changed coords and masks, or one summary per subsystem need.
   - Update `TaskBoardSystem` and projection consumers to process batches.
   - Expected value:
     big reduction in queue traffic and downstream per-message overhead.

11. Convert footprint wind sampling to anchor-owned entities for multi-tile plants.
   - Today each footprint tile can resample neighbors.
   - Instead, keep one anchor-owned ecology record for plant-level derived data like averaged wind exposure.
   - Tile visuals can mirror anchor-owned results.
   - Expected value:
     avoids duplicate work across footprint tiles.

12. Consider SoA-style ecology storage for the hottest fields.
   - If Flecs access remains too expensive in `Debug`, keep ECS ownership/public contracts but mirror hot ecology fields in tightly packed arrays owned by the ecology system.
   - Sync back only on change or at message/projection boundaries.
   - Expected value:
     highest raw speed, but highest complexity and most invasive redesign.

## Recommended Order

1. Query occupied ecology tiles instead of dense tile iteration for the growth path.
2. Split occupied growth logic from full-grid terrain drift logic.
3. Add message coalescing for `TileEcologyChanged`.
4. Add dirty-tile processing.
5. Lower update frequency for slow terrain drift.
6. If still too slow, redesign around sparse active ecology entities and batched messages.

## What I Would Do First

If the goal is the best performance per engineering hour, I would do this sequence:

1. Refactor `EcologySystem` into:
   - sparse occupied-entity query for plant/cover growth work
   - separate low-frequency terrain-meter pass
2. Batch or gate `TileEcologyChanged`
3. Add `DirtyEcologyTag`

That combination should preserve ECS style, keep ownership clean, and deliver much more performance than micro-optimizing the current dense full-grid tick.

## Risks

- Batched message redesign affects downstream consumers such as task-board and projection systems.
- Dirty-tile approaches need careful invalidation when weather contributions or modifiers change.
- Lower-frequency terrain simulation may subtly affect tuning and tests.
- Anchor-owned multi-tile plant redesign affects several message and placement assumptions.

## Success Criteria

- `Debug` dense-cover benchmark:
  cut `EcologySystem` cost meaningfully below current multi-millisecond range.
- `Release` benchmark:
  keep `EcologySystem` comfortably sub-millisecond in dense-cover.
- No system-test regressions in ecology/task-board/projection behavior.
