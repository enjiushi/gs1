#pragma once

#include "content/defs/faction_defs.h"
#include "site/economy_state.h"
#include "support/id_types.h"
#include "support/site_objective_types.h"
#include "gs1/types.h"

#include <cstdint>
#include <vector>

namespace gs1
{
struct PrototypeSupportItemContent final
{
    ItemId item_id {};
    std::uint32_t quantity {0};
};

struct PrototypePhoneListingContent final
{
    std::uint32_t listing_id {0};
    PhoneListingKind kind {PhoneListingKind::BuyItem};
    std::uint32_t item_or_unlockable_id {0};
    std::int32_t price {0};
    std::uint32_t internal_price_cash_points {0U};
    std::uint32_t quantity {0};
};

struct PrototypePhoneBuyStockContent final
{
    std::uint32_t listing_id {0};
    ItemId item_id {};
    std::uint32_t base_stock_cash_points {0U};
    std::uint32_t stock_cash_points_variance {0U};
};

struct PrototypeSiteStartingPlantContent final
{
    PlantId plant_id {};
    TileCoord anchor_tile {};
    float initial_density {0.0f};
};

struct PrototypeSiteContent final
{
    SiteId site_id {};
    std::uint32_t site_archetype_id {0};
    SiteObjectiveType objective_type {SiteObjectiveType::DenseRestoration};
    FactionId featured_faction_id {};
    Gs1SiteState initial_state {GS1_SITE_STATE_LOCKED};
    TileCoord regional_map_tile {};
    std::uint32_t support_package_id {0};
    std::vector<PrototypeSupportItemContent> exported_support_items {};
    std::vector<ModifierId> nearby_aura_modifier_ids {};
    std::uint32_t site_completion_tile_threshold {0};
    double site_time_limit_minutes {0.0};
    SiteObjectiveTargetEdge objective_target_edge {SiteObjectiveTargetEdge::East};
    std::uint8_t objective_target_band_width {0U};
    std::int32_t objective_target_cash_points {0};
    float highway_max_average_sand_cover {0.0f};
    std::int32_t completion_reputation_reward {0};
    std::int32_t completion_faction_reputation_reward {0};
    TileCoord camp_anchor_tile {};
    float default_weather_heat {0.0f};
    float default_weather_wind {0.0f};
    float default_weather_dust {0.0f};
    float default_weather_wind_direction_degrees {0.0f};
    std::vector<PrototypeSiteStartingPlantContent> starting_plants {};
    std::int32_t phone_delivery_fee {0};
    std::uint16_t phone_delivery_minutes {0};
    std::vector<std::uint32_t> initial_revealed_unlockable_ids {};
    std::vector<std::uint32_t> initial_direct_purchase_unlockable_ids {};
    std::vector<PrototypePhoneListingContent> seeded_phone_listings {};
    std::vector<PrototypePhoneBuyStockContent> phone_buy_stock_pool {};
};

struct PrototypeCampaignContent final
{
    std::int32_t starting_site_cash {0};
    std::uint32_t support_quota_per_contributor {0};
    std::vector<PrototypeSupportItemContent> baseline_deployment_items {};
    std::vector<PrototypeSiteContent> sites {};
    std::vector<SiteId> initially_revealed_site_ids {};
    std::vector<SiteId> initially_available_site_ids {};
};

[[nodiscard]] const PrototypeCampaignContent& get_prototype_campaign_content() noexcept;
[[nodiscard]] const PrototypeSiteContent* find_prototype_site_content(SiteId site_id) noexcept;
}  // namespace gs1
