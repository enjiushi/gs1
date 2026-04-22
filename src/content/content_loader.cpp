#include "content/content_loader.h"

#include "content/content_validator.h"

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace gs1
{
namespace
{
[[nodiscard]] std::filesystem::path content_table_root()
{
    return std::filesystem::path {GS1_CONTENT_TABLE_DIR};
}

[[noreturn]] void fail_load(
    const std::filesystem::path& path,
    std::size_t line_number,
    std::string_view message)
{
    if (line_number == 0U)
    {
        std::fprintf(
            stderr,
            "Failed to load content table %s: %.*s\n",
            path.string().c_str(),
            static_cast<int>(message.size()),
            message.data());
    }
    else
    {
        std::fprintf(
            stderr,
            "Failed to load content table %s:%zu: %.*s\n",
            path.string().c_str(),
            line_number,
            static_cast<int>(message.size()),
            message.data());
    }

    std::abort();
}

[[nodiscard]] std::string trim_copy(std::string_view text)
{
    std::size_t begin = 0U;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r'))
    {
        ++begin;
    }
    while (end > begin &&
        (text[end - 1U] == ' ' || text[end - 1U] == '\t' || text[end - 1U] == '\r'))
    {
        --end;
    }

    return std::string {text.substr(begin, end - begin)};
}

[[nodiscard]] bool is_empty_or_comment(std::string_view line)
{
    const auto trimmed = trim_copy(line);
    return trimmed.empty() || trimmed[0] == '#';
}

[[nodiscard]] std::vector<std::string> split_delimited(std::string_view line, char delimiter)
{
    std::vector<std::string> fields {};
    std::size_t start = 0U;
    while (start <= line.size())
    {
        const auto next = line.find(delimiter, start);
        if (next == std::string_view::npos)
        {
            fields.push_back(trim_copy(line.substr(start)));
            break;
        }

        fields.push_back(trim_copy(line.substr(start, next - start)));
        start = next + 1U;
    }

    return fields;
}

[[nodiscard]] std::string_view store_string_view(ContentDatabase& content, std::string value)
{
    content.owned_strings.push_back(std::move(value));
    return content.owned_strings.back();
}

[[nodiscard]] const char* store_cstring(ContentDatabase& content, std::string value)
{
    content.owned_strings.push_back(std::move(value));
    return content.owned_strings.back().c_str();
}

template <typename T>
[[nodiscard]] T parse_unsigned(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    T value {};
    const auto* begin = field.data();
    const auto* end = field.data() + field.size();
    const auto [ptr, error] = std::from_chars(begin, end, value);
    if (error != std::errc {} || ptr != end)
    {
        fail_load(path, line_number, "invalid unsigned integer field");
    }
    return value;
}

template <typename T>
[[nodiscard]] T parse_signed(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    T value {};
    const auto* begin = field.data();
    const auto* end = field.data() + field.size();
    const auto [ptr, error] = std::from_chars(begin, end, value);
    if (error != std::errc {} || ptr != end)
    {
        fail_load(path, line_number, "invalid signed integer field");
    }
    return value;
}

[[nodiscard]] float parse_float(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    char* parse_end = nullptr;
    const float value = std::strtof(field.c_str(), &parse_end);
    if (parse_end == field.c_str() || parse_end == nullptr || *parse_end != '\0')
    {
        fail_load(path, line_number, "invalid float field");
    }
    return value;
}

[[nodiscard]] bool parse_bool(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "1" || field == "true" || field == "True")
    {
        return true;
    }
    if (field == "0" || field == "false" || field == "False")
    {
        return false;
    }

    fail_load(path, line_number, "invalid boolean field");
}

[[nodiscard]] ItemSourceRule parse_item_source_rule(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "None")
    {
        return ItemSourceRule::None;
    }
    if (field == "BuyOnly")
    {
        return ItemSourceRule::BuyOnly;
    }
    if (field == "CraftOnly")
    {
        return ItemSourceRule::CraftOnly;
    }
    if (field == "BuyOrCraft")
    {
        return ItemSourceRule::BuyOrCraft;
    }
    if (field == "HarvestOnly")
    {
        return ItemSourceRule::HarvestOnly;
    }

    fail_load(path, line_number, "invalid item source rule");
}

[[nodiscard]] std::uint32_t parse_item_capabilities(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field.empty() || field == "None")
    {
        return ITEM_CAPABILITY_NONE;
    }

    std::uint32_t flags = ITEM_CAPABILITY_NONE;
    for (const auto& token : split_delimited(field, ','))
    {
        if (token == "Drink")
        {
            flags |= ITEM_CAPABILITY_DRINK;
        }
        else if (token == "Eat")
        {
            flags |= ITEM_CAPABILITY_EAT;
        }
        else if (token == "UseMedicine")
        {
            flags |= ITEM_CAPABILITY_USE_MEDICINE;
        }
        else if (token == "Plant")
        {
            flags |= ITEM_CAPABILITY_PLANT;
        }
        else if (token == "Sell")
        {
            flags |= ITEM_CAPABILITY_SELL;
        }
        else if (token == "Deploy")
        {
            flags |= ITEM_CAPABILITY_DEPLOY;
        }
        else
        {
            fail_load(path, line_number, "invalid item capability token");
        }
    }

    return flags;
}

[[nodiscard]] PlantHeightClass parse_plant_height_class(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "None")
    {
        return PlantHeightClass::None;
    }
    if (field == "Low")
    {
        return PlantHeightClass::Low;
    }
    if (field == "Medium")
    {
        return PlantHeightClass::Medium;
    }
    if (field == "Tall")
    {
        return PlantHeightClass::Tall;
    }

    fail_load(path, line_number, "invalid plant height class");
}

[[nodiscard]] CraftingStationKind parse_crafting_station_kind(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "None")
    {
        return CraftingStationKind::None;
    }
    if (field == "Cooking")
    {
        return CraftingStationKind::Cooking;
    }
    if (field == "Fabrication")
    {
        return CraftingStationKind::Fabrication;
    }

    fail_load(path, line_number, "invalid crafting station kind");
}

[[nodiscard]] SiteObjectiveType parse_site_objective_type(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "DenseRestoration")
    {
        return SiteObjectiveType::DenseRestoration;
    }
    if (field == "HighwayProtection")
    {
        return SiteObjectiveType::HighwayProtection;
    }
    if (field == "GreenWallConnection")
    {
        return SiteObjectiveType::GreenWallConnection;
    }
    if (field == "PureSurvival")
    {
        return SiteObjectiveType::PureSurvival;
    }

    fail_load(path, line_number, "invalid site objective type");
}

[[nodiscard]] Gs1SiteState parse_site_state(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "Locked")
    {
        return GS1_SITE_STATE_LOCKED;
    }
    if (field == "Available")
    {
        return GS1_SITE_STATE_AVAILABLE;
    }
    if (field == "Completed")
    {
        return GS1_SITE_STATE_COMPLETED;
    }

    fail_load(path, line_number, "invalid site state");
}

[[nodiscard]] SiteObjectiveTargetEdge parse_site_objective_target_edge(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "North")
    {
        return SiteObjectiveTargetEdge::North;
    }
    if (field == "East")
    {
        return SiteObjectiveTargetEdge::East;
    }
    if (field == "South")
    {
        return SiteObjectiveTargetEdge::South;
    }
    if (field == "West")
    {
        return SiteObjectiveTargetEdge::West;
    }

    fail_load(path, line_number, "invalid site objective target edge");
}

[[nodiscard]] TaskProgressKind parse_task_progress_kind(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "None")
    {
        return TaskProgressKind::None;
    }
    if (field == "RestorationTiles")
    {
        return TaskProgressKind::RestorationTiles;
    }
    if (field == "BuyItem")
    {
        return TaskProgressKind::BuyItem;
    }
    if (field == "SellItem")
    {
        return TaskProgressKind::SellItem;
    }
    if (field == "TransferItem")
    {
        return TaskProgressKind::TransferItem;
    }
    if (field == "PlantItem")
    {
        return TaskProgressKind::PlantItem;
    }
    if (field == "CraftRecipe")
    {
        return TaskProgressKind::CraftRecipe;
    }
    if (field == "ConsumeItem")
    {
        return TaskProgressKind::ConsumeItem;
    }

    fail_load(path, line_number, "invalid task progress kind");
}

[[nodiscard]] RewardEffectKind parse_reward_effect_kind(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "None")
    {
        return RewardEffectKind::None;
    }
    if (field == "Money")
    {
        return RewardEffectKind::Money;
    }
    if (field == "ItemDelivery")
    {
        return RewardEffectKind::ItemDelivery;
    }
    if (field == "RevealUnlockable")
    {
        return RewardEffectKind::RevealUnlockable;
    }
    if (field == "RunModifier")
    {
        return RewardEffectKind::RunModifier;
    }
    if (field == "CampaignReputation")
    {
        return RewardEffectKind::CampaignReputation;
    }
    if (field == "FactionReputation")
    {
        return RewardEffectKind::FactionReputation;
    }

    fail_load(path, line_number, "invalid reward effect kind");
}

[[nodiscard]] PhoneListingKind parse_phone_listing_kind(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "BuyItem")
    {
        return PhoneListingKind::BuyItem;
    }
    if (field == "SellItem")
    {
        return PhoneListingKind::SellItem;
    }
    if (field == "HireContractor")
    {
        return PhoneListingKind::HireContractor;
    }
    if (field == "PurchaseUnlockable")
    {
        return PhoneListingKind::PurchaseUnlockable;
    }

    fail_load(path, line_number, "invalid phone listing kind");
}

[[nodiscard]] ActionKind parse_action_kind(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "Plant")
    {
        return ActionKind::Plant;
    }
    if (field == "Build")
    {
        return ActionKind::Build;
    }
    if (field == "Repair")
    {
        return ActionKind::Repair;
    }
    if (field == "Water")
    {
        return ActionKind::Water;
    }
    if (field == "ClearBurial")
    {
        return ActionKind::ClearBurial;
    }
    if (field == "Craft")
    {
        return ActionKind::Craft;
    }
    if (field == "Drink")
    {
        return ActionKind::Drink;
    }
    if (field == "Eat")
    {
        return ActionKind::Eat;
    }

    fail_load(path, line_number, "invalid site action kind");
}

[[nodiscard]] PlacementOccupancyLayer parse_placement_occupancy_layer(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "None")
    {
        return PlacementOccupancyLayer::None;
    }
    if (field == "GroundCover")
    {
        return PlacementOccupancyLayer::GroundCover;
    }
    if (field == "Structure")
    {
        return PlacementOccupancyLayer::Structure;
    }

    fail_load(path, line_number, "invalid placement occupancy layer");
}

[[nodiscard]] ModifierPresetKind parse_modifier_preset_kind(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "NearbyAura")
    {
        return ModifierPresetKind::NearbyAura;
    }
    if (field == "RunModifier")
    {
        return ModifierPresetKind::RunModifier;
    }

    fail_load(path, line_number, "invalid modifier preset kind");
}

[[nodiscard]] TechnologyEntryKind parse_technology_entry_kind(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "GlobalModifier")
    {
        return TechnologyEntryKind::GlobalModifier;
    }
    if (field == "MechanismChange")
    {
        return TechnologyEntryKind::MechanismChange;
    }

    fail_load(path, line_number, "invalid technology entry kind");
}

[[nodiscard]] ReputationUnlockKind parse_reputation_unlock_kind(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "Plant")
    {
        return ReputationUnlockKind::Plant;
    }
    if (field == "Device")
    {
        return ReputationUnlockKind::Device;
    }
    if (field == "Recipe")
    {
        return ReputationUnlockKind::Recipe;
    }

    fail_load(path, line_number, "invalid reputation unlock kind");
}

[[nodiscard]] std::vector<SiteId> parse_site_id_list(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    std::vector<SiteId> values {};
    if (field.empty())
    {
        return values;
    }

    for (const auto& token : split_delimited(field, ','))
    {
        values.push_back(SiteId {parse_unsigned<std::uint32_t>(path, line_number, token)});
    }

    return values;
}

[[nodiscard]] std::vector<ModifierId> parse_modifier_id_list(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    std::vector<ModifierId> values {};
    if (field.empty())
    {
        return values;
    }

    for (const auto& token : split_delimited(field, ','))
    {
        values.push_back(ModifierId {parse_unsigned<std::uint32_t>(path, line_number, token)});
    }

    return values;
}

[[nodiscard]] std::vector<std::uint32_t> parse_u32_list(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    std::vector<std::uint32_t> values {};
    if (field.empty())
    {
        return values;
    }

    for (const auto& token : split_delimited(field, ','))
    {
        values.push_back(parse_unsigned<std::uint32_t>(path, line_number, token));
    }

    return values;
}

[[nodiscard]] std::vector<PrototypeSupportItemContent> parse_support_item_list(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    std::vector<PrototypeSupportItemContent> values {};
    if (field.empty())
    {
        return values;
    }

    for (const auto& token : split_delimited(field, ';'))
    {
        const auto parts = split_delimited(token, ':');
        if (parts.size() != 2U)
        {
            fail_load(path, line_number, "invalid support item field");
        }

        values.push_back(PrototypeSupportItemContent {
            ItemId {parse_unsigned<std::uint32_t>(path, line_number, parts[0])},
            parse_unsigned<std::uint32_t>(path, line_number, parts[1])});
    }

    return values;
}

PrototypeSiteContent* find_loaded_site(
    PrototypeCampaignContent& campaign,
    SiteId site_id) noexcept
{
    for (auto& site : campaign.sites)
    {
        if (site.site_id == site_id)
        {
            return &site;
        }
    }

    return nullptr;
}

void load_prototype_campaign_sites(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 24U)
        {
            fail_load(path, line_number, "unexpected site column count");
        }

        PrototypeSiteContent site {};
        site.site_id = SiteId {parse_unsigned<std::uint32_t>(path, line_number, fields[0])};
        site.site_archetype_id = parse_unsigned<std::uint32_t>(path, line_number, fields[1]);
        site.objective_type = parse_site_objective_type(path, line_number, fields[2]);
        site.featured_faction_id = FactionId {parse_unsigned<std::uint32_t>(path, line_number, fields[3])};
        site.initial_state = parse_site_state(path, line_number, fields[4]);
        site.adjacent_site_ids = parse_site_id_list(path, line_number, fields[5]);
        site.support_package_id = parse_unsigned<std::uint32_t>(path, line_number, fields[6]);
        site.exported_support_items = parse_support_item_list(path, line_number, fields[7]);
        site.nearby_aura_modifier_ids = parse_modifier_id_list(path, line_number, fields[8]);
        site.site_completion_tile_threshold = parse_unsigned<std::uint32_t>(path, line_number, fields[9]);
        site.site_time_limit_minutes = static_cast<double>(parse_float(path, line_number, fields[10]));
        site.objective_target_edge = parse_site_objective_target_edge(path, line_number, fields[11]);
        site.objective_target_band_width = parse_unsigned<std::uint8_t>(path, line_number, fields[12]);
        site.highway_max_average_sand_cover = parse_float(path, line_number, fields[13]);
        site.completion_reputation_reward = parse_signed<std::int32_t>(path, line_number, fields[14]);
        site.completion_faction_reputation_reward = parse_signed<std::int32_t>(path, line_number, fields[15]);
        site.camp_anchor_tile.x = parse_signed<std::int32_t>(path, line_number, fields[16]);
        site.camp_anchor_tile.y = parse_signed<std::int32_t>(path, line_number, fields[17]);
        site.phone_delivery_fee = parse_signed<std::int32_t>(path, line_number, fields[18]);
        site.phone_delivery_minutes = parse_unsigned<std::uint16_t>(path, line_number, fields[19]);
        site.initial_revealed_unlockable_ids = parse_u32_list(path, line_number, fields[20]);
        site.initial_direct_purchase_unlockable_ids = parse_u32_list(path, line_number, fields[21]);

        content.prototype_campaign.sites.push_back(std::move(site));

        const auto site_id = content.prototype_campaign.sites.back().site_id;
        if (parse_bool(path, line_number, fields[22]))
        {
            content.prototype_campaign.initially_revealed_site_ids.push_back(site_id);
        }
        if (parse_bool(path, line_number, fields[23]))
        {
            content.prototype_campaign.initially_available_site_ids.push_back(site_id);
        }
    }
}

void load_prototype_campaign_setup(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    bool found_row = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 3U)
        {
            fail_load(path, line_number, "unexpected campaign setup column count");
        }
        if (found_row)
        {
            fail_load(path, line_number, "campaign setup table must contain exactly one data row");
        }

        content.prototype_campaign.starting_campaign_cash =
            parse_signed<std::int32_t>(path, line_number, fields[0]);
        content.prototype_campaign.support_quota_per_contributor =
            parse_unsigned<std::uint32_t>(path, line_number, fields[1]);
        content.prototype_campaign.baseline_deployment_items =
            parse_support_item_list(path, line_number, fields[2]);
        found_row = true;
    }

    if (!found_row)
    {
        fail_load(path, 0U, "campaign setup table must contain one data row");
    }
}

void load_prototype_site_phone_listings(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 6U)
        {
            fail_load(path, line_number, "unexpected phone listing column count");
        }

        const SiteId site_id {parse_unsigned<std::uint32_t>(path, line_number, fields[0])};
        auto* site = find_loaded_site(content.prototype_campaign, site_id);
        if (site == nullptr)
        {
            fail_load(path, line_number, "phone listing references an unknown site id");
        }

        site->seeded_phone_listings.push_back(PrototypePhoneListingContent {
            parse_unsigned<std::uint32_t>(path, line_number, fields[1]),
            parse_phone_listing_kind(path, line_number, fields[2]),
            parse_unsigned<std::uint32_t>(path, line_number, fields[3]),
            parse_signed<std::int32_t>(path, line_number, fields[4]),
            parse_unsigned<std::uint32_t>(path, line_number, fields[5])});
    }
}

void load_item_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 14U)
        {
            fail_load(path, line_number, "unexpected item column count");
        }

        content.item_defs.push_back(ItemDef {
            ItemId {parse_unsigned<std::uint32_t>(path, line_number, fields[0])},
            store_string_view(content, fields[1]),
            parse_unsigned<std::uint16_t>(path, line_number, fields[2]),
            parse_item_source_rule(path, line_number, fields[3]),
            parse_bool(path, line_number, fields[4]),
            parse_signed<std::int32_t>(path, line_number, fields[5]),
            parse_signed<std::int32_t>(path, line_number, fields[6]),
            parse_item_capabilities(path, line_number, fields[7]),
            PlantId {parse_unsigned<std::uint32_t>(path, line_number, fields[8])},
            StructureId {parse_unsigned<std::uint32_t>(path, line_number, fields[9])},
            parse_float(path, line_number, fields[10]),
            parse_float(path, line_number, fields[11]),
            parse_float(path, line_number, fields[12]),
            parse_float(path, line_number, fields[13])});
    }
}

void load_task_template_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 8U)
        {
            fail_load(path, line_number, "unexpected task template column count");
        }

        content.task_template_defs.push_back(TaskTemplateDef {
            TaskTemplateId {parse_unsigned<std::uint32_t>(path, line_number, fields[0])},
            FactionId {parse_unsigned<std::uint32_t>(path, line_number, fields[1])},
            parse_unsigned<std::uint32_t>(path, line_number, fields[2]),
            parse_task_progress_kind(path, line_number, fields[3]),
            parse_unsigned<std::uint32_t>(path, line_number, fields[4]),
            ItemId {parse_unsigned<std::uint32_t>(path, line_number, fields[5])},
            RecipeId {parse_unsigned<std::uint32_t>(path, line_number, fields[6])},
            parse_signed<std::int32_t>(path, line_number, fields[7])});
    }
}

void load_reward_candidate_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 10U)
        {
            fail_load(path, line_number, "unexpected reward candidate column count");
        }

        content.reward_candidate_defs.push_back(RewardCandidateDef {
            RewardCandidateId {parse_unsigned<std::uint32_t>(path, line_number, fields[0])},
            parse_reward_effect_kind(path, line_number, fields[1]),
            parse_signed<std::int32_t>(path, line_number, fields[2]),
            ItemId {parse_unsigned<std::uint32_t>(path, line_number, fields[3])},
            parse_unsigned<std::uint32_t>(path, line_number, fields[4]),
            parse_unsigned<std::uint32_t>(path, line_number, fields[5]),
            ModifierId {parse_unsigned<std::uint32_t>(path, line_number, fields[6])},
            FactionId {parse_unsigned<std::uint32_t>(path, line_number, fields[7])},
            parse_unsigned<std::uint16_t>(path, line_number, fields[8]),
            parse_signed<std::int32_t>(path, line_number, fields[9])});
    }
}

void load_site_action_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 9U)
        {
            fail_load(path, line_number, "unexpected site action column count");
        }

        content.site_action_defs.push_back(SiteActionDef {
            parse_action_kind(path, line_number, fields[0]),
            parse_float(path, line_number, fields[1]),
            parse_float(path, line_number, fields[2]),
            parse_float(path, line_number, fields[3]),
            parse_float(path, line_number, fields[4]),
            parse_placement_occupancy_layer(path, line_number, fields[5]),
            parse_bool(path, line_number, fields[6]),
            parse_bool(path, line_number, fields[7]),
            parse_bool(path, line_number, fields[8])});
    }
}

void load_modifier_preset_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 18U)
        {
            fail_load(path, line_number, "unexpected modifier preset column count");
        }

        ModifierPresetDef preset {};
        preset.preset_kind = parse_modifier_preset_kind(path, line_number, fields[0]);
        preset.preset_index = parse_unsigned<std::uint32_t>(path, line_number, fields[1]);
        preset.totals.heat = parse_float(path, line_number, fields[2]);
        preset.totals.wind = parse_float(path, line_number, fields[3]);
        preset.totals.dust = parse_float(path, line_number, fields[4]);
        preset.totals.moisture = parse_float(path, line_number, fields[5]);
        preset.totals.fertility = parse_float(path, line_number, fields[6]);
        preset.totals.salinity = parse_float(path, line_number, fields[7]);
        preset.totals.growth_pressure = parse_float(path, line_number, fields[8]);
        preset.totals.salinity_density_cap = parse_float(path, line_number, fields[9]);
        preset.totals.plant_density = parse_float(path, line_number, fields[10]);
        preset.totals.health = parse_float(path, line_number, fields[11]);
        preset.totals.hydration = parse_float(path, line_number, fields[12]);
        preset.totals.nourishment = parse_float(path, line_number, fields[13]);
        preset.totals.energy_cap = parse_float(path, line_number, fields[14]);
        preset.totals.energy = parse_float(path, line_number, fields[15]);
        preset.totals.morale = parse_float(path, line_number, fields[16]);
        preset.totals.work_efficiency = parse_float(path, line_number, fields[17]);

        if (preset.preset_kind == ModifierPresetKind::NearbyAura)
        {
            content.nearby_aura_modifier_presets.push_back(preset);
        }
        else
        {
            content.run_modifier_presets.push_back(preset);
        }
    }
}

void load_technology_tier_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 4U)
        {
            fail_load(path, line_number, "unexpected technology tier column count");
        }

        content.technology_tier_defs.push_back(TechnologyTierDef {
            FactionId {parse_unsigned<std::uint32_t>(path, line_number, fields[0])},
            parse_unsigned<std::uint8_t>(path, line_number, fields[1]),
            {0U, 0U, 0U},
            parse_signed<std::int32_t>(path, line_number, fields[2]),
            store_string_view(content, fields[3])});
    }
}

void load_total_reputation_tier_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 3U)
        {
            fail_load(path, line_number, "unexpected total reputation tier column count");
        }

        content.total_reputation_tier_defs.push_back(TotalReputationTierDef {
            parse_unsigned<std::uint8_t>(path, line_number, fields[0]),
            {0U, 0U, 0U},
            parse_signed<std::int32_t>(path, line_number, fields[1]),
            store_string_view(content, fields[2])});
    }
}

void load_reputation_unlock_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 6U)
        {
            fail_load(path, line_number, "unexpected reputation unlock column count");
        }

        const auto tier_index = parse_unsigned<std::uint8_t>(path, line_number, fields[0]);
        const auto slot_index = parse_unsigned<std::uint8_t>(path, line_number, fields[1]);
        content.reputation_unlock_defs.push_back(ReputationUnlockDef {
            reputation_unlock_id(tier_index, slot_index),
            parse_reputation_unlock_kind(path, line_number, fields[2]),
            tier_index,
            {0U, 0U},
            parse_unsigned<std::uint32_t>(path, line_number, fields[3]),
            store_string_view(content, fields[4]),
            store_string_view(content, fields[5])});
    }
}

void load_technology_node_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 8U)
        {
            fail_load(path, line_number, "unexpected technology node column count");
        }

        const auto faction_id = FactionId {parse_unsigned<std::uint32_t>(path, line_number, fields[0])};
        const auto tier_index = parse_unsigned<std::uint8_t>(path, line_number, fields[1]);
        const auto slot_index = parse_unsigned<std::uint8_t>(path, line_number, fields[2]);
        const auto entry_kind = parse_technology_entry_kind(path, line_number, fields[3]);
        content.technology_node_defs.push_back(TechnologyNodeDef {
            TechNodeId {technology_node_id(faction_id, tier_index, slot_index)},
            faction_id,
            tier_index,
            slot_index,
            entry_kind,
            {0U, 0U, 0U},
            parse_signed<std::int32_t>(path, line_number, fields[4]),
            entry_kind == TechnologyEntryKind::GlobalModifier
                ? ModifierId {technology_modifier_id(faction_id, tier_index, slot_index)}
                : ModifierId {},
            entry_kind == TechnologyEntryKind::MechanismChange
                ? technology_mechanism_change_id(faction_id, tier_index, slot_index)
                : 0U,
            parse_bool(path, line_number, fields[5]),
            {0U, 0U, 0U},
            store_string_view(content, fields[6]),
            store_string_view(content, fields[7])});
    }
}

void load_initial_unlocked_plant_ids(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 1U)
        {
            fail_load(path, line_number, "unexpected initial unlocked plants column count");
        }

        content.initial_unlocked_plant_ids.push_back(
            PlantId {parse_unsigned<std::uint32_t>(path, line_number, fields[0])});
    }
}

void load_plant_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 22U)
        {
            fail_load(path, line_number, "unexpected plant column count");
        }

        content.plant_defs.push_back(PlantDef {
            PlantId {parse_unsigned<std::uint32_t>(path, line_number, fields[0])},
            store_cstring(content, fields[1]),
            parse_plant_height_class(path, line_number, fields[2]),
            parse_bool(path, line_number, fields[3]),
            parse_unsigned<std::uint8_t>(path, line_number, fields[4]),
            parse_unsigned<std::uint8_t>(path, line_number, fields[5]),
            parse_unsigned<std::uint8_t>(path, line_number, fields[6]),
            parse_unsigned<std::uint8_t>(path, line_number, fields[7]),
            parse_float(path, line_number, fields[8]),
            parse_float(path, line_number, fields[9]),
            parse_float(path, line_number, fields[10]),
            parse_float(path, line_number, fields[11]),
            parse_float(path, line_number, fields[12]),
            parse_float(path, line_number, fields[13]),
            parse_float(path, line_number, fields[14]),
            parse_float(path, line_number, fields[15]),
            parse_float(path, line_number, fields[16]),
            parse_float(path, line_number, fields[17]),
            parse_float(path, line_number, fields[18]),
            parse_float(path, line_number, fields[19]),
            parse_float(path, line_number, fields[20]),
            parse_float(path, line_number, fields[21])});
    }
}

void load_structure_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 10U)
        {
            fail_load(path, line_number, "unexpected structure column count");
        }

        content.structure_defs.push_back(StructureDef {
            StructureId {parse_unsigned<std::uint32_t>(path, line_number, fields[0])},
            store_string_view(content, fields[1]),
            parse_float(path, line_number, fields[2]),
            parse_float(path, line_number, fields[3]),
            parse_unsigned<std::uint8_t>(path, line_number, fields[4]),
            parse_unsigned<std::uint16_t>(path, line_number, fields[5]),
            parse_crafting_station_kind(path, line_number, fields[6]),
            parse_bool(path, line_number, fields[7]),
            parse_unsigned<std::uint8_t>(path, line_number, fields[8]),
            parse_unsigned<std::uint8_t>(path, line_number, fields[9])});
    }
}

void load_craft_recipe_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    std::ifstream stream {path};
    if (!stream.is_open())
    {
        fail_load(path, 0U, "unable to open file");
    }

    std::string line {};
    std::size_t line_number = 0U;
    bool skipped_header = false;
    while (std::getline(stream, line))
    {
        ++line_number;
        if (is_empty_or_comment(line))
        {
            continue;
        }
        if (!skipped_header)
        {
            skipped_header = true;
            continue;
        }

        const auto fields = split_delimited(line, '\t');
        if (fields.size() != 6U)
        {
            fail_load(path, line_number, "unexpected craft recipe column count");
        }

        CraftRecipeDef recipe_def {};
        recipe_def.recipe_id = RecipeId {parse_unsigned<std::uint32_t>(path, line_number, fields[0])};
        recipe_def.station_structure_id =
            StructureId {parse_unsigned<std::uint32_t>(path, line_number, fields[1])};
        recipe_def.output_item_id = ItemId {parse_unsigned<std::uint32_t>(path, line_number, fields[2])};
        recipe_def.output_quantity = parse_unsigned<std::uint16_t>(path, line_number, fields[3]);
        recipe_def.craft_minutes = parse_float(path, line_number, fields[4]);

        if (!fields[5].empty())
        {
            const auto ingredient_tokens = split_delimited(fields[5], ';');
            if (ingredient_tokens.size() > recipe_def.ingredients.size())
            {
                fail_load(path, line_number, "too many craft recipe ingredients");
            }

            for (std::size_t index = 0; index < ingredient_tokens.size(); ++index)
            {
                const auto parts = split_delimited(ingredient_tokens[index], ':');
                if (parts.size() != 2U)
                {
                    fail_load(path, line_number, "invalid craft recipe ingredient field");
                }

                recipe_def.ingredients[index] = CraftRecipeIngredientDef {
                    ItemId {parse_unsigned<std::uint32_t>(path, line_number, parts[0])},
                    parse_unsigned<std::uint16_t>(path, line_number, parts[1])};
            }

            recipe_def.ingredient_count = static_cast<std::uint8_t>(ingredient_tokens.size());
        }

        content.craft_recipe_defs.push_back(recipe_def);
    }
}

template <typename DefVector, typename MapType, typename IdAccessor>
void index_defs(
    const std::filesystem::path& path,
    std::string_view label,
    const DefVector& defs,
    MapType& index_map,
    IdAccessor id_accessor)
{
    for (std::size_t index = 0U; index < defs.size(); ++index)
    {
        const auto id = id_accessor(defs[index]);
        const auto [_, inserted] = index_map.emplace(id, index);
        if (!inserted)
        {
            std::string message = "duplicate ";
            message += label;
            message += " id";
            fail_load(path, 0U, message);
        }
    }
}
}  // namespace

ContentDatabase ContentLoader::load_prototype_content()
{
    ContentDatabase content {};

    const auto root = content_table_root();
    const auto sites_path = root / "sites.tsv";
    const auto campaign_setup_path = root / "campaign_setup.tsv";
    const auto phone_listings_path = root / "phone_listings.tsv";
    const auto items_path = root / "items.tsv";
    const auto plants_path = root / "plants.tsv";
    const auto structures_path = root / "structures.tsv";
    const auto craft_recipes_path = root / "craft_recipes.tsv";
    const auto task_templates_path = root / "task_templates.tsv";
    const auto reward_candidates_path = root / "reward_candidates.tsv";
    const auto site_actions_path = root / "site_actions.tsv";
    const auto modifier_presets_path = root / "modifier_presets.tsv";
    const auto technology_tiers_path = root / "technology_tiers.tsv";
    const auto total_reputation_tiers_path = root / "total_reputation_tiers.tsv";
    const auto reputation_unlocks_path = root / "reputation_unlocks.tsv";
    const auto technology_nodes_path = root / "technology_nodes.tsv";
    const auto initial_unlocked_plants_path = root / "initial_unlocked_plants.tsv";

    load_prototype_campaign_sites(content, sites_path);
    load_prototype_campaign_setup(content, campaign_setup_path);
    load_prototype_site_phone_listings(content, phone_listings_path);
    load_item_defs(content, items_path);
    load_plant_defs(content, plants_path);
    load_structure_defs(content, structures_path);
    load_craft_recipe_defs(content, craft_recipes_path);
    load_task_template_defs(content, task_templates_path);
    load_reward_candidate_defs(content, reward_candidates_path);
    load_site_action_defs(content, site_actions_path);
    load_modifier_preset_defs(content, modifier_presets_path);
    load_technology_tier_defs(content, technology_tiers_path);
    load_total_reputation_tier_defs(content, total_reputation_tiers_path);
    load_reputation_unlock_defs(content, reputation_unlocks_path);
    load_technology_node_defs(content, technology_nodes_path);
    load_initial_unlocked_plant_ids(content, initial_unlocked_plants_path);

    index_defs(
        sites_path,
        "site",
        content.prototype_campaign.sites,
        content.index.site_by_id,
        [](const PrototypeSiteContent& def) {
            return def.site_id.value;
        });

    index_defs(items_path, "item", content.item_defs, content.index.item_by_id, [](const ItemDef& def) {
        return def.item_id.value;
    });
    index_defs(plants_path, "plant", content.plant_defs, content.index.plant_by_id, [](const PlantDef& def) {
        return def.plant_id.value;
    });
    index_defs(
        structures_path,
        "structure",
        content.structure_defs,
        content.index.structure_by_id,
        [](const StructureDef& def) {
            return def.structure_id.value;
        });
    index_defs(
        craft_recipes_path,
        "craft recipe",
        content.craft_recipe_defs,
        content.index.craft_recipe_by_id,
        [](const CraftRecipeDef& def) {
            return def.recipe_id.value;
        });
    index_defs(
        task_templates_path,
        "task template",
        content.task_template_defs,
        content.index.task_template_by_id,
        [](const TaskTemplateDef& def) {
            return def.task_template_id.value;
        });
    index_defs(
        reward_candidates_path,
        "reward candidate",
        content.reward_candidate_defs,
        content.index.reward_candidate_by_id,
        [](const RewardCandidateDef& def) {
            return def.reward_candidate_id.value;
        });
    index_defs(
        site_actions_path,
        "site action",
        content.site_action_defs,
        content.index.site_action_by_kind,
        [](const SiteActionDef& def) {
            return static_cast<std::uint32_t>(def.action_kind);
        });

    const auto issues = validate_content_database(content);
    for (const auto& issue : issues)
    {
        std::fprintf(
            stderr,
            "Content validation %s: %s\n",
            issue.severity == ContentValidationSeverity::Error
                ? "error"
                : (issue.severity == ContentValidationSeverity::Warning ? "warning" : "info"),
            issue.message == nullptr ? "(no message)" : issue.message);
        if (issue.severity == ContentValidationSeverity::Error)
        {
            std::abort();
        }
    }

    return content;
}

const ContentDatabase& prototype_content_database() noexcept
{
    static const ContentDatabase content = ContentLoader::load_prototype_content();
    return content;
}
}  // namespace gs1
