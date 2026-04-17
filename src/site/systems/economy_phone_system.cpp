#include "content/defs/item_defs.h"
#include "site/craft_logic.h"
#include "site/inventory_storage.h"
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
constexpr std::uint16_t k_default_delivery_minutes = 30U;
constexpr std::uint32_t k_site1_water_listing_id = 1U;
constexpr std::uint32_t k_site1_food_listing_id = 2U;
constexpr std::uint32_t k_site1_medicine_listing_id = 3U;
constexpr std::uint32_t k_site1_wind_reed_listing_id = 4U;
constexpr std::uint32_t k_site1_saltbush_listing_id = 5U;
constexpr std::uint32_t k_site1_shade_cactus_listing_id = 6U;
constexpr std::uint32_t k_site1_sunfruit_vine_listing_id = 7U;
constexpr std::uint32_t k_site1_wood_listing_id = 8U;
constexpr std::uint32_t k_site1_iron_listing_id = 9U;
constexpr std::uint32_t k_site1_contractor_listing_id = 10U;
constexpr std::uint32_t k_site1_unlockable_listing_id = 11U;
constexpr std::uint32_t k_site1_unlockable_id = 101U;
constexpr std::uint32_t k_sell_listing_id_base = 1000U;

std::uint32_t normalize_quantity(std::uint16_t value) noexcept
{
    return value == 0U ? 1U : static_cast<std::uint32_t>(value);
}

bool money_delta_fits(std::int64_t amount) noexcept
{
    return amount >= std::numeric_limits<std::int32_t>::min() &&
        amount <= std::numeric_limits<std::int32_t>::max();
}

void mark_phone_dirty(SiteSystemContext<EconomyPhoneSystem>& context) noexcept
{
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_PHONE);
}

void mark_phone_and_hud_dirty(SiteSystemContext<EconomyPhoneSystem>& context) noexcept
{
    context.world.mark_projection_dirty(
        SITE_PROJECTION_UPDATE_PHONE | SITE_PROJECTION_UPDATE_HUD);
}

PhoneListingState make_item_listing(
    PhoneListingKind kind,
    std::uint32_t listing_id,
    ItemId item_id,
    std::uint32_t quantity) noexcept
{
    const auto* item_def = find_item_def(item_id);
    return PhoneListingState {
        kind,
        item_id,
        listing_id,
        item_def == nullptr
            ? 0
            : (kind == PhoneListingKind::SellItem ? item_def->sell_price : item_def->buy_price),
        quantity,
        true};
}

std::uint32_t make_sell_listing_id(ItemId item_id) noexcept
{
    return k_sell_listing_id_base + item_id.value;
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

PhoneListingState* find_sell_listing_for_item(EconomyState& economy, std::uint32_t item_id) noexcept
{
    for (auto& listing : economy.available_phone_listings)
    {
        if (listing.kind == PhoneListingKind::SellItem && listing.item_id.value == item_id)
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

std::vector<std::uint32_t> phone_item_instance_ids(
    SiteSystemContext<EconomyPhoneSystem>& context)
{
    const auto& craft = context.world.read_craft();
    const auto& inventory = context.world.read_inventory();
    if (craft.phone_cache.source_membership_revision == inventory.item_membership_revision)
    {
        return craft.phone_cache.item_instance_ids;
    }

    return inventory_storage::collect_item_instance_ids_in_containers(
        context.site_run,
        inventory_storage::collect_all_storage_containers(context.site_run, true));
}

std::uint32_t available_global_item_quantity(
    SiteSystemContext<EconomyPhoneSystem>& context,
    ItemId item_id)
{
    const auto available_quantity = craft_logic::available_cached_item_quantity(
        context.site_run,
        phone_item_instance_ids(context),
        item_id);
    std::uint32_t reserved_quantity = 0U;
    for (const auto& reserved_stack : context.world.read_action().reserved_input_item_stacks)
    {
        if (reserved_stack.item_id == item_id)
        {
            reserved_quantity += reserved_stack.quantity;
        }
    }

    return available_quantity > reserved_quantity ? available_quantity - reserved_quantity : 0U;
}

std::uint64_t action_reservation_signature(const ActionState& action_state) noexcept
{
    std::uint64_t signature =
        action_state.current_action_id.has_value()
            ? static_cast<std::uint64_t>(action_state.current_action_id->value)
            : 0ULL;
    signature ^= static_cast<std::uint64_t>(action_state.reserved_input_item_stacks.size()) << 32U;
    for (const auto& reserved_stack : action_state.reserved_input_item_stacks)
    {
        signature ^= static_cast<std::uint64_t>(reserved_stack.item_id.value) << 8U;
        signature ^= static_cast<std::uint64_t>(reserved_stack.quantity) << 24U;
        signature ^= static_cast<std::uint64_t>(reserved_stack.container_kind) << 56U;
    }

    return signature;
}

bool same_listing_vector(
    const std::vector<PhoneListingState>& lhs,
    const std::vector<PhoneListingState>& rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        const auto& left = lhs[index];
        const auto& right = rhs[index];
        if (left.kind != right.kind ||
            left.item_id != right.item_id ||
            left.listing_id != right.listing_id ||
            left.price != right.price ||
            left.quantity != right.quantity ||
            left.occupied != right.occupied)
        {
            return false;
        }
    }

    return true;
}

void refresh_dynamic_sell_listings(
    SiteSystemContext<EconomyPhoneSystem>& context,
    bool force_dirty = false)
{
    auto& economy = context.world.own_economy();
    const auto& inventory = context.world.read_inventory();
    const auto reservation_signature = action_reservation_signature(context.world.read_action());
    const bool revisions_unchanged =
        !force_dirty &&
        economy.phone_listing_source_membership_revision == inventory.item_membership_revision &&
        economy.phone_listing_source_quantity_revision == inventory.item_quantity_revision &&
        economy.phone_listing_source_action_reservation_signature == reservation_signature;
    if (revisions_unchanged)
    {
        return;
    }

    std::vector<PhoneListingState> sell_listings {};
    for (const auto& item_def : k_prototype_item_defs)
    {
        if (!item_has_capability(item_def, ITEM_CAPABILITY_SELL))
        {
            continue;
        }

        const auto available_quantity = available_global_item_quantity(context, item_def.item_id);
        if (available_quantity == 0U)
        {
            continue;
        }

        sell_listings.push_back(make_item_listing(
            PhoneListingKind::SellItem,
            make_sell_listing_id(item_def.item_id),
            item_def.item_id,
            available_quantity));
    }

    std::sort(sell_listings.begin(), sell_listings.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.item_id.value < rhs.item_id.value;
    });

    std::vector<PhoneListingState> refreshed {};
    refreshed.reserve(economy.available_phone_listings.size() + sell_listings.size());
    for (const auto& listing : economy.available_phone_listings)
    {
        if (listing.kind != PhoneListingKind::SellItem)
        {
            refreshed.push_back(listing);
        }
    }
    refreshed.insert(refreshed.end(), sell_listings.begin(), sell_listings.end());

    const bool changed = !same_listing_vector(refreshed, economy.available_phone_listings);
    economy.available_phone_listings = std::move(refreshed);
    economy.phone_listing_source_membership_revision = inventory.item_membership_revision;
    economy.phone_listing_source_quantity_revision = inventory.item_quantity_revision;
    economy.phone_listing_source_action_reservation_signature = reservation_signature;
    if (changed || force_dirty)
    {
        mark_phone_dirty(context);
    }
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

    if (listing.item_id.value != 0U)
    {
        GameMessage delivery_message {};
        delivery_message.type = GameMessageType::InventoryDeliveryRequested;
        delivery_message.set_payload(InventoryDeliveryRequestedMessage {
            listing.item_id.value,
            static_cast<std::uint16_t>(quantity),
            k_default_delivery_minutes});
        context.message_queue.push_back(delivery_message);
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

    const auto available = available_global_item_quantity(context, listing.item_id);
    if (available < quantity)
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

    listing.quantity = available - quantity;

    GameMessage consume_message {};
    consume_message.type = GameMessageType::InventoryGlobalItemConsumeRequested;
    consume_message.set_payload(InventoryGlobalItemConsumeRequestedMessage {
        listing.item_id.value,
        static_cast<std::uint16_t>(quantity),
        0U});
    context.message_queue.push_back(consume_message);

    mark_phone_and_hud_dirty(context);
    return GS1_STATUS_OK;
}

Gs1Status process_contractor_hire(
    SiteSystemContext<EconomyPhoneSystem>& context,
    const ContractorHireRequestedMessage& payload)
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
    economy.phone_listing_source_membership_revision = 0U;
    economy.phone_listing_source_quantity_revision = 0U;
    economy.phone_listing_source_action_reservation_signature = 0U;
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
    economy.available_phone_listings.push_back(make_item_listing(
        PhoneListingKind::BuyItem,
        k_site1_water_listing_id,
        ItemId {k_item_water_container},
        6U));
    economy.available_phone_listings.push_back(make_item_listing(
        PhoneListingKind::BuyItem,
        k_site1_food_listing_id,
        ItemId {k_item_food_pack},
        5U));
    economy.available_phone_listings.push_back(make_item_listing(
        PhoneListingKind::BuyItem,
        k_site1_medicine_listing_id,
        ItemId {k_item_medicine_pack},
        4U));
    economy.available_phone_listings.push_back(make_item_listing(
        PhoneListingKind::BuyItem,
        k_site1_wind_reed_listing_id,
        ItemId {k_item_wind_reed_seed_bundle},
        10U));
    economy.available_phone_listings.push_back(make_item_listing(
        PhoneListingKind::BuyItem,
        k_site1_saltbush_listing_id,
        ItemId {k_item_saltbush_seed_bundle},
        8U));
    economy.available_phone_listings.push_back(make_item_listing(
        PhoneListingKind::BuyItem,
        k_site1_shade_cactus_listing_id,
        ItemId {k_item_shade_cactus_seed_bundle},
        8U));
    economy.available_phone_listings.push_back(make_item_listing(
        PhoneListingKind::BuyItem,
        k_site1_sunfruit_vine_listing_id,
        ItemId {k_item_sunfruit_vine_seed_bundle},
        6U));
    economy.available_phone_listings.push_back(make_item_listing(
        PhoneListingKind::BuyItem,
        k_site1_wood_listing_id,
        ItemId {k_item_wood_bundle},
        12U));
    economy.available_phone_listings.push_back(make_item_listing(
        PhoneListingKind::BuyItem,
        k_site1_iron_listing_id,
        ItemId {k_item_iron_bundle},
        10U));
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

    refresh_dynamic_sell_listings(context, true);
    mark_phone_and_hud_dirty(context);
}
}  // namespace

bool EconomyPhoneSystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::SiteRunStarted:
    case GameMessageType::PhoneListingPurchaseRequested:
    case GameMessageType::PhoneListingSaleRequested:
    case GameMessageType::ContractorHireRequested:
    case GameMessageType::SiteUnlockablePurchaseRequested:
        return true;
    default:
        return false;
    }
}

Gs1Status EconomyPhoneSystem::process_message(
    SiteSystemContext<EconomyPhoneSystem>& context,
    const GameMessage& message)
{
    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
    {
        const auto& payload = message.payload_as<SiteRunStartedMessage>();
        seed_site_economy(context, payload.site_id);
        return GS1_STATUS_OK;
    }

    case GameMessageType::PhoneListingPurchaseRequested:
    {
        const auto& payload = message.payload_as<PhoneListingPurchaseRequestedMessage>();
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

    case GameMessageType::PhoneListingSaleRequested:
    {
        const auto& payload = message.payload_as<PhoneListingSaleRequestedMessage>();
        auto* listing = find_listing(context.world.own_economy(), payload.listing_id_or_item_id);
        if (listing == nullptr)
        {
            listing = find_sell_listing_for_item(
                context.world.own_economy(),
                payload.listing_id_or_item_id);
            if (listing == nullptr)
            {
                return GS1_STATUS_NOT_FOUND;
            }
        }

        const std::uint32_t quantity = normalize_quantity(payload.quantity);
        return process_sell_listing(context, *listing, quantity);
    }

    case GameMessageType::ContractorHireRequested:
        return process_contractor_hire(context, message.payload_as<ContractorHireRequestedMessage>());

    case GameMessageType::SiteUnlockablePurchaseRequested:
    {
        const auto& payload = message.payload_as<SiteUnlockablePurchaseRequestedMessage>();
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

void EconomyPhoneSystem::run(SiteSystemContext<EconomyPhoneSystem>& context)
{
    refresh_dynamic_sell_listings(context);
}
}  // namespace gs1
