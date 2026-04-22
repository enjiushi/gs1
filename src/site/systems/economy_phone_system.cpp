#include "campaign/systems/technology_system.h"
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
constexpr std::int32_t k_phone_delivery_fee = 5;
constexpr std::uint16_t k_immediate_delivery_minutes = 0U;
constexpr std::uint32_t k_site1_water_listing_id = 1U;
constexpr std::uint32_t k_site1_food_listing_id = 2U;
constexpr std::uint32_t k_site1_medicine_listing_id = 3U;
constexpr std::uint32_t k_site1_wind_reed_listing_id = 4U;
constexpr std::uint32_t k_site1_salt_bean_listing_id = 5U;
constexpr std::uint32_t k_site1_shade_cactus_listing_id = 6U;
constexpr std::uint32_t k_site1_sunfruit_vine_listing_id = 7U;
constexpr std::uint32_t k_site1_wood_listing_id = 8U;
constexpr std::uint32_t k_site1_iron_listing_id = 9U;
constexpr std::uint32_t k_site1_contractor_listing_id = 10U;
constexpr std::uint32_t k_site1_unlockable_listing_id = 11U;
constexpr std::uint32_t k_site1_root_binder_listing_id = 12U;
constexpr std::uint32_t k_site1_dew_grass_listing_id = 13U;
constexpr std::uint32_t k_site1_thorn_shrub_listing_id = 14U;
constexpr std::uint32_t k_site1_medicinal_sage_listing_id = 15U;
constexpr std::uint32_t k_site1_sand_willow_listing_id = 16U;
constexpr std::uint32_t k_site1_basic_straw_checkerboard_listing_id = 17U;
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

void sync_projected_money(SiteSystemContext<EconomyPhoneSystem>& context) noexcept
{
    context.world.own_economy().money = std::max(0, context.campaign.cash);
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
        0U,
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

void queue_campaign_cash_delta_message(
    SiteSystemContext<EconomyPhoneSystem>& context,
    std::int32_t delta) noexcept
{
    if (delta == 0)
    {
        return;
    }

    GameMessage cash_message {};
    cash_message.type = GameMessageType::CampaignCashDeltaRequested;
    cash_message.set_payload(CampaignCashDeltaRequestedMessage {delta});
    context.message_queue.push_back(cash_message);
}

void clamp_cart_quantity(PhoneListingState& listing) noexcept
{
    if (listing.kind != PhoneListingKind::BuyItem)
    {
        listing.cart_quantity = 0U;
        return;
    }

    if (listing.quantity != 0U)
    {
        listing.cart_quantity = std::min(listing.cart_quantity, listing.quantity);
    }
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
            left.cart_quantity != right.cart_quantity ||
            left.occupied != right.occupied)
        {
            return false;
        }
    }

    return true;
}

std::uint32_t cart_item_quantity(const EconomyState& economy) noexcept
{
    std::uint32_t total = 0U;
    for (const auto& listing : economy.available_phone_listings)
    {
        if (listing.kind == PhoneListingKind::BuyItem)
        {
            total += listing.cart_quantity;
        }
    }

    return total;
}

bool queue_delivery_batch_message(
    SiteSystemContext<EconomyPhoneSystem>& context,
    const InventoryDeliveryBatchEntry* entries,
    std::uint8_t entry_count) noexcept
{
    if (entry_count == 0U || entry_count > k_inventory_delivery_batch_entry_count)
    {
        return false;
    }

    GameMessage delivery_message {};
    delivery_message.type = GameMessageType::InventoryDeliveryBatchRequested;
    auto& payload = delivery_message.emplace_payload<InventoryDeliveryBatchRequestedMessage>();
    payload.minutes_until_arrival = k_immediate_delivery_minutes;
    payload.entry_count = entry_count;
    payload.reserved0 = 0U;
    for (std::size_t index = 0; index < k_inventory_delivery_batch_entry_count; ++index)
    {
        payload.entries[index] = InventoryDeliveryBatchEntry {};
    }
    for (std::uint8_t index = 0U; index < entry_count; ++index)
    {
        payload.entries[index] = entries[index];
    }
    context.message_queue.push_back(delivery_message);
    return true;
}

bool queue_single_delivery_message(
    SiteSystemContext<EconomyPhoneSystem>& context,
    ItemId item_id,
    std::uint32_t quantity) noexcept
{
    if (item_id.value == 0U || quantity == 0U ||
        item_id.value > static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max()) ||
        quantity > static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max()))
    {
        return false;
    }

    InventoryDeliveryBatchEntry entry {};
    entry.item_id = static_cast<std::uint16_t>(item_id.value);
    entry.quantity = static_cast<std::uint16_t>(quantity);
    return queue_delivery_batch_message(context, &entry, 1U);
}

void queue_purchase_completed_message(
    SiteSystemContext<EconomyPhoneSystem>& context,
    const PhoneListingState& listing,
    std::uint32_t quantity) noexcept
{
    if (listing.item_id.value == 0U || quantity == 0U)
    {
        return;
    }

    GameMessage message {};
    message.type = GameMessageType::PhoneListingPurchased;
    message.set_payload(PhoneListingPurchasedMessage {
        listing.listing_id,
        listing.item_id.value,
        static_cast<std::uint16_t>(std::min<std::uint32_t>(quantity, 65535U)),
        0U});
    context.message_queue.push_back(message);
}

void queue_sale_completed_message(
    SiteSystemContext<EconomyPhoneSystem>& context,
    const PhoneListingState& listing,
    std::uint32_t quantity) noexcept
{
    if (listing.item_id.value == 0U || quantity == 0U)
    {
        return;
    }

    GameMessage message {};
    message.type = GameMessageType::PhoneListingSold;
    message.set_payload(PhoneListingSoldMessage {
        listing.listing_id,
        listing.item_id.value,
        static_cast<std::uint16_t>(std::min<std::uint32_t>(quantity, 65535U)),
        0U});
    context.message_queue.push_back(message);
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
    for (const auto& item_def : all_item_defs())
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
            auto preserved_listing = listing;
            clamp_cart_quantity(preserved_listing);
            refreshed.push_back(preserved_listing);
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

    const auto total_cost =
        static_cast<std::int64_t>(listing.price) * quantity + static_cast<std::int64_t>(k_phone_delivery_fee);
    if (total_cost < 0 || !money_delta_fits(-total_cost))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (!adjust_money(context, static_cast<std::int32_t>(-total_cost)))
    {
        return GS1_STATUS_INVALID_STATE;
    }
    queue_campaign_cash_delta_message(context, static_cast<std::int32_t>(-total_cost));

    if (limited)
    {
        listing.quantity = available - quantity;
    }
    clamp_cart_quantity(listing);

    if (listing.item_id.value != 0U &&
        !queue_single_delivery_message(context, listing.item_id, quantity))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    queue_purchase_completed_message(context, listing, quantity);

    mark_phone_and_hud_dirty(context);
    return GS1_STATUS_OK;
}

Gs1Status process_cart_add(
    SiteSystemContext<EconomyPhoneSystem>& context,
    PhoneListingState& listing,
    std::uint32_t quantity)
{
    if (listing.kind != PhoneListingKind::BuyItem)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (listing.quantity != 0U && listing.cart_quantity >= listing.quantity)
    {
        return GS1_STATUS_OK;
    }

    const auto remaining_capacity =
        listing.quantity == 0U
            ? quantity
            : (listing.quantity > listing.cart_quantity ? (listing.quantity - listing.cart_quantity) : 0U);
    const auto quantity_to_add = listing.quantity == 0U ? quantity : std::min(quantity, remaining_capacity);
    if (quantity_to_add == 0U)
    {
        return GS1_STATUS_OK;
    }

    listing.cart_quantity += quantity_to_add;
    mark_phone_dirty(context);
    return GS1_STATUS_OK;
}

Gs1Status process_cart_remove(
    SiteSystemContext<EconomyPhoneSystem>& context,
    PhoneListingState& listing,
    std::uint32_t quantity)
{
    if (listing.kind != PhoneListingKind::BuyItem)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (listing.cart_quantity == 0U)
    {
        return GS1_STATUS_OK;
    }

    listing.cart_quantity =
        listing.cart_quantity > quantity ? (listing.cart_quantity - quantity) : 0U;
    mark_phone_dirty(context);
    return GS1_STATUS_OK;
}

Gs1Status process_cart_checkout(SiteSystemContext<EconomyPhoneSystem>& context)
{
    auto& economy = context.world.own_economy();
    if (cart_item_quantity(economy) == 0U)
    {
        return GS1_STATUS_INVALID_STATE;
    }

    InventoryDeliveryBatchEntry entries[k_inventory_delivery_batch_entry_count] {};
    std::uint8_t entry_count = 0U;
    std::int64_t subtotal = 0;
    for (auto& listing : economy.available_phone_listings)
    {
        clamp_cart_quantity(listing);
        if (listing.kind != PhoneListingKind::BuyItem || listing.cart_quantity == 0U)
        {
            continue;
        }

        if (listing.quantity != 0U && listing.quantity < listing.cart_quantity)
        {
            return GS1_STATUS_INVALID_STATE;
        }

        subtotal += static_cast<std::int64_t>(listing.price) * listing.cart_quantity;
        if (listing.item_id.value != 0U)
        {
            if (entry_count >= k_inventory_delivery_batch_entry_count ||
                listing.item_id.value > static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max()) ||
                listing.cart_quantity > static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max()))
            {
                return GS1_STATUS_INVALID_STATE;
            }

            entries[entry_count].item_id = static_cast<std::uint16_t>(listing.item_id.value);
            entries[entry_count].quantity = static_cast<std::uint16_t>(listing.cart_quantity);
            entry_count += 1U;
        }
    }

    const auto total_cost = subtotal + static_cast<std::int64_t>(k_phone_delivery_fee);
    if (subtotal <= 0 || total_cost < 0 || !money_delta_fits(-total_cost))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (!adjust_money(context, static_cast<std::int32_t>(-total_cost)))
    {
        return GS1_STATUS_INVALID_STATE;
    }
    queue_campaign_cash_delta_message(context, static_cast<std::int32_t>(-total_cost));

    for (auto& listing : economy.available_phone_listings)
    {
        if (listing.kind != PhoneListingKind::BuyItem || listing.cart_quantity == 0U)
        {
            continue;
        }

        if (listing.quantity != 0U)
        {
            listing.quantity -= listing.cart_quantity;
        }
        listing.cart_quantity = 0U;
    }

        if (entry_count > 0U && !queue_delivery_batch_message(context, entries, entry_count))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    for (const auto& entry : entries)
    {
        if (entry.item_id == 0U || entry.quantity == 0U)
        {
            continue;
        }

        PhoneListingState synthetic_listing {};
        synthetic_listing.listing_id = k_site1_water_listing_id;
        synthetic_listing.item_id = ItemId {entry.item_id};
        for (const auto& listing : economy.available_phone_listings)
        {
            if (listing.kind == PhoneListingKind::BuyItem &&
                listing.item_id.value == entry.item_id)
            {
                synthetic_listing.listing_id = listing.listing_id;
                break;
            }
        }
        queue_purchase_completed_message(context, synthetic_listing, entry.quantity);
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

    const auto inventory_available = available_global_item_quantity(context, listing.item_id);
    const auto effective_available = std::min(listing.quantity, inventory_available);
    const auto quantity_to_sell = std::min(quantity, effective_available);
    if (quantity_to_sell == 0U)
    {
        refresh_dynamic_sell_listings(context, true);
        return GS1_STATUS_OK;
    }

    const auto total_gain = static_cast<std::int64_t>(listing.price) * quantity_to_sell;
    if (total_gain < 0 || !money_delta_fits(total_gain))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (!adjust_money(context, static_cast<std::int32_t>(total_gain)))
    {
        return GS1_STATUS_INVALID_STATE;
    }
    queue_campaign_cash_delta_message(context, static_cast<std::int32_t>(total_gain));

    listing.quantity = effective_available - quantity_to_sell;

    GameMessage consume_message {};
    consume_message.type = GameMessageType::InventoryGlobalItemConsumeRequested;
    consume_message.set_payload(InventoryGlobalItemConsumeRequestedMessage {
        listing.item_id.value,
        static_cast<std::uint16_t>(quantity_to_sell),
        0U});
    context.message_queue.push_back(consume_message);
    queue_sale_completed_message(context, listing, quantity_to_sell);

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
    queue_campaign_cash_delta_message(context, static_cast<std::int32_t>(-total_cost));

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
    queue_campaign_cash_delta_message(context, static_cast<std::int32_t>(-price));

    remove_unlockable(economy.direct_purchase_unlockable_ids, unlockable_id);
    if (!contains_unlockable(economy.revealed_site_unlockable_ids, unlockable_id))
    {
        economy.revealed_site_unlockable_ids.push_back(unlockable_id);
    }

    mark_phone_and_hud_dirty(context);
    return GS1_STATUS_OK;
}

void reveal_unlockable_for_site(
    SiteSystemContext<EconomyPhoneSystem>& context,
    std::uint32_t unlockable_id)
{
    if (unlockable_id == 0U)
    {
        return;
    }

    auto& economy = context.world.own_economy();
    if (!contains_unlockable(economy.revealed_site_unlockable_ids, unlockable_id))
    {
        economy.revealed_site_unlockable_ids.push_back(unlockable_id);
    }

    mark_phone_and_hud_dirty(context);
}

void append_site_one_seed_listing(
    SiteSystemContext<EconomyPhoneSystem>& context,
    std::uint32_t listing_id,
    std::uint32_t item_id,
    std::uint32_t quantity)
{
    const auto* item_def = find_item_def(ItemId {item_id});
    if (item_def == nullptr)
    {
        return;
    }

    if (item_def->linked_plant_id.value != 0U &&
        !TechnologySystem::plant_unlocked(context.campaign, item_def->linked_plant_id))
    {
        return;
    }

    context.world.own_economy().available_phone_listings.push_back(make_item_listing(
        PhoneListingKind::BuyItem,
        listing_id,
        ItemId {item_id},
        quantity));
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
    sync_projected_money(context);

    if (site_id != 1U)
    {
        mark_phone_and_hud_dirty(context);
        return;
    }

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
    append_site_one_seed_listing(context, k_site1_wind_reed_listing_id, k_item_wind_reed_seed_bundle, 10U);
    append_site_one_seed_listing(context, k_site1_salt_bean_listing_id, k_item_salt_bean_seed_bundle, 8U);
    append_site_one_seed_listing(context, k_site1_shade_cactus_listing_id, k_item_shade_cactus_seed_bundle, 8U);
    append_site_one_seed_listing(context, k_site1_sunfruit_vine_listing_id, k_item_sunfruit_vine_seed_bundle, 6U);
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
    append_site_one_seed_listing(
        context,
        k_site1_basic_straw_checkerboard_listing_id,
        k_item_basic_straw_checkerboard,
        8U);
    append_site_one_seed_listing(context, k_site1_root_binder_listing_id, k_item_root_binder_seed_bundle, 8U);
    append_site_one_seed_listing(context, k_site1_dew_grass_listing_id, k_item_dew_grass_seed_bundle, 8U);
    append_site_one_seed_listing(context, k_site1_thorn_shrub_listing_id, k_item_thorn_shrub_seed_bundle, 6U);
    append_site_one_seed_listing(context, k_site1_medicinal_sage_listing_id, k_item_medicinal_sage_seed_bundle, 6U);
    append_site_one_seed_listing(context, k_site1_sand_willow_listing_id, k_item_sand_willow_seed_bundle, 4U);
    economy.available_phone_listings.push_back(PhoneListingState {
        PhoneListingKind::HireContractor,
        ItemId {0U},
        k_site1_contractor_listing_id,
        8,
        3U,
        0U,
        true});
    economy.available_phone_listings.push_back(PhoneListingState {
        PhoneListingKind::PurchaseUnlockable,
        ItemId {k_site1_unlockable_id},
        k_site1_unlockable_listing_id,
        20,
        1U,
        0U,
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
    case GameMessageType::EconomyMoneyAwardRequested:
    case GameMessageType::PhoneListingPurchaseRequested:
    case GameMessageType::PhoneListingSaleRequested:
    case GameMessageType::PhoneListingCartAddRequested:
    case GameMessageType::PhoneListingCartRemoveRequested:
    case GameMessageType::PhoneCartCheckoutRequested:
    case GameMessageType::ContractorHireRequested:
    case GameMessageType::SiteUnlockableRevealRequested:
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

    case GameMessageType::EconomyMoneyAwardRequested:
    {
        const auto& payload = message.payload_as<EconomyMoneyAwardRequestedMessage>();
        if (payload.delta == 0)
        {
            return GS1_STATUS_OK;
        }

        if (!adjust_money(context, payload.delta))
        {
            return GS1_STATUS_INVALID_STATE;
        }
        queue_campaign_cash_delta_message(context, payload.delta);

        mark_phone_and_hud_dirty(context);
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
                refresh_dynamic_sell_listings(context, true);
                return GS1_STATUS_OK;
            }
        }

        const std::uint32_t quantity = normalize_quantity(payload.quantity);
        return process_sell_listing(context, *listing, quantity);
    }

    case GameMessageType::PhoneListingCartAddRequested:
    {
        const auto& payload = message.payload_as<PhoneListingCartAddRequestedMessage>();
        auto* listing = find_listing(context.world.own_economy(), payload.listing_id);
        if (listing == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        return process_cart_add(context, *listing, normalize_quantity(payload.quantity));
    }

    case GameMessageType::PhoneListingCartRemoveRequested:
    {
        const auto& payload = message.payload_as<PhoneListingCartRemoveRequestedMessage>();
        auto* listing = find_listing(context.world.own_economy(), payload.listing_id);
        if (listing == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }

        return process_cart_remove(context, *listing, normalize_quantity(payload.quantity));
    }

    case GameMessageType::PhoneCartCheckoutRequested:
        return process_cart_checkout(context);

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

    case GameMessageType::SiteUnlockableRevealRequested:
    {
        const auto& payload = message.payload_as<SiteUnlockableRevealRequestedMessage>();
        reveal_unlockable_for_site(context, payload.unlockable_id);
        return GS1_STATUS_OK;
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
