#include "gs1_godot_regional_site_selection_policy.h"

#include <cassert>
#include <vector>

namespace
{
Gs1RuntimeRegionalMapSiteProjection make_site(std::uint32_t site_id) noexcept
{
    Gs1RuntimeRegionalMapSiteProjection site {};
    site.site_id = site_id;
    return site;
}

void missing_selection_stays_missing()
{
    const std::vector<Gs1RuntimeRegionalMapSiteProjection> sites {
        make_site(1U),
        make_site(2U)};
    assert(gs1_godot_find_explicitly_selected_regional_site(sites, std::nullopt) == nullptr);
}

void explicit_selection_resolves_matching_site()
{
    const std::vector<Gs1RuntimeRegionalMapSiteProjection> sites {
        make_site(1U),
        make_site(2U)};
    const auto* selected = gs1_godot_find_explicitly_selected_regional_site(sites, 2U);
    assert(selected != nullptr);
    assert(selected->site_id == 2U);
}

void unknown_selection_does_not_fallback_to_first_site()
{
    const std::vector<Gs1RuntimeRegionalMapSiteProjection> sites {
        make_site(1U),
        make_site(2U)};
    assert(gs1_godot_find_explicitly_selected_regional_site(sites, 99U) == nullptr);
}
}

int main()
{
    missing_selection_stays_missing();
    explicit_selection_resolves_matching_site();
    unknown_selection_does_not_fallback_to_first_site();
    return 0;
}
