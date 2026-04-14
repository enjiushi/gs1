#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "site/systems/economy_phone_system.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace gs1
{
namespace
{
constexpr std::int32_t k_site1_initial_money = 45;
constexpr std::uint32_t k_site1_buy_listing_id = 1U;
constexpr std::uint32_t k_site1_sale_listing_id = 2U;
constexpr std::uint32_t k_site1_contractor_listing_id = 3U;
constexpr std::uint32_t k_site1_unlockable_listing_id = 4U;
constexpr std::uint32_t k_site1_unlockable_id = 101U;

std::uint32_t normalize_quantity(std::uint16_t value) noexcept
{
    return value == 0U ? 1U : static_cast<std::uint32_t>(value);
}

bool money_delta_fits(std::int64_t amount) noexcept
{
    return amount >= std::numeric_limits<std::int32_t>::min() &&
        amount <= std::numeric_limits<std::int32_t>::max();
}

void mark_phone_and_hud_dirty(SiteSystemContext<EconomyPhoneSystem>& context) noexcept
{
    context.world.mark_projection_dirty(
        SITE_PROJECTION_UPDATE_PHONE | SITE_PROJECTION_UPDATE_HUD);
}

bool adjust_money(SiteSystemContext<EconomyPhoneSystem>& context, std::int32_t delta) noexcept
{
    auto& economy = context.world.own_economy();
    const auto current = static_cast<std::int64_t>(economy.money);
    const auto updated = current + delta;
    if (updated < 0 || updated > std::numeric_limits<std::int32_t>::max())
    {
        return false;
    }

    economy.money = static_cast<std::int32_t>(updated);
    return true;
}

PhoneListingState* find_listing(EconomyState& economy, std::uint32_t listing_id) noexcept
{
    for (auto& listing : economy.available_phone_listings)
    {
        if (listing.listing_id == listing_id)
        {
            return &listing;
        }
    }
    return nullptr;
}

PhoneListingState* find_unlockable_listing(EconomyState& economy, std::uint32_t unlockable_id) noexcept
{
    for (auto& listing : economy.available_phone_listings)
    {
        if (listing.kind == PhoneListingKind::PurchaseUnlockable &&
            listing.item_id.value == unlockable_id)
        {
            return &listing;
        }
    }
    return nullptr;
}

bool contains_unlockable(
    const std::vector<std::uint32_t>& container,
    std::uint32_t unlockable_id) noexcept
{
    return std::find(container.begin(), container.end(), unlockable_id) != container.end();
}

void remove_unlockable(std::vector<std::uint32_t>& container, std::uint32_t unlockable_id) noexcept
{
    container.erase(std::remove(container.begin(), container.end(), unlockable_id), container.end());
}

Gs1Status process_buy_listing(
    SiteSystemContext<EconomyPhoneSystem>& context,
    PhoneListingState& listing,
    std::uint32_t quantity)
{
    if (listing.kind != PhoneListingKind::BuyItem)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const bool limited = listing.quantity != 0U;
    const std::uint32_t available = listing.quantity;
    if (limited && available < quantity)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const auto total_cost = static_cast<std::int64_t>(listing.price) * quantity;
    if (total_cost < 0 || !money_delta_fits(-total_cost))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (!adjust_money(context, static_cast<std::int32_t>(-total_cost)))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (limited)
    {
        listing.quantity = available - quantity;
    }

    mark_phone_and_hud_dirty(context);
    return GS1_STATUS_OK;
}

Gs1Status process_sell_listing(
    SiteSystemContext<EconomyPhoneSystem>& context,
    PhoneListingState& listing,
    std::uint32_t quantity)
{
    if (listing.kind != PhoneListingKind::SellItem)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const bool limited = listing.quantity != 0U;
    const std::uint32_t available = listing.quantity;
    if (limited && available < quantity)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const auto total_gain = static_cast<std::int64_t>(listing.price) * quantity;
    if (total_gain < 0 || !money_delta_fits(total_gain))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (!adjust_money(context, static_cast<std::int32_t>(total_gain)))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (limited)
    {
        listing.quantity = available - quantity;
    }

    mark_phone_and_hud_dirty(context);
    return GS1_STATUS_OK;
}

Gs1Status process_contractor_hire(
    SiteSystemContext<EconomyPhoneSystem>& context,
    const ContractorHireRequestedCommand& payload)
{
    auto* listing = find_listing(context.world.own_economy(), payload.listing_or_offer_id);
    if (listing == nullptr)
    {
        return GS1_STATUS_NOT_FOUND;
    }
    if (listing->kind != PhoneListingKind::HireContractor)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const std::uint32_t work_units = payload.requested_work_units == 0U ? 1U : payload.requested_work_units;
    const bool limited = listing->quantity != 0U;
    const std::uint32_t available = listing->quantity;
    if (limited && available < work_units)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const auto total_cost = static_cast<std::int64_t>(listing->price) * work_units;
    if (total_cost < 0 || !money_delta_fits(-total_cost))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (!adjust_money(context, static_cast<std::int32_t>(-total_cost)))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (limited)
    {
        listing->quantity = available - work_units;
    }

    mark_phone_and_hud_dirty(context);
    return GS1_STATUS_OK;
}

Gs1Status process_unlockable_purchase(
    SiteSystemContext<EconomyPhoneSystem>& context,
    std::uint32_t unlockable_id,
    std::int32_t price)
{
    auto& economy = context.world.own_economy();
    if (price < 0 || !money_delta_fits(-static_cast<std::int64_t>(price)))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (!contains_unlockable(economy.direct_purchase_unlockable_ids, unlockable_id))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (!adjust_money(context, static_cast<std::int32_t>(-price)))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    remove_unlockable(economy.direct_purchase_unlockable_ids, unlockable_id);
    if (!contains_unlockable(economy.revealed_site_unlockable_ids, unlockable_id))
    {
        economy.revealed_site_unlockable_ids.push_back(unlockable_id);
    }

    mark_phone_and_hud_dirty(context);
    return GS1_STATUS_OK;
}

void seed_site_economy(SiteSystemContext<EconomyPhoneSystem>& context, std::uint32_t site_id)
{
    auto& economy = context.world.own_economy();
    economy.available_phone_listings.clear();
    economy.revealed_site_unlockable_ids.clear();
    economy.direct_purchase_unlockable_ids.clear();

    if (site_id != 1U)
    {
        economy.money = 0;
        mark_phone_and_hud_dirty(context);
        return;
    }

    economy.money = k_site1_initial_money;
    economy.available_phone_listings.push_back(PhoneListingState {
        PhoneListingKind::BuyItem,
        ItemId {1U},
        k_site1_buy_listing_id,
        5,
        5U,
        true});
    economy.available_phone_listings.push_back(PhoneListingState {
        PhoneListingKind::SellItem,
        ItemId {2U},
        k_site1_sale_listing_id,
        2,
        10U,
        true});
    economy.available_phone_listings.push_back(PhoneListingState {
        PhoneListingKind::HireContractor,
        ItemId {0U},
        k_site1_contractor_listing_id,
        8,
        3U,
        true});
    economy.available_phone_listings.push_back(PhoneListingState {
        PhoneListingKind::PurchaseUnlockable,
        ItemId {k_site1_unlockable_id},
        k_site1_unlockable_listing_id,
        20,
        1U,
        true});

    economy.revealed_site_unlockable_ids.push_back(k_site1_unlockable_id);
    economy.direct_purchase_unlockable_ids.push_back(k_site1_unlockable_id);

    mark_phone_and_hud_dirty(context);
}
}  // namespace

bool EconomyPhoneSystem::subscribes_to(GameCommandType type) noexcept
{
    switch (type)
    {
    case GameCommandType::SiteRunStarted:
    case GameCommandType::PhoneListingPurchaseRequested:
    case GameCommandType::PhoneListingSaleRequested:
    case GameCommandType::ContractorHireRequested:
    case GameCommandType::SiteUnlockablePurchaseRequested:
        return true;
    default:
        return false;
    }
}

Gs1Status EconomyPhoneSystem::process_command(
    SiteSystemContext<EconomyPhoneSystem>& context,
    const GameCommand& command)
{
    switch (command.type)
    {
    case GameCommandType::SiteRunStarted:
    {
        const auto& payload = command.payload_as<SiteRunStartedCommand>();
        seed_site_economy(context, payload.site_id);
        return GS1_STATUS_OK;
    }

    case GameCommandType::PhoneListingPurchaseRequested:
    {
        const auto& payload = command.payload_as<PhoneListingPurchaseRequestedCommand>();
        auto* listing = find_listing(context.world.own_economy(), payload.listing_id);
        if (listing == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        const std::uint32_t quantity = normalize_quantity(payload.quantity);
        switch (listing->kind)
        {
        case PhoneListingKind::BuyItem:
            return process_buy_listing(context, *listing, quantity);
        case PhoneListingKind::PurchaseUnlockable:
            return process_unlockable_purchase(context, listing->item_id.value, listing->price);
        default:
            return GS1_STATUS_INVALID_STATE;
        }
    }

    case GameCommandType::PhoneListingSaleRequested:
    {
        const auto& payload = command.payload_as<PhoneListingSaleRequestedCommand>();
        auto* listing = find_listing(context.world.own_economy(), payload.listing_id_or_item_id);
        if (listing == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        const std::uint32_t quantity = normalize_quantity(payload.quantity);
        return process_sell_listing(context, *listing, quantity);
    }

    case GameCommandType::ContractorHireRequested:
        return process_contractor_hire(context, command.payload_as<ContractorHireRequestedCommand>());

    case GameCommandType::SiteUnlockablePurchaseRequested:
    {
        const auto& payload = command.payload_as<SiteUnlockablePurchaseRequestedCommand>();
        const auto* listing = find_unlockable_listing(context.world.own_economy(), payload.unlockable_id);
        if (listing == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        return process_unlockable_purchase(context, payload.unlockable_id, listing->price);
    }

    default:
        return GS1_STATUS_OK;
    }
}

void EconomyPhoneSystem::run(SiteSystemContext<EconomyPhoneSystem>&)
{
}
}  // namespace gs1
