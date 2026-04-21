#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"
#include "content/content_loader.h"

namespace gs1
{
std::span<const ItemDef> all_item_defs() noexcept
{
    return prototype_content_database().item_defs;
}

const ItemDef* find_item_def(ItemId item_id) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.item_by_id.find(item_id.value);
    return it == content.index.item_by_id.end() ? nullptr : &content.item_defs[it->second];
}

std::uint32_t item_stack_size(ItemId item_id) noexcept
{
    const auto* item_def = find_item_def(item_id);
    return item_def == nullptr ? 1U : static_cast<std::uint32_t>(item_def->stack_size);
}

std::span<const PlantDef> all_plant_defs() noexcept
{
    return prototype_content_database().plant_defs;
}

const PlantDef* find_plant_def(PlantId plant_id) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.plant_by_id.find(plant_id.value);
    return it == content.index.plant_by_id.end() ? nullptr : &content.plant_defs[it->second];
}

std::span<const StructureDef> all_structure_defs() noexcept
{
    return prototype_content_database().structure_defs;
}

const StructureDef* find_structure_def(StructureId structure_id) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.structure_by_id.find(structure_id.value);
    return it == content.index.structure_by_id.end() ? nullptr : &content.structure_defs[it->second];
}

bool structure_has_storage(StructureId structure_id) noexcept
{
    const auto* structure_def = find_structure_def(structure_id);
    return structure_def != nullptr && structure_def->grants_storage && structure_def->storage_slot_count > 0U;
}

bool structure_is_crafting_station(StructureId structure_id) noexcept
{
    const auto* structure_def = find_structure_def(structure_id);
    return structure_def != nullptr &&
        structure_def->crafting_station_kind != CraftingStationKind::None;
}

std::span<const CraftRecipeDef> all_craft_recipe_defs() noexcept
{
    return prototype_content_database().craft_recipe_defs;
}

const CraftRecipeDef* find_craft_recipe_def(
    StructureId station_structure_id,
    ItemId output_item_id) noexcept
{
    for (const auto& recipe_def : all_craft_recipe_defs())
    {
        if (recipe_def.station_structure_id == station_structure_id &&
            recipe_def.output_item_id == output_item_id)
        {
            return &recipe_def;
        }
    }

    return nullptr;
}

const CraftRecipeDef* find_craft_recipe_def(RecipeId recipe_id) noexcept
{
    const auto& content = prototype_content_database();
    const auto it = content.index.craft_recipe_by_id.find(recipe_id.value);
    return it == content.index.craft_recipe_by_id.end()
        ? nullptr
        : &content.craft_recipe_defs[it->second];
}
}  // namespace gs1
