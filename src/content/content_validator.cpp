#include "content/content_validator.h"

#include "content/defs/faction_defs.h"
#include "support/currency.h"

namespace gs1
{
namespace
{
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

[[nodiscard]] bool modifier_preset_indices_are_unique_and_contiguous(
    std::span<const ModifierPresetDef> presets) noexcept
{
    for (std::size_t index = 0U; index < presets.size(); ++index)
    {
        if (presets[index].preset_index != index)
        {
            return false;
        }
    }

    return true;
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

        if (plant_def.harvest_item_id.value == 0U)
        {
            if (plant_def.harvest_quantity != 0U ||
                plant_def.harvest_action_duration_minutes != 0.0f ||
                plant_def.harvest_density_required != 0.0f ||
                plant_def.harvest_density_removed != 0.0f)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Plants without harvest output must keep harvest tuning at zero."});
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
        if (harvest_item.source_rule != ItemSourceRule::HarvestOnly ||
            (harvest_item.linked_plant_id.value != 0U &&
                harvest_item.linked_plant_id != plant_def.plant_id))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Harvest plants must point at harvest-only items linked back to the same plant or to the shared generic harvest item."});
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
        if (item_def.internal_price_cash_points == 0U)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Item definitions must author a positive internal cash-point value."});
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
        if (content.nearby_aura_modifier_presets.empty() ||
            content.run_modifier_presets.empty())
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Modifier preset content must define at least one nearby-aura preset and one run-modifier preset."});
        }
        else if (!modifier_preset_indices_are_unique_and_contiguous(content.nearby_aura_modifier_presets) ||
            !modifier_preset_indices_are_unique_and_contiguous(content.run_modifier_presets))
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Modifier preset indices must be unique and contiguous starting at zero within each preset kind."});
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
        else if (tuning.ecology.density_growth_pressure_safe_threshold < 0.0f ||
            tuning.ecology.density_growth_pressure_safe_threshold > 1.0f ||
            tuning.ecology.density_loss_pressure_threshold < 0.0f ||
            tuning.ecology.density_loss_pressure_threshold > 1.0f ||
            tuning.ecology.density_growth_pressure_safe_threshold >
                tuning.ecology.density_loss_pressure_threshold)
        {
            issues.push_back(ContentValidationIssue {
                ContentValidationSeverity::Error,
                "Gameplay tuning ecology pressure thresholds must stay in the 0-1 range and keep the safe threshold below the loss threshold."});
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

            if (unlock_def.unlock_kind == ReputationUnlockKind::Plant &&
                !content.index.plant_by_id.contains(unlock_def.content_id))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Reputation unlock definitions reference an unknown plant id."});
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

            if (node_def.reputation_cost < 0 || node_def.internal_cost_cash_points == 0U)
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Technology node definitions must use non-negative reputation cost and positive internal cash-point value."});
                break;
            }

            if (!cash_points_are_cash_aligned(node_def.internal_cost_cash_points))
            {
                issues.push_back(ContentValidationIssue {
                    ContentValidationSeverity::Error,
                    "Technology node internal cash-point values must be divisible by 100 so player-facing cash stays whole."});
                break;
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
                        "Technology node definitions must use only one node per faction and tier."});
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
