#include "content/content_validator.h"

#include "content/defs/faction_defs.h"
#include "support/currency.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace gs1
{
namespace
{
inline constexpr float k_starter_plant_pool = 90.0f;
inline constexpr float k_unlock_step_plant_pool = 10.0f;

[[nodiscard]] bool is_power_of_two(std::uint8_t value) noexcept
{
    return value != 0U && (value & static_cast<std::uint8_t>(value - 1U)) == 0U;
}

[[nodiscard]] bool is_axis_aligned_to_span(std::int32_t coord, std::uint8_t span) noexcept
{
    const std::int32_t normalized_span =
        static_cast<std::int32_t>(span == 0U ? 1U : span);
    return (coord % normalized_span) == 0;
}

[[nodiscard]] bool is_density_meter_in_range(float value) noexcept
{
    return value >= 0.0f && value <= 100.0f;
}

[[nodiscard]] bool is_weather_meter_in_range(float value) noexcept
{
    return value >= 0.0f && value <= 100.0f;
}

[[nodiscard]] bool percent_total_is_100(float value) noexcept
{
    return std::fabs(value - 100.0f) <= 0.001f;
}

[[nodiscard]] float max_output_power_for_item_value(std::uint32_t internal_cash_points) noexcept
{
    return static_cast<float>(internal_cash_points) / 9.0f;
}

[[nodiscard]] bool item_has_player_meter_valuation(const ItemDef& item_def) noexcept
{
    return item_def.health_delta != 0.0f ||
        item_def.hydration_delta != 0.0f ||
        item_def.nourishment_delta != 0.0f ||
        item_def.energy_delta != 0.0f ||
        item_def.morale_delta != 0.0f;
}

[[nodiscard]] const ModifierDef* find_content_modifier_def(
    const ContentDatabase& content,
    ModifierId modifier_id) noexcept
{
    const auto it = content.index.modifier_by_id.find(modifier_id.value);
    return it == content.index.modifier_by_id.end()
        ? nullptr
        : &content.modifier_defs[it->second];
}

[[nodiscard]] std::uint32_t resolved_item_cash_points_for_validation(
    const ContentDatabase& content,
    const ItemDef& item_def) noexcept
{
    constexpr std::uint32_t k_timed_buff_internal_cash_points = 200U;
    const auto modifier_it = content.index.modifier_by_id.find(item_def.modifier_id.value);
    const ModifierDef* modifier_def =
        modifier_it == content.index.modifier_by_id.end()
            ? nullptr
            : &content.modifier_defs[modifier_it->second];
    const auto& meter_cash_points = content.gameplay_tuning.player_meter_cash_points;
    const double derived_value =
        static_cast<double>(item_def.health_delta) * static_cast<double>(meter_cash_points.health_per_point) +
        static_cast<double>(item_def.hydration_delta) * static_cast<double>(meter_cash_points.hydration_per_point) +
        static_cast<double>(item_def.nourishment_delta) * static_cast<double>(meter_cash_points.nourishment_per_point) +
        static_cast<double>(item_def.energy_delta) * static_cast<double>(meter_cash_points.energy_per_point) +
        static_cast<double>(item_def.morale_delta) * static_cast<double>(meter_cash_points.morale_per_point);
    if (derived_value > 0.0)
    {
        return static_cast<std::uint32_t>(std::lround(derived_value)) +
            (modifier_def == nullptr || modifier_def->duration_eight_hour_blocks == 0U
                    ? 0U
                    : k_timed_buff_internal_cash_points);
    }

    return item_def.internal_price_cash_points +
        (modifier_def == nullptr || modifier_def->duration_eight_hour_blocks == 0U
                ? 0U
                : k_timed_buff_internal_cash_points);
}

[[nodiscard]] std::unordered_map<std::uint32_t, float> build_expected_plant_pool_targets(
    const ContentDatabase& content)
{
    std::unordered_map<std::uint32_t, float> targets {};
    for (const auto plant_id : content.initial_unlocked_plant_ids)
    {
        targets.insert_or_assign(plant_id.value, k_starter_plant_pool);
    }

    std::vector<const ReputationUnlockDef*> plant_unlocks {};
    plant_unlocks.reserve(content.reputation_unlock_defs.size());
    for (const auto& unlock_def : content.reputation_unlock_defs)
    {
        if (unlock_def.unlock_kind == ReputationUnlockKind::Plant)
        {
            plant_unlocks.push_back(&unlock_def);
        }
    }

    std::sort(
        plant_unlocks.begin(),
        plant_unlocks.end(),
        [](const ReputationUnlockDef* lhs, const ReputationUnlockDef* rhs)
        {
            if (lhs->reputation_requirement != rhs->reputation_requirement)
            {
                return lhs->reputation_requirement < rhs->reputation_requirement;
            }

            return lhs->content_id < rhs->content_id;
        });

    for (std::size_t index = 0U; index < plant_unlocks.size(); ++index)
    {
        targets.insert_or_assign(
            plant_unlocks[index]->content_id,
            k_starter_plant_pool + (k_unlock_step_plant_pool * static_cast<float>(index + 1U)));
    }

    return targets;
}
}  // namespace

std::vector<ContentValidationIssue> validate_content_database(
    const ContentDatabase& content)
{
    std::vector<ContentValidationIssue> issues {};

    for (const auto& plant_def : content.plant_defs)
    {
        if (!is_power_of_two(plant_def.footprint_width) ||
            !is_power_of_two(plant_def.footprint_height))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Plant footprints must use power-of-two tile sizes."});
            break;
        }

        if (!is_weather_meter_in_range(plant_def.salt_tolerance) ||
            !is_weather_meter_in_range(plant_def.heat_tolerance) ||
            !is_weather_meter_in_range(plant_def.wind_resistance) ||
            !is_weather_meter_in_range(plant_def.dust_tolerance))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Plant resistance and tolerance values must stay in the 0-100 range."});
            break;
        }

        if (plant_def.protection_ratio < 0.0f || plant_def.protection_ratio > 1.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Plant protection ratios must stay in the 0-1 range."});
            break;
        }

        if (plant_def.aura_size > 3U || plant_def.wind_protection_range > 3U)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Plant aura and wind-protection ranges must stay in the authored 0-3 range."});
            break;
        }

        if (plant_def.fertility_improve_power < 0.0f ||
            plant_def.output_power < 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Plant fertility and output meters must be non-negative."});
            break;
        }

        if (!plant_focus_has_aura(plant_def.focus) && plant_def.aura_size != 0U)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Only protection or support focused plants may project a non-zero aura range."});
            break;
        }

        if (!plant_focus_has_wind_projection(plant_def.focus) &&
            plant_def.wind_protection_range != 0U)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Only protection-focused plants may project a non-zero wind-protection range."});
            break;
        }

        if (!plant_focus_has_wind_projection(plant_def.focus) &&
            plant_def.protection_ratio != 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Non-protection plant roles must keep outward protection ratio at zero."});
            break;
        }

        if (plant_focus_has_wind_projection(plant_def.focus) &&
            plant_def.protection_ratio <= 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Protection-focused plants must keep a positive protection ratio."});
            break;
        }

        if (plant_def.harvest_item_id.value == 0U)
        {
            if (plant_def.output_power != 0.0f ||
                plant_def.harvest_quantity != 0U ||
                plant_def.secondary_harvest_item_id.value != 0U ||
                plant_def.secondary_harvest_quantity != 0U ||
                plant_def.harvest_action_duration_minutes != 0.0f ||
                plant_def.harvest_density_required != 0.0f ||
                plant_def.harvest_density_removed != 0.0f)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Plants without harvest output must keep output power and harvest tuning at zero."});
                break;
            }

            continue;
        }

        if (!content.index.item_by_id.contains(plant_def.harvest_item_id.value))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Plant harvest metadata references an unknown item id."});
            break;
        }

        const auto& harvest_item =
            content.item_defs.at(content.index.item_by_id.at(plant_def.harvest_item_id.value));
        const auto harvest_item_cash_points =
            resolved_item_cash_points_for_validation(content, harvest_item);
        if (harvest_item.source_rule != ItemSourceRule::HarvestOnly ||
            (harvest_item.linked_plant_id.value != 0U &&
                harvest_item.linked_plant_id != plant_def.plant_id))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Harvest plants must point at harvest-only items linked back to the same plant or to the shared generic harvest item."});
            break;
        }

        if (plant_def.secondary_harvest_item_id.value != 0U)
        {
            if (plant_def.secondary_harvest_quantity == 0U ||
                !content.index.item_by_id.contains(plant_def.secondary_harvest_item_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Secondary harvest outputs must reference a valid item and positive quantity."});
                break;
            }

            const auto& secondary_harvest_item =
                content.item_defs.at(content.index.item_by_id.at(plant_def.secondary_harvest_item_id.value));
            if (secondary_harvest_item.source_rule != ItemSourceRule::HarvestOnly ||
                (secondary_harvest_item.linked_plant_id.value != 0U &&
                    secondary_harvest_item.linked_plant_id != plant_def.plant_id))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Secondary harvest outputs must point at harvest-only items linked back to the same plant or to the shared generic harvest item."});
                break;
            }
        }
        else if (plant_def.secondary_harvest_quantity != 0U)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Secondary harvest quantities require a matching secondary harvest item id."});
            break;
        }

        if (plant_def.output_power <= 0.0f ||
            plant_def.output_power >
                max_output_power_for_item_value(
                    harvest_item_cash_points * static_cast<std::uint32_t>(plant_def.harvest_quantity)))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Harvest plants must author a positive output meter that stays within the allowed band for the total primary harvest cash-point value."});
            break;
        }

        if (plant_def.harvest_quantity == 0U ||
            plant_def.harvest_action_duration_minutes <= 0.0f ||
            plant_def.harvest_density_required <= 0.0f ||
            plant_def.harvest_density_required > 100.0f ||
            plant_def.harvest_density_removed <= 0.0f ||
            plant_def.harvest_density_removed > 100.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Harvest plants must author valid harvest quantity, timing, and density thresholds."});
            break;
        }
    }

    if (issues.empty())
    {
        const auto expected_pool_targets = build_expected_plant_pool_targets(content);
        for (const auto& plant_def : content.plant_defs)
        {
            const auto it = expected_pool_targets.find(plant_def.plant_id.value);
            if (it == expected_pool_targets.end())
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Every authored plant must be accounted for by either the starter unlock list or the reputation unlock table."});
                break;
            }

            if (std::fabs(plant_total_meter_pool(plant_def) - it->second) > 0.001f)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Plant meter pools must match the authored roster ladder of 90 for starters plus 10 per unlock step."});
                break;
            }
        }
    }

    for (const auto& structure_def : content.structure_defs)
    {
        if (!is_power_of_two(structure_def.footprint_width) ||
            !is_power_of_two(structure_def.footprint_height))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Structure footprints must use power-of-two tile sizes."});
            break;
        }
    }

    for (const auto& item_def : content.item_defs)
    {
        const auto resolved_cash_points =
            resolved_item_cash_points_for_validation(content, item_def);
        if (resolved_cash_points == 0U)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Item definitions must resolve to a positive internal cash-point value through authored fallback pricing or shared player-meter valuation."});
            break;
        }

        if (item_def.linked_plant_id.value != 0U &&
            !content.index.plant_by_id.contains(item_def.linked_plant_id.value))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Item definition references an unknown plant id."});
            break;
        }

        if (item_def.linked_structure_id.value != 0U &&
            !content.index.structure_by_id.contains(item_def.linked_structure_id.value))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Item definition references an unknown structure id."});
            break;
        }

        if (item_def.source_rule == ItemSourceRule::ExcavationOnly)
        {
            if (item_def.consumable ||
                item_def.linked_plant_id.value != 0U ||
                item_def.linked_structure_id.value != 0U ||
                item_def.capability_flags != ITEM_CAPABILITY_SELL)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Excavation-only items must remain non-consumable sell-only merchandise with no linked plant or structure."});
                break;
            }
        }

        if (item_has_player_meter_valuation(item_def) &&
            item_def.internal_price_cash_points != 0U)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Meter-valued item definitions should omit fallback internal cash-point authoring and derive value from the shared player-meter cash-point tuning instead."});
            break;
        }

        if (!item_has_player_meter_valuation(item_def) &&
            item_def.internal_price_cash_points == 0U)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Items without player-meter valuation must author internal_price_cash_points so item buy/sell pricing can derive from a single internal value."});
            break;
        }

        if (item_def.modifier_id.value != 0U)
        {
            const auto* modifier_def = find_content_modifier_def(content, item_def.modifier_id);
            if (modifier_def == nullptr ||
                modifier_def->preset_kind != ModifierPresetKind::RunModifier)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Item definition modifier_id must reference a known run modifier definition."});
                break;
            }
        }
    }

    if (issues.empty())
    {
        bool saw_rough = false;
        bool saw_careful = false;
        bool saw_thorough = false;
        for (const auto& depth_def : content.excavation_depth_defs)
        {
            const float tier_total =
                depth_def.common_tier_percent +
                depth_def.uncommon_tier_percent +
                depth_def.rare_tier_percent +
                depth_def.very_rare_tier_percent +
                depth_def.jackpot_tier_percent;
            if (depth_def.depth == ExcavationDepth::None ||
                depth_def.duration_minutes <= 0.0f ||
                depth_def.energy_cost_multiplier <= 0.0f ||
                depth_def.find_chance_percent < 0.0f ||
                depth_def.find_chance_percent > 100.0f ||
                !percent_total_is_100(tier_total))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Excavation depth definitions must use valid depths, positive durations and energy multipliers, 0-100 find chance, and tier totals that sum to 100%."});
                break;
            }

            saw_rough = saw_rough || depth_def.depth == ExcavationDepth::Rough;
            saw_careful = saw_careful || depth_def.depth == ExcavationDepth::Careful;
            saw_thorough = saw_thorough || depth_def.depth == ExcavationDepth::Thorough;
        }

        if (issues.empty() && (!saw_rough || !saw_careful || !saw_thorough))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Excavation depth definitions must include rough, careful, and thorough rows."});
        }
    }

    if (issues.empty())
    {
        struct DepthTierTotals final
        {
            float common {0.0f};
            float uncommon {0.0f};
            float rare {0.0f};
            float very_rare {0.0f};
            float jackpot {0.0f};
        };

        DepthTierTotals rough_totals {};
        DepthTierTotals careful_totals {};
        DepthTierTotals thorough_totals {};
        for (const auto& entry : content.excavation_loot_entry_defs)
        {
            const auto item_it = content.index.item_by_id.find(entry.item_id.value);
            if (item_it == content.index.item_by_id.end())
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Excavation loot entries must reference known item ids."});
                break;
            }

            const auto& item_def = content.item_defs[item_it->second];
            if (entry.depth == ExcavationDepth::None ||
                item_def.source_rule != ItemSourceRule::ExcavationOnly ||
                entry.tier == ExcavationLootTier::None ||
                entry.percent_within_tier < 0.0f)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Excavation loot entries must reference excavation-only items, use valid depths and tiers, and non-negative tier percentages."});
                break;
            }

            auto* totals = &rough_totals;
            switch (entry.depth)
            {
            case ExcavationDepth::Rough:
                totals = &rough_totals;
                break;
            case ExcavationDepth::Careful:
                totals = &careful_totals;
                break;
            case ExcavationDepth::Thorough:
                totals = &thorough_totals;
                break;
            case ExcavationDepth::None:
            default:
                break;
            }

            switch (entry.tier)
            {
            case ExcavationLootTier::Common:
                totals->common += entry.percent_within_tier;
                break;
            case ExcavationLootTier::Uncommon:
                totals->uncommon += entry.percent_within_tier;
                break;
            case ExcavationLootTier::Rare:
                totals->rare += entry.percent_within_tier;
                break;
            case ExcavationLootTier::VeryRare:
                totals->very_rare += entry.percent_within_tier;
                break;
            case ExcavationLootTier::Jackpot:
                totals->jackpot += entry.percent_within_tier;
                break;
            case ExcavationLootTier::None:
            default:
                break;
            }
        }

        if (issues.empty() &&
            (!percent_total_is_100(rough_totals.common) ||
                !percent_total_is_100(rough_totals.uncommon) ||
                !percent_total_is_100(rough_totals.rare) ||
                !percent_total_is_100(rough_totals.very_rare) ||
                !percent_total_is_100(rough_totals.jackpot) ||
                !percent_total_is_100(careful_totals.common) ||
                !percent_total_is_100(careful_totals.uncommon) ||
                !percent_total_is_100(careful_totals.rare) ||
                !percent_total_is_100(careful_totals.very_rare) ||
                !percent_total_is_100(careful_totals.jackpot) ||
                !percent_total_is_100(thorough_totals.common) ||
                !percent_total_is_100(thorough_totals.uncommon) ||
                !percent_total_is_100(thorough_totals.rare) ||
                !percent_total_is_100(thorough_totals.very_rare) ||
                !percent_total_is_100(thorough_totals.jackpot)))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Each excavation depth+tier loot pool must sum to 100%."});
        }
    }

    for (const auto& recipe_def : content.craft_recipe_defs)
    {
        if (!content.index.structure_by_id.contains(recipe_def.station_structure_id.value))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Craft recipe references an unknown crafting station structure id."});
            break;
        }
        if (!content.index.item_by_id.contains(recipe_def.output_item_id.value))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Craft recipe references an unknown output item id."});
            break;
        }
        if (recipe_def.craft_minutes <= 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Craft recipe definitions must use positive craft duration values."});
            break;
        }
        if (recipe_def.energy_cost < 0.0f ||
            recipe_def.hydration_cost < 0.0f ||
            recipe_def.nourishment_cost < 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Craft recipe definitions cannot use negative worker meter costs."});
            break;
        }
        if (recipe_def.ingredient_count > recipe_def.ingredients.size())
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Craft recipe ingredient count exceeds the supported maximum."});
            break;
        }

        for (std::uint8_t index = 0U; index < recipe_def.ingredient_count; ++index)
        {
            if (!content.index.item_by_id.contains(recipe_def.ingredients[index].item_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Craft recipe references an unknown ingredient item id."});
                break;
            }
        }

        if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
        {
            break;
        }
    }

    if (issues.empty())
    {
        for (const auto& task_template_def : content.task_template_defs)
        {
            if (find_faction_def(task_template_def.publisher_faction_id) == nullptr)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Task template references an unknown publisher faction id."});
                break;
            }

            if (task_template_def.item_id.value != 0U &&
                !content.index.item_by_id.contains(task_template_def.item_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Task template references an unknown item id."});
                break;
            }

            if (task_template_def.recipe_id.value != 0U &&
                !content.index.craft_recipe_by_id.contains(task_template_def.recipe_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Task template references an unknown recipe id."});
                break;
            }

            if (task_template_def.plant_id.value != 0U &&
                !content.index.plant_by_id.contains(task_template_def.plant_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Task template references an unknown plant id."});
                break;
            }

            if (task_template_def.structure_id.value != 0U &&
                !content.index.structure_by_id.contains(task_template_def.structure_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Task template references an unknown primary structure id."});
                break;
            }

            if (task_template_def.secondary_structure_id.value != 0U &&
                !content.index.structure_by_id.contains(task_template_def.secondary_structure_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Task template references an unknown secondary structure id."});
                break;
            }

            if (task_template_def.tertiary_structure_id.value != 0U &&
                !content.index.structure_by_id.contains(task_template_def.tertiary_structure_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Task template references an unknown tertiary structure id."});
                break;
            }

            if (task_template_def.expected_task_hours_in_game < 0.0f)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Task templates cannot use negative expected in-game hours."});
                break;
            }

            if (task_template_def.risk_multiplier < 0.0f)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Task templates cannot use negative risk multipliers."});
                break;
            }

        }
    }

    if (issues.empty())
    {
        for (const auto& seed_def : content.site_onboarding_task_seed_defs)
        {
            if (!content.index.site_by_id.contains(seed_def.site_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site onboarding task seed references an unknown site id."});
                break;
            }

            if (!content.index.task_template_by_id.contains(seed_def.task_template_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site onboarding task seed references an unknown task template id."});
                break;
            }

            if (seed_def.item_id.value != 0U &&
                !content.index.item_by_id.contains(seed_def.item_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site onboarding task seed references an unknown item id."});
                break;
            }

            if (seed_def.plant_id.value != 0U &&
                !content.index.plant_by_id.contains(seed_def.plant_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site onboarding task seed references an unknown plant id."});
                break;
            }

            if (seed_def.recipe_id.value != 0U &&
                !content.index.craft_recipe_by_id.contains(seed_def.recipe_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site onboarding task seed references an unknown recipe id."});
                break;
            }

            if (seed_def.structure_id.value != 0U &&
                !content.index.structure_by_id.contains(seed_def.structure_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site onboarding task seed references an unknown primary structure id."});
                break;
            }

            if (seed_def.secondary_structure_id.value != 0U &&
                !content.index.structure_by_id.contains(seed_def.secondary_structure_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site onboarding task seed references an unknown secondary structure id."});
                break;
            }

            if (seed_def.tertiary_structure_id.value != 0U &&
                !content.index.structure_by_id.contains(seed_def.tertiary_structure_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site onboarding task seed references an unknown tertiary structure id."});
                break;
            }

            if (seed_def.reward_candidate_id.value != 0U &&
                !content.index.reward_candidate_by_id.contains(seed_def.reward_candidate_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site onboarding task seed references an unknown reward candidate id."});
                break;
            }
        }
    }

    if (issues.empty())
    {
        for (const auto& reward_candidate_def : content.reward_candidate_defs)
        {
            if (reward_candidate_def.item_id.value != 0U &&
                !content.index.item_by_id.contains(reward_candidate_def.item_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Reward candidate references an unknown item id."});
                break;
            }

            if (reward_candidate_def.faction_id.value != 0U &&
                find_faction_def(reward_candidate_def.faction_id) == nullptr)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Reward candidate references an unknown faction id."});
                break;
            }
        }
    }

    if (issues.empty())
    {
        for (std::size_t index = 0U; index < content.site_action_defs.size(); ++index)
        {
            const auto& action_def = content.site_action_defs[index];
            if (action_def.action_kind == ActionKind::None)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site action definitions cannot include ActionKind::None."});
                break;
            }

            if (action_def.duration_minutes_per_unit <= 0.0f)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site action definitions must use positive action duration values."});
                break;
            }

            if (action_def.energy_cost_per_unit < 0.0f ||
                action_def.hydration_cost_per_unit < 0.0f ||
                action_def.nourishment_cost_per_unit < 0.0f ||
                action_def.morale_cost_per_unit < 0.0f)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site action definitions cannot use negative worker meter costs."});
                break;
            }

            if (action_def.heat_to_energy_cost < 0.0f ||
                action_def.wind_to_energy_cost < 0.0f ||
                action_def.dust_to_energy_cost < 0.0f ||
                action_def.heat_to_hydration_cost < 0.0f ||
                action_def.wind_to_hydration_cost < 0.0f ||
                action_def.dust_to_hydration_cost < 0.0f ||
                action_def.heat_to_nourishment_cost < 0.0f ||
                action_def.wind_to_nourishment_cost < 0.0f ||
                action_def.dust_to_nourishment_cost < 0.0f ||
                action_def.heat_to_morale_cost < 0.0f ||
                action_def.wind_to_morale_cost < 0.0f ||
                action_def.dust_to_morale_cost < 0.0f)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site action weather-to-meter cost coefficients must be non-negative."});
                break;
            }

            for (std::size_t previous = 0U; previous < index; ++previous)
            {
                if (content.site_action_defs[previous].action_kind == action_def.action_kind)
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Site action definitions must use unique action kinds."});
                    break;
                }
            }

            if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
            {
                break;
            }
        }
    }

    if (issues.empty())
    {
        constexpr ActionKind k_required_action_kinds[] = {
            ActionKind::Plant,
            ActionKind::Build,
            ActionKind::Repair,
            ActionKind::Water,
            ActionKind::ClearBurial,
            ActionKind::Craft,
            ActionKind::Drink,
            ActionKind::Eat,
        };

        for (const auto action_kind : k_required_action_kinds)
        {
            if (!content.index.site_action_by_kind.contains(static_cast<std::uint32_t>(action_kind)))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Site action definitions must include the full required runtime action set."});
                break;
            }
        }
    }

    if (issues.empty())
    {
        bool saw_nearby_aura = false;
        bool saw_run_modifier = false;
        for (const auto& modifier_def : content.modifier_defs)
        {
            if (modifier_def.preset_kind == ModifierPresetKind::NearbyAura)
            {
                saw_nearby_aura = true;
            }
            else if (modifier_def.preset_kind == ModifierPresetKind::RunModifier)
            {
                saw_run_modifier = true;
            }
        }

        if (!saw_nearby_aura || !saw_run_modifier)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Modifier preset content must define at least one nearby-aura preset and one run-modifier preset."});
        }

        for (const auto& modifier_def : content.modifier_defs)
        {
            if (modifier_def.modifier_id.value == 0U)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Modifier definitions must use non-zero modifier ids."});
                break;
            }

            if (modifier_def.preset_kind == ModifierPresetKind::NearbyAura &&
                modifier_def.duration_eight_hour_blocks != 0U)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Nearby-aura modifier definitions must use duration_eight_hour_blocks = 0."});
                break;
            }

            const auto& action_modifiers = modifier_def.action_cost_modifiers;
            if (action_modifiers.hydration_weight_delta <= -1.0f ||
                action_modifiers.nourishment_weight_delta <= -1.0f ||
                action_modifiers.energy_weight_delta <= -1.0f ||
                action_modifiers.morale_weight_delta <= -1.0f)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Modifier action-cost weight deltas must stay above -1 so final costs cannot go negative before bias."});
                break;
            }
        }
    }

    if (issues.empty())
    {
        for (const auto& site : content.prototype_campaign.sites)
        {
            for (const auto modifier_id : site.nearby_aura_modifier_ids)
            {
                const auto* modifier_def = find_content_modifier_def(content, modifier_id);
                if (modifier_def == nullptr ||
                    modifier_def->preset_kind != ModifierPresetKind::NearbyAura)
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Site nearby_aura_modifier_ids must reference known nearby-aura modifier ids."});
                    break;
                }
            }

            if (!issues.empty())
            {
                break;
            }
        }
    }

    if (issues.empty())
    {
        for (const auto& reward_candidate_def : content.reward_candidate_defs)
        {
            const auto* modifier_def = find_content_modifier_def(content, reward_candidate_def.modifier_id);
            if (reward_candidate_def.effect_kind == RewardEffectKind::RunModifier &&
                (modifier_def == nullptr ||
                    modifier_def->preset_kind != ModifierPresetKind::RunModifier))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Run-modifier reward candidates must reference known run modifier ids."});
                break;
            }
        }
    }

    if (issues.empty())
    {
        for (const auto& faction_def : k_prototype_faction_defs)
        {
            const auto* modifier_def =
                find_content_modifier_def(content, ModifierId {faction_def.assistant_package_id});
            if (faction_def.assistant_package_id != 0U &&
                (modifier_def == nullptr ||
                    modifier_def->preset_kind != ModifierPresetKind::RunModifier))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Faction assistant package ids must reference known run modifier ids."});
                break;
            }
        }
    }

    if (issues.empty())
    {
        for (const auto& node_def : content.technology_node_defs)
        {
            const auto* modifier_def = find_content_modifier_def(content, node_def.linked_modifier_id);
            if (node_def.entry_kind == TechnologyEntryKind::GlobalModifier &&
                (modifier_def == nullptr ||
                    modifier_def->preset_kind != ModifierPresetKind::RunModifier))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Global-modifier technology nodes must reference known run modifier ids."});
                break;
            }
        }
    }

    if (issues.empty())
    {
        const auto& tuning = content.gameplay_tuning;
        if (tuning.worker_condition.factor_max < tuning.worker_condition.factor_min)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning worker-condition factor max must be greater than or equal to factor min."});
        }
        else if (tuning.worker_condition.energy_background_increase_real_minutes <= 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning worker-condition energy background increase real-minute speed must stay positive."});
        }
        else if (tuning.worker_condition.energy_background_min_speed_factor < 0.0f ||
            tuning.worker_condition.energy_background_min_speed_factor > 1.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning worker-condition energy background minimum speed factor must stay in the 0-1 range."});
        }
        else if (tuning.worker_condition.morale_background_increase_real_minutes <= 0.0f ||
            tuning.worker_condition.morale_background_decrease_real_minutes <= 0.0f ||
            tuning.worker_condition.morale_support_real_minutes <= 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning worker-condition morale background and support real-minute speeds must stay positive."});
        }
        else if (tuning.worker_condition.sheltered_exposure_scale < 0.0f ||
            tuning.worker_condition.sheltered_exposure_scale > 1.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning worker-condition sheltered exposure scale must stay in the 0-1 range."});
        }
        else if (tuning.player_meter_cash_points.health_per_point <= 0.0f ||
            tuning.player_meter_cash_points.hydration_per_point <= 0.0f ||
            tuning.player_meter_cash_points.nourishment_per_point <= 0.0f ||
            tuning.player_meter_cash_points.energy_per_point <= 0.0f ||
            tuning.player_meter_cash_points.morale_per_point <= 0.0f ||
            tuning.player_meter_cash_points.buy_price_multiplier <= 0.0f ||
            tuning.player_meter_cash_points.sell_price_multiplier <= 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning player-meter internal cash-point values and item buy/sell multipliers must stay positive."});
        }
        else if (tuning.modifier_system.modifier_channel_limit <= 0.0f ||
            tuning.modifier_system.factor_weight_limit <= 0.0f ||
            tuning.modifier_system.factor_bias_limit < 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning modifier limits must stay non-negative and use positive clamp ranges."});
        }
        else if (tuning.device_support.water_evaporation_base < 0.0f ||
            tuning.device_support.heat_evaporation_multiplier < 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning device-support evaporation values must be non-negative."});
        }
        else if (tuning.camp_durability.durability_max <= 0.0f ||
            tuning.camp_durability.peak_event_pressure_total <= 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning camp durability maximums and peak event pressure totals must be positive."});
        }
        else if (tuning.camp_durability.protection_threshold > tuning.camp_durability.durability_max ||
            tuning.camp_durability.delivery_threshold > tuning.camp_durability.protection_threshold ||
            tuning.camp_durability.shared_storage_threshold > tuning.camp_durability.delivery_threshold)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning camp service thresholds must descend from protection to delivery to shared storage within the durability max."});
        }
        else if (tuning.ecology.resistance_density_influence < 0.0f ||
            tuning.ecology.resistance_density_influence > 1.0f ||
            tuning.ecology.salinity_cap_softening < 0.0f ||
            tuning.ecology.salinity_cap_softening > 1.0f ||
            tuning.ecology.salinity_cap_min_unit < 0.0f ||
            tuning.ecology.salinity_cap_min_unit > 1.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning ecology normalized softening and cap values must stay in the 0-1 range."});
        }
        else if (tuning.ecology.density_full_range_real_minutes <= 0.0f)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning ecology density full-range real minutes must stay above zero."});
        }
    }

    if (issues.empty())
    {
        for (const auto plant_id : content.initial_unlocked_plant_ids)
        {
            if (!content.index.plant_by_id.contains(plant_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Initial unlocked plant list references an unknown plant id."});
                break;
            }
        }
    }

    if (issues.empty())
    {
        for (std::size_t index = 0U; index < content.technology_tier_defs.size(); ++index)
        {
            const auto& tier_def = content.technology_tier_defs[index];
            if (tier_def.tier_index == 0U)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Technology tier definitions must use non-zero tier indices."});
                break;
            }

            for (std::size_t previous = 0U; previous < index; ++previous)
            {
                if (content.technology_tier_defs[previous].tier_index == tier_def.tier_index)
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Technology tier definitions must use unique tier indices."});
                    break;
                }
            }

            if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
            {
                break;
            }
        }
    }

    if (issues.empty())
    {
        for (std::size_t index = 0U; index < content.reputation_unlock_defs.size(); ++index)
        {
            const auto& unlock_def = content.reputation_unlock_defs[index];
            if (unlock_def.reputation_requirement < 0)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Reputation unlock definitions must use non-negative reputation requirements."});
                break;
            }

            switch (unlock_def.unlock_kind)
            {
            case ReputationUnlockKind::Plant:
                if (!content.index.plant_by_id.contains(unlock_def.content_id))
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Plant reputation unlocks must reference known plant ids."});
                }
                break;

            case ReputationUnlockKind::Item:
                if (!content.index.item_by_id.contains(unlock_def.content_id))
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Item reputation unlocks must reference known item ids."});
                }
                break;

            case ReputationUnlockKind::StructureRecipe:
            {
                if (!content.index.structure_by_id.contains(unlock_def.content_id))
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Structure-recipe reputation unlocks must reference known structure ids."});
                    break;
                }

                bool found_matching_recipe = false;
                for (const auto& recipe_def : content.craft_recipe_defs)
                {
                    const auto item_it = content.index.item_by_id.find(recipe_def.output_item_id.value);
                    if (item_it != content.index.item_by_id.end() &&
                        content.item_defs[item_it->second].linked_structure_id.value == unlock_def.content_id)
                    {
                        found_matching_recipe = true;
                        break;
                    }
                }

                if (!found_matching_recipe)
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Structure-recipe reputation unlocks must target a structure with at least one matching craft recipe output."});
                }
                break;
            }

            case ReputationUnlockKind::Recipe:
                if (!content.index.craft_recipe_by_id.contains(unlock_def.content_id))
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Recipe reputation unlocks must reference known recipe ids."});
                }
                break;
            }

            if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
            {
                break;
            }

            for (std::size_t previous = 0U; previous < index; ++previous)
            {
                if (content.reputation_unlock_defs[previous].unlock_id == unlock_def.unlock_id)
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Reputation unlock definitions must use unique unlock ids."});
                    break;
                }
            }

            if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
            {
                break;
            }
        }
    }

    if (issues.empty())
    {
        for (std::size_t index = 0U; index < content.technology_node_defs.size(); ++index)
        {
            const auto& node_def = content.technology_node_defs[index];
            bool tier_exists = false;
            for (const auto& tier_def : content.technology_tier_defs)
            {
                if (tier_def.tier_index == node_def.tier_index)
                {
                    tier_exists = true;
                    break;
                }
            }
            if (!tier_exists)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Technology node definitions must reference an existing technology tier."});
                break;
            }

            if (find_faction_def(node_def.faction_id) == nullptr)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Technology node definitions must reference valid faction ids."});
                break;
            }

            if (node_def.reputation_requirement < 0 || node_def.internal_cost_cash_points == 0U)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Technology node definitions must use non-negative reputation requirements and positive internal cash-point values."});
                break;
            }

            if (node_def.reputation_requirement != static_cast<std::int32_t>(node_def.tier_index) ||
                node_def.reputation_requirement <= 0 ||
                node_def.reputation_requirement > k_faction_tech_tier_count)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Technology nodes must form a linear faction-reputation ladder where each tier requires exactly its own tier index inside the 1-32 band."});
                break;
            }

            if (node_def.unlock_effect_parameter < 0.0f ||
                node_def.effect_parameter_per_bonus_reputation < 0.0f)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Technology node definitions must use non-negative unlock and bonus reputation effect parameters."});
                break;
            }

            if (!cash_points_are_cash_aligned(node_def.internal_cost_cash_points))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Technology node internal cash-point values must be divisible by 100 so player-facing cash stays whole."});
                break;
            }

            if (node_def.granted_content_kind == TechnologyGrantedContentKind::None)
            {
                if (node_def.granted_content_id != 0U)
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Technology node definitions without a granted-content kind must leave granted content id at zero."});
                    break;
                }
            }
            else if (node_def.granted_content_kind == TechnologyGrantedContentKind::Item)
            {
                if (!content.index.item_by_id.contains(node_def.granted_content_id))
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Technology node granted item ids must reference known item definitions."});
                    break;
                }
            }
            else if (node_def.granted_content_kind == TechnologyGrantedContentKind::Plant)
            {
                if (!content.index.plant_by_id.contains(node_def.granted_content_id))
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Technology node granted plant ids must reference known plant definitions."});
                    break;
                }
            }
            else if (node_def.granted_content_kind == TechnologyGrantedContentKind::Structure)
            {
                if (!content.index.structure_by_id.contains(node_def.granted_content_id))
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Technology node granted structure ids must reference known structure definitions."});
                    break;
                }
            }
            else if (node_def.granted_content_kind == TechnologyGrantedContentKind::Recipe)
            {
                if (!content.index.craft_recipe_by_id.contains(node_def.granted_content_id))
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Technology node granted recipe ids must reference known recipe definitions."});
                    break;
                }
            }

            for (std::size_t previous = 0U; previous < index; ++previous)
            {
                if (content.technology_node_defs[previous].tech_node_id == node_def.tech_node_id)
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Technology node definitions must use unique technology node ids."});
                    break;
                }

                if (content.technology_node_defs[previous].faction_id == node_def.faction_id &&
                    content.technology_node_defs[previous].tier_index == node_def.tier_index)
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Technology node definitions must use one unique linear tech node per faction and tier."});
                    break;
                }
            }

            if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
            {
                break;
            }
        }
    }

    if (issues.empty())
    {
        if (content.prototype_campaign.starting_campaign_cash < 0)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Prototype campaign starting cash must be non-negative."});
        }
        else if (content.prototype_campaign.support_quota_per_contributor == 0U)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Prototype campaign support quota per contributor must be greater than zero."});
        }
    }

    if (issues.empty())
    {
        for (const auto& loadout_item : content.prototype_campaign.baseline_deployment_items)
        {
            if (!content.index.item_by_id.contains(loadout_item.item_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Prototype campaign baseline loadout references an unknown item id."});
                break;
            }
        }
    }

    for (const auto& site_def : content.prototype_campaign.sites)
    {
        if (find_faction_def(site_def.featured_faction_id) == nullptr)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Prototype site content references an unknown faction id."});
            break;
        }

        for (const auto& other_site_def : content.prototype_campaign.sites)
        {
            if (other_site_def.site_id == site_def.site_id)
            {
                continue;
            }

            if (other_site_def.regional_map_tile.x == site_def.regional_map_tile.x &&
                other_site_def.regional_map_tile.y == site_def.regional_map_tile.y)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Prototype regional-map site tiles must stay unique."});
                break;
            }
        }
        if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
        {
            break;
        }

        for (const auto& support_item : site_def.exported_support_items)
        {
            if (!content.index.item_by_id.contains(support_item.item_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Prototype site content references an unknown support item id."});
                break;
            }
        }
        if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
        {
            break;
        }

        if (site_def.phone_delivery_fee < 0)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Prototype site content uses a negative phone delivery fee."});
            break;
        }

        if (!is_weather_meter_in_range(site_def.default_weather_heat) ||
            !is_weather_meter_in_range(site_def.default_weather_wind) ||
            !is_weather_meter_in_range(site_def.default_weather_dust))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Prototype site default weather values must stay in the 0-100 range."});
            break;
        }

        for (const auto& starting_plant : site_def.starting_plants)
        {
            const auto plant_it = content.index.plant_by_id.find(starting_plant.plant_id.value);
            if (plant_it == content.index.plant_by_id.end())
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Prototype site starting plants must reference known plant ids."});
                break;
            }

            const auto& plant_def = content.plant_defs[plant_it->second];

            if (!is_density_meter_in_range(starting_plant.initial_density))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Prototype site starting plants must use an initial density in the 0-100 range."});
                break;
            }

            if (!is_axis_aligned_to_span(starting_plant.anchor_tile.x, plant_def.footprint_width) ||
                !is_axis_aligned_to_span(starting_plant.anchor_tile.y, plant_def.footprint_height))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Prototype site starting plant anchors must align to the authored plant footprint."});
                break;
            }
        }
        if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
        {
            break;
        }

        for (const auto unlockable_id : site_def.initial_direct_purchase_unlockable_ids)
        {
            bool present_in_revealed = false;
            for (const auto revealed_id : site_def.initial_revealed_unlockable_ids)
            {
                if (revealed_id == unlockable_id)
                {
                    present_in_revealed = true;
                    break;
                }
            }

            if (!present_in_revealed)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Prototype site direct-purchase unlockables must also be in the revealed unlockable list."});
                break;
            }
        }
        if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
        {
            break;
        }

        for (std::size_t index = 0U; index < site_def.seeded_phone_listings.size(); ++index)
        {
            const auto& listing = site_def.seeded_phone_listings[index];
            for (std::size_t previous = 0U; previous < index; ++previous)
            {
                if (site_def.seeded_phone_listings[previous].listing_id == listing.listing_id)
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Prototype site phone listings must use unique listing ids per site."});
                    break;
                }
            }
            if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
            {
                break;
            }

            switch (listing.kind)
            {
            case PhoneListingKind::BuyItem:
                if (!content.index.item_by_id.contains(listing.item_or_unlockable_id))
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Prototype site buy listing references an unknown item id."});
                }
                break;

            case PhoneListingKind::HireContractor:
                if (listing.price <= 0 || listing.quantity == 0U)
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Prototype site contractor listings must have positive price and quantity."});
                }
                break;

            case PhoneListingKind::PurchaseUnlockable:
                if (listing.item_or_unlockable_id == 0U || listing.quantity == 0U ||
                    listing.internal_price_cash_points == 0U ||
                    !cash_points_are_cash_aligned(listing.internal_price_cash_points))
                {
                    issues.push_back(ContentValidationIssue {
                        ContentValidationSeverity::Error,
                        "Prototype site unlockable listings must have a non-zero unlockable id, quantity, and 100-aligned internal cash-point value."});
                }
                break;

            case PhoneListingKind::SellItem:
            default:
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Prototype site seeded phone listings only support buy-item, hire-contractor, and purchase-unlockable entries."});
                break;
            }

            if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
            {
                break;
            }
        }
        if (!issues.empty() && issues.back().severity == ContentValidationSeverity::Error)
        {
            break;
        }
    }

    if (issues.empty())
    {
        for (const auto site_id : content.prototype_campaign.initially_revealed_site_ids)
        {
            if (!content.index.site_by_id.contains(site_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Prototype campaign initial revealed site list references an unknown site id."});
                break;
            }
        }
    }

    if (issues.empty())
    {
        for (const auto site_id : content.prototype_campaign.initially_available_site_ids)
        {
            if (!content.index.site_by_id.contains(site_id.value))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Prototype campaign initial available site list references an unknown site id."});
                break;
            }
        }
    }

    return issues;
}
}  // namespace gs1
