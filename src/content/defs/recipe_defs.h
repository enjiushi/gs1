#pragma once

#include "support/id_types.h"

namespace gs1
{
struct RecipeDef final
{
    RecipeId recipe_id {};
    std::uint32_t output_quantity {0};
};
}  // namespace gs1
