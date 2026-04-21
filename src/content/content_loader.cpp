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
    content.prototype_campaign = &get_prototype_campaign_content();

    const auto root = content_table_root();
    const auto items_path = root / "items.tsv";
    const auto plants_path = root / "plants.tsv";
    const auto structures_path = root / "structures.tsv";
    const auto craft_recipes_path = root / "craft_recipes.tsv";

    load_item_defs(content, items_path);
    load_plant_defs(content, plants_path);
    load_structure_defs(content, structures_path);
    load_craft_recipe_defs(content, craft_recipes_path);

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
