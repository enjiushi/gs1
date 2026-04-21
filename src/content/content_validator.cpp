#include "content/content_validator.h"

namespace gs1
{
namespace
{
[[nodiscard]] bool is_power_of_two(std::uint8_t value) noexcept
{
    return value != 0U && (value & static_cast<std::uint8_t>(value - 1U)) == 0U;
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

    return issues;
}
}  // namespace gs1
