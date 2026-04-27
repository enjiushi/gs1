#include "content/content_loader.h"

#include "content/content_validator.h"
#include "support/currency.h"
#include "toml.hpp"

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
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
            "Failed to load content file %s: %.*s\n",
            path.string().c_str(),
            static_cast<int>(message.size()),
            message.data());
    }
    else
    {
        std::fprintf(
            stderr,
            "Failed to load content file %s:%zu: %.*s\n",
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
    if (field == "ExcavationOnly")
    {
        return ItemSourceRule::ExcavationOnly;
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

[[nodiscard]] PlantFocus parse_plant_focus(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "SetupProtection")
    {
        return PlantFocus::Setup;
    }
    if (field == "Setup")
    {
        return PlantFocus::Setup;
    }
    if (field == "Protection")
    {
        return PlantFocus::Protection;
    }
    if (field == "OutputWorkerSupport")
    {
        return PlantFocus::OutputWorkerSupport;
    }
    if (field == "SalinityOutput")
    {
        return PlantFocus::SalinityOutput;
    }
    if (field == "SoilSupport")
    {
        return PlantFocus::SoilSupport;
    }
    if (field == "Output")
    {
        return PlantFocus::Output;
    }
    if (field == "SoilSupportSalinity")
    {
        return PlantFocus::SoilSupportSalinity;
    }
    if (field == "ProtectionOutput")
    {
        return PlantFocus::ProtectionOutput;
    }
    if (field == "ProtectionWorkerSupport")
    {
        return PlantFocus::ProtectionWorkerSupport;
    }
    if (field == "ProtectionSoilSupport")
    {
        return PlantFocus::ProtectionSoilSupport;
    }

    fail_load(path, line_number, "invalid plant focus");
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
    if (field == "SubmitItem")
    {
        return TaskProgressKind::SubmitItem;
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
    if (field == "PerformAction")
    {
        return TaskProgressKind::PerformAction;
    }
    if (field == "CraftItem")
    {
        return TaskProgressKind::CraftItem;
    }
    if (field == "CraftAnyItem")
    {
        return TaskProgressKind::CraftAnyItem;
    }
    if (field == "BuildStructure")
    {
        return TaskProgressKind::BuildStructure;
    }
    if (field == "BuildAnyStructure")
    {
        return TaskProgressKind::BuildAnyStructure;
    }
    if (field == "BuildStructureSet")
    {
        return TaskProgressKind::BuildStructureSet;
    }
    if (field == "PlantAny")
    {
        return TaskProgressKind::PlantAny;
    }
    if (field == "EarnMoney")
    {
        return TaskProgressKind::EarnMoney;
    }
    if (field == "SellCraftedItem")
    {
        return TaskProgressKind::SellCraftedItem;
    }
    if (field == "KeepAllWorkerMetersAboveForDuration")
    {
        return TaskProgressKind::KeepAllWorkerMetersAboveForDuration;
    }
    if (field == "KeepTileMoistureAtLeastForDuration")
    {
        return TaskProgressKind::KeepTileMoistureAtLeastForDuration;
    }
    if (field == "KeepTileHeatAtMostForDuration")
    {
        return TaskProgressKind::KeepTileHeatAtMostForDuration;
    }
    if (field == "KeepTileDustAtMostForDuration")
    {
        return TaskProgressKind::KeepTileDustAtMostForDuration;
    }
    if (field == "PlantCountAtDensity")
    {
        return TaskProgressKind::PlantCountAtDensity;
    }
    if (field == "AnyPlantDensityAtLeast")
    {
        return TaskProgressKind::AnyPlantDensityAtLeast;
    }
    if (field == "PlantWindProtectionAtLeast")
    {
        return TaskProgressKind::PlantWindProtectionAtLeast;
    }
    if (field == "PlantHeatProtectionAtLeast")
    {
        return TaskProgressKind::PlantHeatProtectionAtLeast;
    }
    if (field == "PlantDustProtectionAtLeast")
    {
        return TaskProgressKind::PlantDustProtectionAtLeast;
    }
    if (field == "KeepAllDevicesIntegrityAboveForDuration")
    {
        return TaskProgressKind::KeepAllDevicesIntegrityAboveForDuration;
    }
    if (field == "NearbyPopulatedPlantTilesAtLeast")
    {
        return TaskProgressKind::NearbyPopulatedPlantTilesAtLeast;
    }
    if (field == "KeepAllLivingPlantsNotWitheringForDuration")
    {
        return TaskProgressKind::KeepAllLivingPlantsNotWitheringForDuration;
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
    if (field == "None")
    {
        return ActionKind::None;
    }
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
    if (field == "Harvest")
    {
        return ActionKind::Harvest;
    }
    if (field == "Excavate")
    {
        return ActionKind::Excavate;
    }

    fail_load(path, line_number, "invalid site action kind");
}

[[nodiscard]] ExcavationDepth parse_excavation_depth(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "None")
    {
        return ExcavationDepth::None;
    }
    if (field == "Rough")
    {
        return ExcavationDepth::Rough;
    }
    if (field == "Careful")
    {
        return ExcavationDepth::Careful;
    }
    if (field == "Thorough")
    {
        return ExcavationDepth::Thorough;
    }

    fail_load(path, line_number, "invalid excavation depth");
}

[[nodiscard]] ExcavationLootTier parse_excavation_loot_tier(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "None")
    {
        return ExcavationLootTier::None;
    }
    if (field == "Common")
    {
        return ExcavationLootTier::Common;
    }
    if (field == "Uncommon")
    {
        return ExcavationLootTier::Uncommon;
    }
    if (field == "Rare")
    {
        return ExcavationLootTier::Rare;
    }
    if (field == "VeryRare")
    {
        return ExcavationLootTier::VeryRare;
    }
    if (field == "Jackpot")
    {
        return ExcavationLootTier::Jackpot;
    }

    fail_load(path, line_number, "invalid excavation loot tier");
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

[[nodiscard]] TechnologyNodeKind parse_technology_node_kind(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "BaseTech")
    {
        return TechnologyNodeKind::BaseTech;
    }
    if (field == "Enhancement")
    {
        return TechnologyNodeKind::Enhancement;
    }

    fail_load(path, line_number, "invalid technology node kind");
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

[[nodiscard]] TechnologyGrantedContentKind parse_technology_granted_content_kind(
    const std::filesystem::path& path,
    std::size_t line_number,
    const std::string& field)
{
    if (field == "None")
    {
        return TechnologyGrantedContentKind::None;
    }
    if (field == "Item")
    {
        return TechnologyGrantedContentKind::Item;
    }
    if (field == "Plant")
    {
        return TechnologyGrantedContentKind::Plant;
    }
    if (field == "Structure")
    {
        return TechnologyGrantedContentKind::Structure;
    }
    if (field == "Recipe")
    {
        return TechnologyGrantedContentKind::Recipe;
    }

    fail_load(path, line_number, "invalid technology granted content kind");
}

[[nodiscard]] std::size_t toml_line_number(const toml::node& node) noexcept
{
    return static_cast<std::size_t>(node.source().begin.line);
}

[[nodiscard]] std::size_t toml_line_number(const toml::source_region& source) noexcept
{
    return static_cast<std::size_t>(source.begin.line);
}

[[nodiscard]] std::size_t toml_line_number(const toml::node_view<const toml::node>& node) noexcept
{
    return node.node() != nullptr ? toml_line_number(*node.node()) : 0U;
}

[[noreturn]] void fail_missing_toml_field(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    std::string message = "missing required field '";
    message += key;
    message += '\'';
    fail_load(path, toml_line_number(table), message);
}

[[noreturn]] void fail_invalid_toml_field(
    const std::filesystem::path& path,
    const toml::node_view<const toml::node>& node,
    std::string_view key,
    std::string_view expected)
{
    std::string message = "field '";
    message += key;
    message += "' ";
    message += expected;
    fail_load(path, toml_line_number(node), message);
}

[[nodiscard]] toml::table load_toml_document(const std::filesystem::path& path)
{
#if TOML_EXCEPTIONS
    try
    {
        return toml::parse_file(path.string());
    }
    catch (const toml::parse_error& error)
    {
        fail_load(path, toml_line_number(error.source()), error.description());
    }
#else
    auto parse_result = toml::parse_file(path.string());
    if (parse_result.failed())
    {
        fail_load(path, toml_line_number(parse_result.error().source()), parse_result.error().description());
    }

    return std::move(parse_result).table();
#endif
}

[[nodiscard]] toml::node_view<const toml::node> require_toml_field(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    const auto field = table[key];
    if (!field)
    {
        fail_missing_toml_field(path, table, key);
    }

    return field;
}

[[nodiscard]] std::string require_toml_string(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    const auto field = require_toml_field(path, table, key);
    if (const auto value = field.value<std::string_view>())
    {
        return std::string {*value};
    }

    fail_invalid_toml_field(path, field, key, "must be a string");
}

template <typename T>
[[nodiscard]] T require_toml_unsigned(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    const auto field = require_toml_field(path, table, key);
    const auto value = field.value<std::int64_t>();
    if (!value.has_value() || *value < 0 ||
        static_cast<std::uint64_t>(*value) > static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
    {
        fail_invalid_toml_field(path, field, key, "must be a non-negative integer");
    }

    return static_cast<T>(*value);
}

template <typename T>
[[nodiscard]] T require_toml_signed(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    const auto field = require_toml_field(path, table, key);
    const auto value = field.value<std::int64_t>();
    if (!value.has_value() ||
        *value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
        *value > static_cast<std::int64_t>(std::numeric_limits<T>::max()))
    {
        fail_invalid_toml_field(path, field, key, "must be an integer");
    }

    return static_cast<T>(*value);
}

template <typename T>
[[nodiscard]] std::optional<T> optional_toml_unsigned(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    const auto field = table[key];
    if (!field)
    {
        return std::nullopt;
    }

    const auto value = field.value<std::int64_t>();
    if (!value.has_value() || *value < 0 ||
        static_cast<std::uint64_t>(*value) > static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
    {
        fail_invalid_toml_field(path, field, key, "must be a non-negative integer");
    }

    return static_cast<T>(*value);
}

[[nodiscard]] std::optional<std::string> optional_toml_string(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    const auto field = table[key];
    if (!field)
    {
        return std::nullopt;
    }

    if (const auto value = field.value<std::string_view>())
    {
        return std::string {*value};
    }

    fail_invalid_toml_field(path, field, key, "must be a string");
}

[[nodiscard]] std::optional<float> optional_toml_float(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    const auto field = table[key];
    if (!field)
    {
        return std::nullopt;
    }

    if (const auto value = field.value<double>())
    {
        return static_cast<float>(*value);
    }
    if (const auto value = field.value<std::int64_t>())
    {
        return static_cast<float>(*value);
    }

    fail_invalid_toml_field(path, field, key, "must be numeric");
}

[[nodiscard]] float require_toml_float(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    const auto field = require_toml_field(path, table, key);
    if (const auto value = field.value<double>())
    {
        return static_cast<float>(*value);
    }
    if (const auto value = field.value<std::int64_t>())
    {
        return static_cast<float>(*value);
    }

    fail_invalid_toml_field(path, field, key, "must be numeric");
}

[[nodiscard]] double require_toml_double(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    return static_cast<double>(require_toml_float(path, table, key));
}

[[nodiscard]] bool require_toml_bool(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    const auto field = require_toml_field(path, table, key);
    if (const auto value = field.value<bool>())
    {
        return *value;
    }

    fail_invalid_toml_field(path, field, key, "must be a boolean");
}

[[nodiscard]] const toml::table& require_toml_table(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    const auto field = require_toml_field(path, table, key);
    if (const auto* value = field.as_table())
    {
        return *value;
    }

    fail_invalid_toml_field(path, field, key, "must be a table");
}

[[nodiscard]] const toml::array& require_toml_array(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    const auto field = require_toml_field(path, table, key);
    if (const auto* array = field.as_array())
    {
        return *array;
    }

    fail_invalid_toml_field(path, field, key, "must be an array");
}

[[nodiscard]] const toml::table& require_array_entry_table(
    const std::filesystem::path& path,
    const toml::node& node,
    std::string_view label)
{
    if (const auto* table = node.as_table())
    {
        return *table;
    }

    std::string message = "'";
    message += label;
    message += "' entries must be tables";
    fail_load(path, toml_line_number(node), message);
}

template <typename T>
[[nodiscard]] T require_toml_array_integer(
    const std::filesystem::path& path,
    const toml::node& node,
    std::string_view label)
{
    if (const auto value = node.value<std::int64_t>())
    {
        if constexpr (std::is_signed_v<T>)
        {
            if (*value >= static_cast<std::int64_t>(std::numeric_limits<T>::min()) &&
                *value <= static_cast<std::int64_t>(std::numeric_limits<T>::max()))
            {
                return static_cast<T>(*value);
            }
        }
        else if (*value >= 0 &&
            static_cast<std::uint64_t>(*value) <= static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
        {
            return static_cast<T>(*value);
        }
    }

    std::string message = "'";
    message += label;
    message += "' entries must be integers";
    fail_load(path, toml_line_number(node), message);
}

[[nodiscard]] PrototypeSupportItemContent parse_support_item_entry(
    const std::filesystem::path& path,
    const toml::table& table)
{
    return PrototypeSupportItemContent {
        ItemId {require_toml_unsigned<std::uint32_t>(path, table, "item_id")},
        require_toml_unsigned<std::uint32_t>(path, table, "quantity")};
}

[[nodiscard]] std::vector<PrototypeSupportItemContent> parse_support_item_array(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    std::vector<PrototypeSupportItemContent> values {};
    const auto& array = require_toml_array(path, table, key);
    values.reserve(array.size());
    for (const auto& node : array)
    {
        values.push_back(parse_support_item_entry(path, require_array_entry_table(path, node, key)));
    }

    return values;
}

[[nodiscard]] std::vector<ModifierId> parse_modifier_id_array(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    std::vector<ModifierId> values {};
    const auto& array = require_toml_array(path, table, key);
    values.reserve(array.size());
    for (const auto& node : array)
    {
        values.push_back(ModifierId {require_toml_array_integer<std::uint32_t>(path, node, key)});
    }

    return values;
}

[[nodiscard]] std::vector<std::uint32_t> parse_u32_array(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    std::vector<std::uint32_t> values {};
    const auto& array = require_toml_array(path, table, key);
    values.reserve(array.size());
    for (const auto& node : array)
    {
        values.push_back(require_toml_array_integer<std::uint32_t>(path, node, key));
    }

    return values;
}

[[nodiscard]] PrototypeSiteStartingPlantContent parse_starting_plant_entry(
    const std::filesystem::path& path,
    const toml::table& table)
{
    PrototypeSiteStartingPlantContent plant {};
    plant.plant_id = PlantId {require_toml_unsigned<std::uint32_t>(path, table, "plant_id")};
    plant.anchor_tile.x = require_toml_signed<std::int32_t>(path, table, "anchor_x");
    plant.anchor_tile.y = require_toml_signed<std::int32_t>(path, table, "anchor_y");
    plant.initial_density = require_toml_float(path, table, "initial_density");
    return plant;
}

[[nodiscard]] std::vector<PrototypeSiteStartingPlantContent> parse_starting_plant_array(
    const std::filesystem::path& path,
    const toml::table& table,
    std::string_view key)
{
    std::vector<PrototypeSiteStartingPlantContent> values {};
    const auto& array = require_toml_array(path, table, key);
    values.reserve(array.size());
    for (const auto& node : array)
    {
        values.push_back(parse_starting_plant_entry(path, require_array_entry_table(path, node, key)));
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
    const auto document = load_toml_document(path);
    const auto& sites = require_toml_array(path, document, "sites");
    for (const auto& node : sites)
    {
        const auto& entry = require_array_entry_table(path, node, "sites");

        PrototypeSiteContent site {};
        site.site_id = SiteId {require_toml_unsigned<std::uint32_t>(path, entry, "site_id")};
        site.site_archetype_id = require_toml_unsigned<std::uint32_t>(path, entry, "site_archetype_id");
        site.objective_type =
            parse_site_objective_type(path, toml_line_number(entry), require_toml_string(path, entry, "objective_type"));
        site.featured_faction_id =
            FactionId {require_toml_unsigned<std::uint32_t>(path, entry, "featured_faction_id")};
        site.initial_state =
            parse_site_state(path, toml_line_number(entry), require_toml_string(path, entry, "initial_state"));
        site.regional_map_tile.x = require_toml_signed<std::int32_t>(path, entry, "regional_map_tile_x");
        site.regional_map_tile.y = require_toml_signed<std::int32_t>(path, entry, "regional_map_tile_y");
        site.support_package_id = require_toml_unsigned<std::uint32_t>(path, entry, "support_package_id");
        site.exported_support_items = parse_support_item_array(path, entry, "exported_support_items");
        site.nearby_aura_modifier_ids = parse_modifier_id_array(path, entry, "nearby_aura_modifier_ids");
        site.site_completion_tile_threshold =
            require_toml_unsigned<std::uint32_t>(path, entry, "site_completion_tile_threshold");
        site.site_time_limit_minutes = require_toml_double(path, entry, "site_time_limit_minutes");
        site.objective_target_edge = parse_site_objective_target_edge(
            path,
            toml_line_number(entry),
            require_toml_string(path, entry, "objective_target_edge"));
        site.objective_target_band_width =
            require_toml_unsigned<std::uint8_t>(path, entry, "objective_target_band_width");
        site.highway_max_average_sand_cover =
            require_toml_float(path, entry, "highway_max_average_sand_cover");
        site.completion_reputation_reward =
            require_toml_signed<std::int32_t>(path, entry, "completion_reputation_reward");
        site.completion_faction_reputation_reward =
            require_toml_signed<std::int32_t>(path, entry, "completion_faction_reputation_reward");
        site.camp_anchor_tile.x = require_toml_signed<std::int32_t>(path, entry, "camp_anchor_x");
        site.camp_anchor_tile.y = require_toml_signed<std::int32_t>(path, entry, "camp_anchor_y");
        site.default_weather_heat = require_toml_float(path, entry, "default_weather_heat");
        site.default_weather_wind = require_toml_float(path, entry, "default_weather_wind");
        site.default_weather_dust = require_toml_float(path, entry, "default_weather_dust");
        site.default_weather_wind_direction_degrees =
            require_toml_float(path, entry, "default_weather_wind_direction_degrees");
        site.starting_plants = parse_starting_plant_array(path, entry, "starting_plants");
        site.phone_delivery_fee =
            cash_points_from_cash(require_toml_signed<std::int32_t>(path, entry, "phone_delivery_fee"));
        site.phone_delivery_minutes =
            require_toml_unsigned<std::uint16_t>(path, entry, "phone_delivery_minutes");
        site.initial_revealed_unlockable_ids =
            parse_u32_array(path, entry, "initial_revealed_unlockable_ids");
        site.initial_direct_purchase_unlockable_ids =
            parse_u32_array(path, entry, "initial_direct_purchase_unlockable_ids");

        content.prototype_campaign.sites.push_back(std::move(site));

        const auto site_id = content.prototype_campaign.sites.back().site_id;
        if (require_toml_bool(path, entry, "initially_revealed"))
        {
            content.prototype_campaign.initially_revealed_site_ids.push_back(site_id);
        }
        if (require_toml_bool(path, entry, "initially_available"))
        {
            content.prototype_campaign.initially_available_site_ids.push_back(site_id);
        }
    }
}

void load_prototype_campaign_setup(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    content.prototype_campaign.starting_campaign_cash =
        cash_points_from_cash(require_toml_signed<std::int32_t>(path, document, "starting_campaign_cash"));
    content.prototype_campaign.support_quota_per_contributor =
        require_toml_unsigned<std::uint32_t>(path, document, "support_quota_per_contributor");
    content.prototype_campaign.baseline_deployment_items =
        parse_support_item_array(path, document, "baseline_deployment_items");
}

void load_prototype_site_phone_listings(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& listings = require_toml_array(path, document, "phone_listings");
    for (const auto& node : listings)
    {
        const auto& entry = require_array_entry_table(path, node, "phone_listings");
        const SiteId site_id {require_toml_unsigned<std::uint32_t>(path, entry, "site_id")};
        auto* site = find_loaded_site(content.prototype_campaign, site_id);
        if (site == nullptr)
        {
            fail_load(path, toml_line_number(entry), "phone listing references an unknown site id");
        }

        const auto kind = parse_phone_listing_kind(
            path,
            toml_line_number(entry),
            require_toml_string(path, entry, "listing_kind"));
        const auto internal_price_cash_points =
            optional_toml_unsigned<std::uint32_t>(path, entry, "internal_price_cash_points").value_or(0U);
        std::int32_t price = 0;
        if (kind == PhoneListingKind::PurchaseUnlockable)
        {
            if (internal_price_cash_points == 0U)
            {
                fail_load(
                    path,
                    toml_line_number(entry),
                    "purchase-unlockable listings must author internal_price_cash_points");
            }
            if (!cash_points_are_cash_aligned(internal_price_cash_points))
            {
                fail_load(
                    path,
                    toml_line_number(entry),
                    "purchase-unlockable listing internal_price_cash_points must be divisible by 100");
            }

            price = static_cast<std::int32_t>(internal_price_cash_points);
        }
        else
        {
            price =
                kind == PhoneListingKind::HireContractor
                ? cash_points_from_cash(require_toml_signed<std::int32_t>(path, entry, "price"))
                : 0;
        }

        site->seeded_phone_listings.push_back(PrototypePhoneListingContent {
            require_toml_unsigned<std::uint32_t>(path, entry, "listing_id"),
            kind,
            require_toml_unsigned<std::uint32_t>(path, entry, "item_or_unlockable_id"),
            price,
            internal_price_cash_points,
            require_toml_unsigned<std::uint32_t>(path, entry, "quantity")});
    }
}

void load_item_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& items = require_toml_array(path, document, "items");
    for (const auto& node : items)
    {
        const auto& entry = require_array_entry_table(path, node, "items");
        content.item_defs.push_back(ItemDef {
            ItemId {require_toml_unsigned<std::uint32_t>(path, entry, "item_id")},
            store_string_view(content, require_toml_string(path, entry, "display_name")),
            require_toml_unsigned<std::uint16_t>(path, entry, "stack_size"),
            parse_item_source_rule(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "source_rule")),
            require_toml_bool(path, entry, "consumable"),
            optional_toml_unsigned<std::uint32_t>(path, entry, "internal_price_cash_points").value_or(0U),
            parse_item_capabilities(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "capabilities")),
            PlantId {require_toml_unsigned<std::uint32_t>(path, entry, "linked_plant_id")},
            StructureId {require_toml_unsigned<std::uint32_t>(path, entry, "linked_structure_id")},
            require_toml_float(path, entry, "health_delta"),
            require_toml_float(path, entry, "hydration_delta"),
            require_toml_float(path, entry, "nourishment_delta"),
            require_toml_float(path, entry, "energy_delta"),
            optional_toml_float(path, entry, "morale_delta").value_or(0.0f)});
    }
}

void load_task_template_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& task_templates = require_toml_array(path, document, "task_templates");
    for (const auto& node : task_templates)
    {
        const auto& entry = require_array_entry_table(path, node, "task_templates");
        content.task_template_defs.push_back(TaskTemplateDef {
            TaskTemplateId {require_toml_unsigned<std::uint32_t>(path, entry, "task_template_id")},
            FactionId {require_toml_unsigned<std::uint32_t>(path, entry, "publisher_faction_id")},
            require_toml_unsigned<std::uint32_t>(path, entry, "task_tier_id"),
            parse_task_progress_kind(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "progress_kind")),
            require_toml_unsigned<std::uint32_t>(path, entry, "target_amount"),
            require_toml_unsigned<std::uint32_t>(path, entry, "required_count"),
            ItemId {require_toml_unsigned<std::uint32_t>(path, entry, "item_id")},
            PlantId {require_toml_unsigned<std::uint32_t>(path, entry, "plant_id")},
            RecipeId {require_toml_unsigned<std::uint32_t>(path, entry, "recipe_id")},
            StructureId {require_toml_unsigned<std::uint32_t>(path, entry, "structure_id")},
            StructureId {require_toml_unsigned<std::uint32_t>(path, entry, "secondary_structure_id")},
            StructureId {require_toml_unsigned<std::uint32_t>(path, entry, "tertiary_structure_id")},
            parse_action_kind(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "action_kind")),
            require_toml_float(path, entry, "threshold_value"),
            require_toml_signed<std::int32_t>(path, entry, "completion_faction_reputation_delta"),
            require_toml_float(path, entry, "expected_task_hours_in_game"),
            require_toml_float(path, entry, "risk_multiplier")});
    }
}

void load_site_onboarding_task_seed_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& task_seeds = require_toml_array(path, document, "site_onboarding_task_seeds");
    for (const auto& node : task_seeds)
    {
        const auto& entry = require_array_entry_table(path, node, "site_onboarding_task_seeds");
        content.site_onboarding_task_seed_defs.push_back(SiteOnboardingTaskSeedDef {
            SiteId {require_toml_unsigned<std::uint32_t>(path, entry, "site_id")},
            TaskTemplateId {require_toml_unsigned<std::uint32_t>(path, entry, "task_template_id")},
            require_toml_unsigned<std::uint32_t>(path, entry, "target_amount"),
            require_toml_unsigned<std::uint32_t>(path, entry, "required_count"),
            ItemId {require_toml_unsigned<std::uint32_t>(path, entry, "item_id")},
            PlantId {require_toml_unsigned<std::uint32_t>(path, entry, "plant_id")},
            RecipeId {require_toml_unsigned<std::uint32_t>(path, entry, "recipe_id")},
            StructureId {require_toml_unsigned<std::uint32_t>(path, entry, "structure_id")},
            StructureId {require_toml_unsigned<std::uint32_t>(path, entry, "secondary_structure_id")},
            StructureId {require_toml_unsigned<std::uint32_t>(path, entry, "tertiary_structure_id")},
            parse_action_kind(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "action_kind")),
            require_toml_float(path, entry, "threshold_value"),
            RewardCandidateId {require_toml_unsigned<std::uint32_t>(path, entry, "reward_candidate_id")}});
    }
}

void load_reward_candidate_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& reward_candidates = require_toml_array(path, document, "reward_candidates");
    for (const auto& node : reward_candidates)
    {
        const auto& entry = require_array_entry_table(path, node, "reward_candidates");
        content.reward_candidate_defs.push_back(RewardCandidateDef {
            RewardCandidateId {require_toml_unsigned<std::uint32_t>(path, entry, "reward_candidate_id")},
            parse_reward_effect_kind(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "effect_kind")),
            cash_points_from_cash(require_toml_signed<std::int32_t>(path, entry, "money_delta")),
            ItemId {require_toml_unsigned<std::uint32_t>(path, entry, "item_id")},
            require_toml_unsigned<std::uint32_t>(path, entry, "quantity"),
            require_toml_unsigned<std::uint32_t>(path, entry, "unlockable_id"),
            ModifierId {require_toml_unsigned<std::uint32_t>(path, entry, "modifier_id")},
            FactionId {require_toml_unsigned<std::uint32_t>(path, entry, "faction_id")},
            require_toml_unsigned<std::uint16_t>(path, entry, "delivery_minutes"),
            require_toml_signed<std::int32_t>(path, entry, "reputation_delta")});
    }
}

void load_site_action_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& site_actions = require_toml_array(path, document, "site_actions");
    for (const auto& node : site_actions)
    {
        const auto& entry = require_array_entry_table(path, node, "site_actions");
        content.site_action_defs.push_back(SiteActionDef {
            parse_action_kind(path, toml_line_number(entry), require_toml_string(path, entry, "action_kind")),
            require_toml_float(path, entry, "duration_minutes_per_unit"),
            require_toml_float(path, entry, "energy_cost_per_unit"),
            require_toml_float(path, entry, "hydration_cost_per_unit"),
            require_toml_float(path, entry, "nourishment_cost_per_unit"),
            require_toml_float(path, entry, "morale_cost_per_unit"),
            require_toml_float(path, entry, "heat_to_energy_cost"),
            require_toml_float(path, entry, "wind_to_energy_cost"),
            require_toml_float(path, entry, "dust_to_energy_cost"),
            require_toml_float(path, entry, "heat_to_hydration_cost"),
            require_toml_float(path, entry, "wind_to_hydration_cost"),
            require_toml_float(path, entry, "dust_to_hydration_cost"),
            require_toml_float(path, entry, "heat_to_nourishment_cost"),
            require_toml_float(path, entry, "wind_to_nourishment_cost"),
            require_toml_float(path, entry, "dust_to_nourishment_cost"),
            require_toml_float(path, entry, "heat_to_morale_cost"),
            require_toml_float(path, entry, "wind_to_morale_cost"),
            require_toml_float(path, entry, "dust_to_morale_cost"),
            parse_placement_occupancy_layer(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "placement_occupancy_layer")),
            require_toml_bool(path, entry, "requests_placement_reservation"),
            require_toml_bool(path, entry, "requires_worker_approach"),
            require_toml_bool(path, entry, "impacts_worker_movement")});
    }
}

void load_excavation_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& excavation_depths = require_toml_array(path, document, "excavation_depths");
    for (const auto& node : excavation_depths)
    {
        const auto& entry = require_array_entry_table(path, node, "excavation_depths");
        content.excavation_depth_defs.push_back(ExcavationDepthDef {
            parse_excavation_depth(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "depth")),
            require_toml_float(path, entry, "energy_cost_multiplier"),
            require_toml_float(path, entry, "find_chance_percent"),
            require_toml_float(path, entry, "common_tier_percent"),
            require_toml_float(path, entry, "uncommon_tier_percent"),
            require_toml_float(path, entry, "rare_tier_percent"),
            require_toml_float(path, entry, "very_rare_tier_percent"),
            require_toml_float(path, entry, "jackpot_tier_percent")});
    }

    const auto& excavation_loot_entries = require_toml_array(path, document, "excavation_loot_entries");
    for (const auto& node : excavation_loot_entries)
    {
        const auto& entry = require_array_entry_table(path, node, "excavation_loot_entries");
        content.excavation_loot_entry_defs.push_back(ExcavationLootEntryDef {
            parse_excavation_depth(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "depth")),
            ItemId {require_toml_unsigned<std::uint32_t>(path, entry, "item_id")},
            parse_excavation_loot_tier(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "tier")),
            require_toml_float(path, entry, "percent_within_tier")});
    }
}

void load_modifier_preset_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& modifier_presets = require_toml_array(path, document, "modifier_presets");
    for (const auto& node : modifier_presets)
    {
        const auto& entry = require_array_entry_table(path, node, "modifier_presets");
        ModifierPresetDef preset {};
        preset.preset_kind =
            parse_modifier_preset_kind(path, toml_line_number(entry), require_toml_string(path, entry, "preset_kind"));
        preset.preset_index = require_toml_unsigned<std::uint32_t>(path, entry, "preset_index");
        preset.totals.heat = require_toml_float(path, entry, "heat");
        preset.totals.wind = require_toml_float(path, entry, "wind");
        preset.totals.dust = require_toml_float(path, entry, "dust");
        preset.totals.moisture = require_toml_float(path, entry, "moisture");
        preset.totals.fertility = require_toml_float(path, entry, "fertility");
        preset.totals.salinity = require_toml_float(path, entry, "salinity");
        preset.totals.growth_pressure = require_toml_float(path, entry, "growth_pressure");
        preset.totals.salinity_density_cap = require_toml_float(path, entry, "salinity_density_cap");
        preset.totals.plant_density = require_toml_float(path, entry, "plant_density");
        preset.totals.health = require_toml_float(path, entry, "health");
        preset.totals.hydration = require_toml_float(path, entry, "hydration");
        preset.totals.nourishment = require_toml_float(path, entry, "nourishment");
        preset.totals.energy_cap = require_toml_float(path, entry, "energy_cap");
        preset.totals.energy = require_toml_float(path, entry, "energy");
        preset.totals.morale = require_toml_float(path, entry, "morale");
        preset.totals.work_efficiency = require_toml_float(path, entry, "work_efficiency");

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

void load_gameplay_tuning_def(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    auto& tuning = content.gameplay_tuning;

    const auto& worker_condition = require_toml_table(path, document, "worker_condition");
    tuning.worker_condition.hydration_base_loss_per_game_minute =
        require_toml_float(path, worker_condition, "hydration_base_loss_per_game_minute");
    tuning.worker_condition.heat_to_hydration_factor =
        require_toml_float(path, worker_condition, "heat_to_hydration_factor");
    tuning.worker_condition.wind_to_hydration_factor =
        require_toml_float(path, worker_condition, "wind_to_hydration_factor");
    tuning.worker_condition.dust_to_hydration_factor =
        require_toml_float(path, worker_condition, "dust_to_hydration_factor");
    tuning.worker_condition.nourishment_base_loss_per_game_minute =
        require_toml_float(path, worker_condition, "nourishment_base_loss_per_game_minute");
    tuning.worker_condition.wind_to_nourishment_factor =
        require_toml_float(path, worker_condition, "wind_to_nourishment_factor");
    tuning.worker_condition.heat_to_nourishment_factor =
        require_toml_float(path, worker_condition, "heat_to_nourishment_factor");
    tuning.worker_condition.dust_to_nourishment_factor =
        require_toml_float(path, worker_condition, "dust_to_nourishment_factor");
    tuning.worker_condition.energy_background_increase_real_minutes =
        require_toml_float(path, worker_condition, "energy_background_increase_real_minutes");
    tuning.worker_condition.energy_background_min_speed_factor =
        require_toml_float(path, worker_condition, "energy_background_min_speed_factor");
    tuning.worker_condition.morale_background_increase_real_minutes =
        require_toml_float(path, worker_condition, "morale_background_increase_real_minutes");
    tuning.worker_condition.morale_background_decrease_real_minutes =
        require_toml_float(path, worker_condition, "morale_background_decrease_real_minutes");
    tuning.worker_condition.morale_support_real_minutes =
        require_toml_float(path, worker_condition, "morale_support_real_minutes");
    tuning.worker_condition.heat_to_health_factor =
        require_toml_float(path, worker_condition, "heat_to_health_factor");
    tuning.worker_condition.wind_to_health_factor =
        require_toml_float(path, worker_condition, "wind_to_health_factor");
    tuning.worker_condition.dust_to_health_factor =
        require_toml_float(path, worker_condition, "dust_to_health_factor");
    tuning.worker_condition.health_energy_cap_factor =
        require_toml_float(path, worker_condition, "health_energy_cap_factor");
    tuning.worker_condition.hydration_energy_cap_factor =
        require_toml_float(path, worker_condition, "hydration_energy_cap_factor");
    tuning.worker_condition.nourishment_energy_cap_factor =
        require_toml_float(path, worker_condition, "nourishment_energy_cap_factor");
    tuning.worker_condition.energy_cap_factor =
        require_toml_float(path, worker_condition, "energy_cap_factor");
    tuning.worker_condition.health_efficiency_factor =
        require_toml_float(path, worker_condition, "health_efficiency_factor");
    tuning.worker_condition.hydration_efficiency_factor =
        require_toml_float(path, worker_condition, "hydration_efficiency_factor");
    tuning.worker_condition.nourishment_efficiency_factor =
        require_toml_float(path, worker_condition, "nourishment_efficiency_factor");
    tuning.worker_condition.morale_efficiency_factor =
        require_toml_float(path, worker_condition, "morale_efficiency_factor");
    tuning.worker_condition.work_efficiency_factor =
        require_toml_float(path, worker_condition, "work_efficiency_factor");
    tuning.worker_condition.factor_weight_default =
        require_toml_float(path, worker_condition, "factor_weight_default");
    tuning.worker_condition.factor_bias_default =
        require_toml_float(path, worker_condition, "factor_bias_default");
    tuning.worker_condition.factor_min =
        require_toml_float(path, worker_condition, "factor_min");
    tuning.worker_condition.factor_max =
        require_toml_float(path, worker_condition, "factor_max");
    tuning.worker_condition.sheltered_exposure_scale =
        require_toml_float(path, worker_condition, "sheltered_exposure_scale");

    const auto& player_meter_cash_points = require_toml_table(path, document, "player_meter_cash_points");
    tuning.player_meter_cash_points.health_per_point =
        require_toml_float(path, player_meter_cash_points, "health_per_point");
    tuning.player_meter_cash_points.hydration_per_point =
        require_toml_float(path, player_meter_cash_points, "hydration_per_point");
    tuning.player_meter_cash_points.nourishment_per_point =
        require_toml_float(path, player_meter_cash_points, "nourishment_per_point");
    tuning.player_meter_cash_points.energy_per_point =
        require_toml_float(path, player_meter_cash_points, "energy_per_point");
    tuning.player_meter_cash_points.morale_per_point =
        require_toml_float(path, player_meter_cash_points, "morale_per_point");
    tuning.player_meter_cash_points.buy_price_multiplier =
        require_toml_float(path, player_meter_cash_points, "buy_price_multiplier");
    tuning.player_meter_cash_points.sell_price_multiplier =
        require_toml_float(path, player_meter_cash_points, "sell_price_multiplier");

    const auto& ecology = require_toml_table(path, document, "ecology");
    tuning.ecology.moisture_gain_per_water_unit =
        require_toml_float(path, ecology, "moisture_gain_per_water_unit");
    tuning.ecology.fertility_to_moisture_cap_factor =
        require_toml_float(path, ecology, "fertility_to_moisture_cap_factor");
    tuning.ecology.salinity_to_fertility_cap_factor =
        require_toml_float(path, ecology, "salinity_to_fertility_cap_factor");
    tuning.ecology.moisture_factor =
        require_toml_float(path, ecology, "moisture_factor");
    tuning.ecology.heat_to_moisture_factor =
        require_toml_float(path, ecology, "heat_to_moisture_factor");
    tuning.ecology.wind_to_moisture_factor =
        require_toml_float(path, ecology, "wind_to_moisture_factor");
    tuning.ecology.fertility_factor =
        require_toml_float(path, ecology, "fertility_factor");
    tuning.ecology.wind_to_fertility_factor =
        require_toml_float(path, ecology, "wind_to_fertility_factor");
    tuning.ecology.dust_to_fertility_factor =
        require_toml_float(path, ecology, "dust_to_fertility_factor");
    tuning.ecology.salinity_factor =
        require_toml_float(path, ecology, "salinity_factor");
    tuning.ecology.salinity_source =
        require_toml_float(path, ecology, "salinity_source");
    tuning.ecology.growth_relief_from_moisture =
        require_toml_float(path, ecology, "growth_relief_from_moisture");
    tuning.ecology.growth_relief_from_fertility =
        require_toml_float(path, ecology, "growth_relief_from_fertility");
    tuning.ecology.growth_pressure_heat_scale =
        require_toml_float(path, ecology, "growth_pressure_heat_scale");
    tuning.ecology.growth_pressure_wind_scale =
        require_toml_float(path, ecology, "growth_pressure_wind_scale");
    tuning.ecology.growth_pressure_dust_scale =
        require_toml_float(path, ecology, "growth_pressure_dust_scale");
    tuning.ecology.growth_pressure_base =
        require_toml_float(path, ecology, "growth_pressure_base");
    tuning.ecology.growth_pressure_heat_weight =
        require_toml_float(path, ecology, "growth_pressure_heat_weight");
    tuning.ecology.growth_pressure_wind_weight =
        require_toml_float(path, ecology, "growth_pressure_wind_weight");
    tuning.ecology.growth_pressure_dust_weight =
        require_toml_float(path, ecology, "growth_pressure_dust_weight");
    tuning.ecology.growth_pressure_burial_weight =
        require_toml_float(path, ecology, "growth_pressure_burial_weight");
    tuning.ecology.growth_pressure_salinity_weight =
        require_toml_float(path, ecology, "growth_pressure_salinity_weight");
    tuning.ecology.growth_pressure_dust_burial_scale =
        require_toml_float(path, ecology, "growth_pressure_dust_burial_scale");
    tuning.ecology.growth_pressure_modifier_influence =
        require_toml_float(path, ecology, "growth_pressure_modifier_influence");
    tuning.ecology.density_full_range_real_minutes =
        require_toml_float(path, ecology, "density_full_range_real_minutes");
    tuning.ecology.salinity_cap_softening =
        require_toml_float(path, ecology, "salinity_cap_softening");
    tuning.ecology.salinity_cap_min_unit =
        require_toml_float(path, ecology, "salinity_cap_min_unit");
    tuning.ecology.salinity_density_cap_modifier_influence =
        require_toml_float(path, ecology, "salinity_density_cap_modifier_influence");
    tuning.ecology.resistance_density_influence =
        require_toml_float(path, ecology, "resistance_density_influence");
    tuning.ecology.tolerance_percent_scale =
        require_toml_float(path, ecology, "tolerance_percent_scale");
    tuning.ecology.density_salinity_overcap_loss_scale =
        require_toml_float(path, ecology, "density_salinity_overcap_loss_scale");
    tuning.ecology.density_modifier_influence =
        require_toml_float(path, ecology, "density_modifier_influence");
    tuning.ecology.constant_wither_rate_scale =
        require_toml_float(path, ecology, "constant_wither_rate_scale");
    tuning.ecology.highway_cover_gain_wind_scale =
        require_toml_float(path, ecology, "highway_cover_gain_wind_scale");
    tuning.ecology.highway_cover_gain_dust_scale =
        require_toml_float(path, ecology, "highway_cover_gain_dust_scale");

    const auto& modifier_system = require_toml_table(path, document, "modifier_system");
    tuning.modifier_system.modifier_channel_limit =
        require_toml_float(path, modifier_system, "modifier_channel_limit");
    tuning.modifier_system.factor_weight_limit =
        require_toml_float(path, modifier_system, "factor_weight_limit");
    tuning.modifier_system.factor_bias_limit =
        require_toml_float(path, modifier_system, "factor_bias_limit");
    tuning.modifier_system.camp_comfort_morale_scale =
        require_toml_float(path, modifier_system, "camp_comfort_morale_scale");
    tuning.modifier_system.camp_comfort_work_efficiency_scale =
        require_toml_float(path, modifier_system, "camp_comfort_work_efficiency_scale");

    const auto& device_support = require_toml_table(path, document, "device_support");
    tuning.device_support.water_evaporation_base =
        require_toml_float(path, device_support, "water_evaporation_base");
    tuning.device_support.heat_evaporation_multiplier =
        require_toml_float(path, device_support, "heat_evaporation_multiplier");

    const auto& camp_durability = require_toml_table(path, document, "camp_durability");
    tuning.camp_durability.durability_max =
        require_toml_float(path, camp_durability, "durability_max");
    tuning.camp_durability.base_wear_per_second =
        require_toml_float(path, camp_durability, "base_wear_per_second");
    tuning.camp_durability.weather_wear_scale =
        require_toml_float(path, camp_durability, "weather_wear_scale");
    tuning.camp_durability.event_timeline_wear_per_second =
        require_toml_float(path, camp_durability, "event_timeline_wear_per_second");
    tuning.camp_durability.peak_event_pressure_total =
        require_toml_float(path, camp_durability, "peak_event_pressure_total");
    tuning.camp_durability.protection_threshold =
        require_toml_float(path, camp_durability, "protection_threshold");
    tuning.camp_durability.delivery_threshold =
        require_toml_float(path, camp_durability, "delivery_threshold");
    tuning.camp_durability.shared_storage_threshold =
        require_toml_float(path, camp_durability, "shared_storage_threshold");
}

void load_technology_tier_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& technology_tiers = require_toml_array(path, document, "technology_tiers");
    for (const auto& node : technology_tiers)
    {
        const auto& entry = require_array_entry_table(path, node, "technology_tiers");
        content.technology_tier_defs.push_back(TechnologyTierDef {
            require_toml_unsigned<std::uint8_t>(path, entry, "tier_index"),
            {0U, 0U, 0U},
            store_string_view(content, require_toml_string(path, entry, "display_name"))});
    }
}

void load_reputation_unlock_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& reputation_unlocks = require_toml_array(path, document, "reputation_unlocks");
    for (const auto& node : reputation_unlocks)
    {
        const auto& entry = require_array_entry_table(path, node, "reputation_unlocks");
        content.reputation_unlock_defs.push_back(ReputationUnlockDef {
            require_toml_unsigned<std::uint32_t>(path, entry, "unlock_id"),
            parse_reputation_unlock_kind(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "unlock_kind")),
            {0U, 0U, 0U},
            require_toml_signed<std::int32_t>(path, entry, "reputation_requirement"),
            require_toml_unsigned<std::uint32_t>(path, entry, "content_id"),
            store_string_view(content, require_toml_string(path, entry, "display_name")),
            store_string_view(content, require_toml_string(path, entry, "description"))});
    }
}

void load_technology_node_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& technology_nodes = require_toml_array(path, document, "technology_nodes");
    for (const auto& node : technology_nodes)
    {
        const auto& entry = require_array_entry_table(path, node, "technology_nodes");
        const auto tier_index = require_toml_unsigned<std::uint8_t>(path, entry, "tier_index");
        const auto faction_id = FactionId {
            require_toml_unsigned<std::uint32_t>(path, entry, "faction_id")};
        const auto node_kind = parse_technology_node_kind(
            path,
            toml_line_number(entry),
            require_toml_string(path, entry, "node_kind"));
        const auto enhancement_choice_index =
            require_toml_unsigned<std::uint8_t>(path, entry, "enhancement_choice_index");
        const auto entry_kind = parse_technology_entry_kind(
            path,
            toml_line_number(entry),
            require_toml_string(path, entry, "entry_kind"));
        const auto granted_content_kind = parse_technology_granted_content_kind(
            path,
            toml_line_number(entry),
            optional_toml_string(path, entry, "granted_content_kind").value_or("None"));
        const auto granted_content_id =
            optional_toml_unsigned<std::uint32_t>(path, entry, "granted_content_id").value_or(0U);
        const auto tech_node_id = TechNodeId {
            technology_node_id(faction_id, tier_index, enhancement_choice_index)};
        content.technology_node_defs.push_back(TechnologyNodeDef {
            tech_node_id,
            faction_id,
            tier_index,
            node_kind,
            enhancement_choice_index,
            {0U},
            entry_kind,
            {0U, 0U, 0U},
            require_toml_signed<std::int32_t>(path, entry, "reputation_requirement"),
            require_toml_unsigned<std::uint32_t>(path, entry, "internal_cost_cash_points"),
            require_toml_float(path, entry, "unlock_effect_parameter"),
            require_toml_float(path, entry, "effect_parameter_per_bonus_reputation"),
            entry_kind == TechnologyEntryKind::GlobalModifier
                ? ModifierId {technology_modifier_id(tech_node_id)}
                : ModifierId {},
            entry_kind == TechnologyEntryKind::MechanismChange
                ? technology_mechanism_change_id(tech_node_id)
                : 0U,
            granted_content_kind,
            {0U, 0U, 0U},
            granted_content_id,
            require_toml_bool(path, entry, "is_todo_placeholder"),
            {0U, 0U, 0U},
            store_string_view(content, require_toml_string(path, entry, "display_name")),
            store_string_view(content, require_toml_string(path, entry, "description"))});
    }
}

void load_initial_unlocked_plant_ids(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& plant_ids = require_toml_array(path, document, "plant_ids");
    content.initial_unlocked_plant_ids.reserve(plant_ids.size());
    for (const auto& node : plant_ids)
    {
        content.initial_unlocked_plant_ids.push_back(
            PlantId {require_toml_array_integer<std::uint32_t>(path, node, "plant_ids")});
    }
}

void load_plant_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& plants = require_toml_array(path, document, "plants");
    for (const auto& node : plants)
    {
        const auto& entry = require_array_entry_table(path, node, "plants");
        content.plant_defs.push_back(PlantDef {
            PlantId {require_toml_unsigned<std::uint32_t>(path, entry, "plant_id")},
            store_cstring(content, require_toml_string(path, entry, "display_name")),
            parse_plant_height_class(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "height_class")),
            parse_plant_focus(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "focus")),
            require_toml_bool(path, entry, "growable"),
            require_toml_unsigned<std::uint8_t>(path, entry, "aura_size"),
            require_toml_unsigned<std::uint8_t>(path, entry, "footprint_width"),
            require_toml_unsigned<std::uint8_t>(path, entry, "footprint_height"),
            require_toml_unsigned<std::uint8_t>(path, entry, "wind_protection_range"),
            require_toml_float(path, entry, "protection_ratio"),
            require_toml_float(path, entry, "plant_action_duration_minutes"),
            require_toml_float(path, entry, "constant_wither_rate"),
            require_toml_float(path, entry, "salt_tolerance"),
            require_toml_float(path, entry, "heat_tolerance"),
            require_toml_float(path, entry, "wind_resistance"),
            require_toml_float(path, entry, "dust_tolerance"),
            require_toml_float(path, entry, "fertility_improve_power"),
            require_toml_float(path, entry, "output_power"),
            require_toml_float(path, entry, "spread_readiness"),
            require_toml_float(path, entry, "spread_chance"),
            require_toml_float(path, entry, "output_dependency"),
            ItemId {require_toml_unsigned<std::uint32_t>(path, entry, "harvest_item_id")},
            require_toml_unsigned<std::uint16_t>(path, entry, "harvest_quantity"),
            0U,
            require_toml_float(path, entry, "harvest_action_duration_minutes"),
            require_toml_float(path, entry, "harvest_density_required"),
            require_toml_float(path, entry, "harvest_density_removed")});
    }
}

void load_structure_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& structures = require_toml_array(path, document, "structures");
    for (const auto& node : structures)
    {
        const auto& entry = require_array_entry_table(path, node, "structures");
        content.structure_defs.push_back(StructureDef {
            StructureId {require_toml_unsigned<std::uint32_t>(path, entry, "structure_id")},
            store_string_view(content, require_toml_string(path, entry, "display_name")),
            require_toml_float(path, entry, "durability"),
            require_toml_unsigned<std::uint8_t>(path, entry, "aura_size"),
            require_toml_float(path, entry, "wind_protection_value"),
            require_toml_unsigned<std::uint8_t>(path, entry, "wind_protection_range"),
            require_toml_float(path, entry, "heat_protection_value"),
            require_toml_float(path, entry, "dust_protection_value"),
            require_toml_float(path, entry, "fertility_improve_value"),
            require_toml_float(path, entry, "salinity_reduction_value"),
            require_toml_float(path, entry, "irrigation_value"),
            require_toml_unsigned<std::uint16_t>(path, entry, "storage_slot_count"),
            parse_crafting_station_kind(
                path,
                toml_line_number(entry),
                require_toml_string(path, entry, "crafting_station_kind")),
            require_toml_bool(path, entry, "grants_storage"),
            require_toml_unsigned<std::uint8_t>(path, entry, "footprint_width"),
            require_toml_unsigned<std::uint8_t>(path, entry, "footprint_height")});
    }
}

void load_craft_recipe_defs(ContentDatabase& content, const std::filesystem::path& path)
{
    const auto document = load_toml_document(path);
    const auto& craft_recipes = require_toml_array(path, document, "craft_recipes");
    for (const auto& node : craft_recipes)
    {
        const auto& entry = require_array_entry_table(path, node, "craft_recipes");
        CraftRecipeDef recipe_def {};
        recipe_def.recipe_id = RecipeId {require_toml_unsigned<std::uint32_t>(path, entry, "recipe_id")};
        recipe_def.station_structure_id =
            StructureId {require_toml_unsigned<std::uint32_t>(path, entry, "station_structure_id")};
        recipe_def.output_item_id = ItemId {require_toml_unsigned<std::uint32_t>(path, entry, "output_item_id")};
        recipe_def.output_quantity = require_toml_unsigned<std::uint16_t>(path, entry, "output_quantity");
        recipe_def.craft_minutes = require_toml_float(path, entry, "craft_minutes");
        recipe_def.energy_cost = require_toml_float(path, entry, "energy_cost");
        recipe_def.hydration_cost = require_toml_float(path, entry, "hydration_cost");
        recipe_def.nourishment_cost = require_toml_float(path, entry, "nourishment_cost");

        const auto& ingredients = require_toml_array(path, entry, "ingredients");
        if (ingredients.size() > recipe_def.ingredients.size())
        {
            fail_load(path, toml_line_number(entry), "too many craft recipe ingredients");
        }

        for (std::size_t index = 0; index < ingredients.size(); ++index)
        {
            const auto& ingredient = require_array_entry_table(path, ingredients[index], "ingredients");
            recipe_def.ingredients[index] = CraftRecipeIngredientDef {
                ItemId {require_toml_unsigned<std::uint32_t>(path, ingredient, "item_id")},
                require_toml_unsigned<std::uint16_t>(path, ingredient, "quantity")};
        }

        recipe_def.ingredient_count = static_cast<std::uint8_t>(ingredients.size());

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
    const auto sites_path = root / "sites.toml";
    const auto campaign_setup_path = root / "campaign_setup.toml";
    const auto phone_listings_path = root / "phone_listings.toml";
    const auto items_path = root / "items.toml";
    const auto plants_path = root / "plants.toml";
    const auto structures_path = root / "structures.toml";
    const auto craft_recipes_path = root / "craft_recipes.toml";
    const auto excavation_tables_path = root / "excavation_tables.toml";
    const auto task_templates_path = root / "task_templates.toml";
    const auto site_onboarding_task_seeds_path = root / "site_onboarding_task_seeds.toml";
    const auto reward_candidates_path = root / "reward_candidates.toml";
    const auto site_actions_path = root / "site_actions.toml";
    const auto modifier_presets_path = root / "modifier_presets.toml";
    const auto gameplay_tuning_path = root / "gameplay_tuning.toml";
    const auto technology_tiers_path = root / "technology_tiers.toml";
    const auto reputation_unlocks_path = root / "reputation_unlocks.toml";
    const auto technology_nodes_path = root / "technology_nodes.toml";
    const auto initial_unlocked_plants_path = root / "initial_unlocked_plants.toml";

    load_prototype_campaign_sites(content, sites_path);
    load_prototype_campaign_setup(content, campaign_setup_path);
    load_prototype_site_phone_listings(content, phone_listings_path);
    load_item_defs(content, items_path);
    load_plant_defs(content, plants_path);
    load_structure_defs(content, structures_path);
    load_craft_recipe_defs(content, craft_recipes_path);
    load_excavation_defs(content, excavation_tables_path);
    load_task_template_defs(content, task_templates_path);
    load_site_onboarding_task_seed_defs(content, site_onboarding_task_seeds_path);
    load_reward_candidate_defs(content, reward_candidates_path);
    load_site_action_defs(content, site_actions_path);
    load_modifier_preset_defs(content, modifier_presets_path);
    load_gameplay_tuning_def(content, gameplay_tuning_path);
    load_technology_tier_defs(content, technology_tiers_path);
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
