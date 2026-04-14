#pragma once

#include "site/tile_grid_state.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace gs1
{
class SiteWorld final
{
public:
    struct CreateDesc final
    {
        SiteId site_id {};
        SiteRunId site_run_id {};
        std::uint32_t site_archetype_id {0};
        TileCoord camp_anchor_tile {};
        float camp_durability {100.0f};
        bool camp_protection_resolved {true};
        bool delivery_point_operational {true};
        bool shared_camp_storage_access_enabled {true};
        TileCoord worker_tile_coord {};
        float worker_tile_x {0.0f};
        float worker_tile_y {0.0f};
        float worker_facing_degrees {0.0f};
        float worker_health {100.0f};
        float worker_hydration {100.0f};
        float worker_nourishment {100.0f};
        float worker_energy_cap {100.0f};
        float worker_energy {100.0f};
        float worker_morale {100.0f};
        float worker_work_efficiency {1.0f};
        bool worker_is_sheltered {false};
        std::uint32_t worker_current_action_id {0};
        bool worker_has_current_action_id {false};
    };

    SiteWorld();
    ~SiteWorld();

    SiteWorld(const SiteWorld&) = delete;
    SiteWorld& operator=(const SiteWorld&) = delete;
    SiteWorld(SiteWorld&&) noexcept;
    SiteWorld& operator=(SiteWorld&&) noexcept;

    void initialize(const CreateDesc& desc, const TileGridState& tile_grid);

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;
    [[nodiscard]] std::size_t tile_count() const noexcept;
    [[nodiscard]] bool contains(TileCoord coord) const noexcept;
    [[nodiscard]] std::uint32_t tile_index(TileCoord coord) const noexcept;
    [[nodiscard]] std::uint64_t tile_entity_id(TileCoord coord) const noexcept;
    [[nodiscard]] std::uint64_t worker_entity_id() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_ {};
};
}  // namespace gs1
