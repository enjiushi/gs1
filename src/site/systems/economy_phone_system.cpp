#include "campaign/systems/technology_system.h"
#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/task_defs.h"
#include "content/prototype_content.h"
#include "runtime/game_runtime.h"
#include "site/craft_logic.h"
#include "site/inventory_storage.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"
#include "site/systems/economy_phone_system.h"
#include "support/currency.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace gs1
{
template <>
struct system_state_tags<EconomyPhoneSystem>
{
    using type = type_list<
        RuntimeCampaignTag,
        RuntimeActiveSiteRunTag,
        RuntimeFixedStepSecondsTag,
        RuntimeMoveDirectionTag>;
};

namespace
{
struct EconomyPhoneContext final
{
    const CampaignState& campaign;
    SiteRunState& site_run;
    SiteWorldAccess<EconomyPhoneSystem> world;
    GameMessageQueue& message_queue;
    double fixed_step_seconds {0.0};
    SiteMoveDirectionInput move_direction {};
};

template <typename Fn>
Gs1Status with_economy_phone_context(
    RuntimeInvocation& invocation,
    Fn&& fn,
    bool missing_context_is_ok = false)
{
    auto access = make_game_state_access<EconomyPhoneSystem>(invocation);
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    const double fixed_step_seconds = access.template read<RuntimeFixedStepSecondsTag>();
    const auto move_direction = access.template read<RuntimeMoveDirectionTag>();
    if (!campaign.has_value() || !site_run.has_value())
    {
        return missing_context_is_ok ? GS1_STATUS_OK : GS1_STATUS_INVALID_STATE;
    }

    EconomyPhoneContext context {
        *campaign,
        *site_run,
        SiteWorldAccess<EconomyPhoneSystem> {*site_run},
        invocation.game_message_queue(),
        fixed_step_seconds,
        SiteMoveDirectionInput {
            move_direction.world_move_x,
            move_direction.world_move_y,
            move_direction.world_move_z,
            move_direction.present}};
    return fn(context);
}

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

bool can_apply_site_cash_delta(
    const EconomyPhoneContext& context,
    std::int32_t delta) noexcept
{
    const auto updated = static_cast<std::int64_t>(context.world.read_economy().current_cash) + delta;
    if (updated < 0 || updated > std::numeric_limits<std::int32_t>::max())
    {
        return false;
    }

    return true;
}

void mark_phone_dirty(EconomyPhoneContext& context) noexcept
{
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_PHONE);
}

void mark_phone_and_hud_dirty(EconomyPhoneContext& context) noexcept
{
    context.world.mark_projection_dirty(
        SITE_PROJECTION_UPDATE_PHONE | SITE_PROJECTION_UPDATE_HUD);
}

bool apply_site_cash_delta(
    EconomyPhoneContext& context,
    std::int32_t delta) noexcept
{
    if (!can_apply_site_cash_delta(context, delta))
    {
        return false;
    }

    auto& economy = context.world.own_economy();
    economy.current_cash = static_cast<std::int32_t>(
        static_cast<std::int64_t>(economy.current_cash) + delta);
    return true;
}

PhoneListingState make_item_listing(
    PhoneListingKind kind,
    std::uint32_t listing_id,
    ItemId item_id,
    std::uint32_t quantity) noexcept
{
    const auto* item_def = find_item_def(item_id);
    const auto item_price_cash_points =
        kind == PhoneListingKind::SellItem
        ? item_sell_price_cash_points(item_id)
        : item_buy_price_cash_points(item_id);
    return PhoneListingState {
        kind,
        item_id,
        listing_id,
        item_def == nullptr ? 0 : static_cast<std::int32_t>(item_price_cash_points),
        quantity,
        0U,
        0U,
        true,
        false};
}

std::uint32_t make_sell_listing_id(ItemId item_id) noexcept
{
    return k_sell_listing_id_base + item_id.value;
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

std::uint64_t mix_seed(
    std::uint64_t base_seed,
    std::uint32_t a,
    std::uint32_t b) noexcept
{
    std::uint64_t value = base_seed ^ (static_cast<std::uint64_t>(a) << 32U) ^ static_cast<std::uint64_t>(b);
    value ^= value >> 33U;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33U;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33U;
    return value;
}

bool onboarding_chain_effective(const EconomyPhoneContext& context) noexcept
{
    for (const auto& task : context.world.read_task_board().visible_tasks)
    {
        if (task.runtime_list_kind == TaskRuntimeListKind::Claimed)
        {
            continue;
        }

        for (const auto& seed_def : all_site_onboarding_task_seed_defs())
        {
            if (seed_def.site_id == context.site_run.site_id &&
                seed_def.task_template_id == task.task_template_id)
            {
                return true;
            }
        }
    }

    return false;
}

bool phone_buy_stock_item_available(
    const EconomyPhoneContext& context,
    ItemId item_id) noexcept
{
    const auto* item_def = find_item_def(item_id);
    if (item_def == nullptr)
    {
        return false;
    }

    if (item_def->linked_plant_id.value != 0U)
    {
        return TechnologySystem::plant_unlocked(context.campaign, item_def->linked_plant_id);
    }

    return item_buy_price_cash_points(item_id) > 0U;
}

PhoneListingState* find_generated_stock_listing(
    EconomyState& economy,
    std::uint32_t listing_id) noexcept
{
    for (auto& listing : economy.available_phone_listings)
    {
        if (listing.generated_from_stock && listing.listing_id == listing_id)
        {
            return &listing;
        }
    }

    return nullptr;
}

std::vector<std::uint64_t> phone_item_instance_ids(
    EconomyPhoneContext& context)
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
    EconomyPhoneContext& context,
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
            left.stock_refresh_generation != right.stock_refresh_generation ||
            left.occupied != right.occupied ||
            left.generated_from_stock != right.generated_from_stock)
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
    EconomyPhoneContext& context,
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
    payload.minutes_until_arrival = context.world.read_economy().phone_delivery_minutes;
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
    EconomyPhoneContext& context,
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
    EconomyPhoneContext& context,
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
    EconomyPhoneContext& context,
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

std::uint32_t resolved_stock_cash_points(
    const EconomyPhoneContext& context,
    const PrototypePhoneBuyStockContent& stock_entry,
    std::uint32_t refresh_generation) noexcept
{
    const auto span = stock_entry.stock_cash_points_variance * 2U + 1U;
    const auto roll = static_cast<std::uint32_t>(
        mix_seed(
            context.world.site_attempt_seed(),
            refresh_generation ^ stock_entry.listing_id,
            stock_entry.item_id.value) %
        std::max<std::uint32_t>(span, 1U));
    const auto delta = static_cast<std::int64_t>(roll) -
        static_cast<std::int64_t>(stock_entry.stock_cash_points_variance);
    const auto total = static_cast<std::int64_t>(stock_entry.base_stock_cash_points) + delta;
    return total > 0 ? static_cast<std::uint32_t>(total) : 0U;
}

std::uint32_t resolved_stock_quantity(
    const EconomyPhoneContext& context,
    const PrototypePhoneBuyStockContent& stock_entry,
    std::uint32_t refresh_generation) noexcept
{
    const auto unit_price = item_buy_price_cash_points(stock_entry.item_id);
    if (unit_price == 0U)
    {
        return 0U;
    }

    const auto stock_cash_points =
        resolved_stock_cash_points(context, stock_entry, refresh_generation);
    if (stock_cash_points == 0U)
    {
        return 1U;
    }

    return std::max<std::uint32_t>(1U, stock_cash_points / unit_price);
}

void refresh_generated_buy_stock_listings(
    EconomyPhoneContext& context,
    bool force_dirty = false)
{
    auto& economy = context.world.own_economy();
    const auto* site_content = find_prototype_site_content(context.site_run.site_id);
    const auto next_generation = economy.phone_buy_stock_refresh_generation + 1U;

    std::vector<PhoneListingState> refreshed {};
    refreshed.reserve(
        economy.available_phone_listings.size() + (site_content == nullptr ? 0U : site_content->phone_buy_stock_pool.size()));
    for (const auto& listing : economy.available_phone_listings)
    {
        if (!listing.generated_from_stock)
        {
            refreshed.push_back(listing);
        }
    }

    if (site_content != nullptr)
    {
        for (const auto& stock_entry : site_content->phone_buy_stock_pool)
        {
            const auto* item_def = find_item_def(stock_entry.item_id);
            if (item_def == nullptr ||
                !phone_buy_stock_item_available(context, stock_entry.item_id))
            {
                continue;
            }

            const auto quantity =
                resolved_stock_quantity(context, stock_entry, next_generation);
            if (quantity == 0U)
            {
                continue;
            }

            PhoneListingState generated_listing = make_item_listing(
                PhoneListingKind::BuyItem,
                stock_entry.listing_id,
                stock_entry.item_id,
                quantity);
            generated_listing.stock_refresh_generation = next_generation;
            generated_listing.generated_from_stock = true;
            if (auto* previous_listing =
                    find_generated_stock_listing(economy, stock_entry.listing_id);
                previous_listing != nullptr)
            {
                generated_listing.cart_quantity = previous_listing->cart_quantity;
                clamp_cart_quantity(generated_listing);
            }
            refreshed.push_back(generated_listing);
        }
    }

    const bool changed = !same_listing_vector(refreshed, economy.available_phone_listings);
    economy.available_phone_listings = std::move(refreshed);
    economy.phone_buy_stock_refresh_generation = next_generation;
    if (changed || force_dirty)
    {
        mark_phone_dirty(context);
    }
}

void refresh_dynamic_sell_listings(
    EconomyPhoneContext& context,
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
    EconomyPhoneContext& context,
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
        static_cast<std::int64_t>(listing.price) * quantity +
        static_cast<std::int64_t>(context.world.read_economy().phone_delivery_fee);
    if (total_cost < 0 || !money_delta_fits(-total_cost))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (!apply_site_cash_delta(context, static_cast<std::int32_t>(-total_cost)))
    {
        return GS1_STATUS_INVALID_STATE;
    }

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
    EconomyPhoneContext& context,
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
    EconomyPhoneContext& context,
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

Gs1Status process_cart_checkout(EconomyPhoneContext& context)
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

    const auto total_cost =
        subtotal + static_cast<std::int64_t>(context.world.read_economy().phone_delivery_fee);
    if (subtotal <= 0 || total_cost < 0 || !money_delta_fits(-total_cost))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (!apply_site_cash_delta(context, static_cast<std::int32_t>(-total_cost)))
    {
        return GS1_STATUS_INVALID_STATE;
    }

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
        synthetic_listing.listing_id = 0U;
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
    EconomyPhoneContext& context,
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

    if (!apply_site_cash_delta(context, static_cast<std::int32_t>(total_gain)))
    {
        return GS1_STATUS_INVALID_STATE;
    }

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
    EconomyPhoneContext& context,
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

    if (!apply_site_cash_delta(context, static_cast<std::int32_t>(-total_cost)))
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
    EconomyPhoneContext& context,
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

    if (!apply_site_cash_delta(context, static_cast<std::int32_t>(-price)))
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

void reveal_unlockable_for_site(
    EconomyPhoneContext& context,
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

void append_seed_phone_listing(
    EconomyPhoneContext& context,
    const PrototypePhoneListingContent& listing_content)
{
    switch (listing_content.kind)
    {
    case PhoneListingKind::BuyItem:
    {
        const auto item_id = ItemId {listing_content.item_or_unlockable_id};
        const auto* item_def = find_item_def(item_id);
        if (item_def == nullptr)
        {
            return;
        }

        if (!phone_buy_stock_item_available(context, item_id))
        {
            return;
        }

        context.world.own_economy().available_phone_listings.push_back(make_item_listing(
            PhoneListingKind::BuyItem,
            listing_content.listing_id,
            item_id,
            listing_content.quantity));
        return;
    }

    case PhoneListingKind::HireContractor:
    case PhoneListingKind::PurchaseUnlockable:
        context.world.own_economy().available_phone_listings.push_back(PhoneListingState {
            listing_content.kind,
            ItemId {listing_content.item_or_unlockable_id},
            listing_content.listing_id,
            listing_content.kind == PhoneListingKind::PurchaseUnlockable
                ? static_cast<std::int32_t>(listing_content.internal_price_cash_points)
                : listing_content.price,
            listing_content.quantity,
            0U,
            0U,
            true,
            false});
        return;

    case PhoneListingKind::SellItem:
    default:
        return;
    }
}

void seed_site_economy(EconomyPhoneContext& context, std::uint32_t site_id)
{
    auto& economy = context.world.own_economy();
    const auto* site_content = find_prototype_site_content(SiteId {site_id});
    economy.phone_listing_source_membership_revision = 0U;
    economy.phone_listing_source_quantity_revision = 0U;
    economy.phone_listing_source_action_reservation_signature = 0U;
    economy.phone_buy_stock_refresh_generation = 0U;
    economy.current_cash = get_prototype_campaign_content().starting_site_cash;
    economy.phone_delivery_fee = site_content == nullptr ? 0 : site_content->phone_delivery_fee;
    economy.phone_delivery_minutes = site_content == nullptr ? 0U : site_content->phone_delivery_minutes;
    economy.available_phone_listings.clear();
    economy.revealed_site_unlockable_ids.clear();
    economy.direct_purchase_unlockable_ids.clear();
    if (site_content == nullptr)
    {
        mark_phone_and_hud_dirty(context);
        return;
    }

    economy.revealed_site_unlockable_ids = site_content->initial_revealed_unlockable_ids;
    economy.direct_purchase_unlockable_ids = site_content->initial_direct_purchase_unlockable_ids;
    for (const auto& listing_content : site_content->seeded_phone_listings)
    {
        append_seed_phone_listing(context, listing_content);
    }

    refresh_generated_buy_stock_listings(context, true);
    refresh_dynamic_sell_listings(context, true);
    mark_phone_and_hud_dirty(context);
}
}  // namespace

Gs1Status process_game_message_impl(
    EconomyPhoneContext& context,
    const GameMessage& message);

bool EconomyPhoneSystem::subscribes_to_host_message(Gs1HostMessageType type) noexcept
{
    return type == GS1_HOST_EVENT_UI_ACTION;
}

Gs1Status process_host_message_impl(
    EconomyPhoneContext& context,
    const Gs1HostMessage& message)
{
    if (message.type != GS1_HOST_EVENT_UI_ACTION)
    {
        return GS1_STATUS_OK;
    }

    const auto& action = message.payload.ui_action.action;
    switch (action.type)
    {
    case GS1_UI_ACTION_BUY_PHONE_LISTING:
    {
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        auto* listing = find_listing(context.world.own_economy(), action.target_id);
        if (listing == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }
        return process_buy_listing(
            context,
            *listing,
            normalize_quantity(static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0)));
    }

    case GS1_UI_ACTION_ADD_PHONE_LISTING_TO_CART:
    {
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        auto* listing = find_listing(context.world.own_economy(), action.target_id);
        if (listing == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }
        return process_cart_add(
            context,
            *listing,
            normalize_quantity(static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0)));
    }

    case GS1_UI_ACTION_REMOVE_PHONE_LISTING_FROM_CART:
    {
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        auto* listing = find_listing(context.world.own_economy(), action.target_id);
        if (listing == nullptr)
        {
            return GS1_STATUS_NOT_FOUND;
        }
        return process_cart_remove(
            context,
            *listing,
            normalize_quantity(static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0)));
    }

    case GS1_UI_ACTION_CHECKOUT_PHONE_CART:
        return process_cart_checkout(context);

    case GS1_UI_ACTION_SELL_PHONE_LISTING:
    {
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        auto* listing = find_listing(context.world.own_economy(), action.target_id);
        if (listing == nullptr)
        {
            listing = find_sell_listing_for_item(
                context.world.own_economy(),
                action.target_id);
            if (listing == nullptr)
            {
                refresh_dynamic_sell_listings(context, true);
                return GS1_STATUS_OK;
            }
        }
        return process_sell_listing(
            context,
            *listing,
            normalize_quantity(static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0)));
    }

    case GS1_UI_ACTION_HIRE_CONTRACTOR:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        return process_contractor_hire(
            context,
            ContractorHireRequestedMessage {
                action.target_id,
                static_cast<std::uint32_t>(action.arg0)});

    case GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        {
            const auto* listing = find_unlockable_listing(context.world.own_economy(), action.target_id);
            if (listing == nullptr)
            {
                return GS1_STATUS_NOT_FOUND;
            }
            return process_unlockable_purchase(context, action.target_id, listing->price);
        }

    default:
        return GS1_STATUS_OK;
    }
}

bool EconomyPhoneSystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::SiteRunStarted:
    case GameMessageType::SiteRefreshTick:
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

Gs1Status process_game_message_impl(
    EconomyPhoneContext& context,
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

    case GameMessageType::SiteRefreshTick:
        if ((message.payload_as<SiteRefreshTickMessage>().refresh_mask &
                SITE_REFRESH_TICK_PHONE_BUY_STOCK) != 0U &&
            !onboarding_chain_effective(context))
        {
            refresh_generated_buy_stock_listings(context, true);
        }
        return GS1_STATUS_OK;

    case GameMessageType::EconomyMoneyAwardRequested:
    {
        const auto& payload = message.payload_as<EconomyMoneyAwardRequestedMessage>();
        if (payload.delta == 0)
        {
            return GS1_STATUS_OK;
        }

        if (!apply_site_cash_delta(context, payload.delta))
        {
            return GS1_STATUS_INVALID_STATE;
        }

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

void run_impl(EconomyPhoneContext& context)
{
    refresh_dynamic_sell_listings(context);
}

const char* EconomyPhoneSystem::name() const noexcept
{
    return "EconomyPhoneSystem";
}

GameMessageSubscriptionSpan EconomyPhoneSystem::subscribed_game_messages() const noexcept
{
    return runtime_subscription_list<GameMessageType, k_game_message_type_count, &EconomyPhoneSystem::subscribes_to>();
}

HostMessageSubscriptionSpan EconomyPhoneSystem::subscribed_host_messages() const noexcept
{
    return runtime_subscription_list<
        Gs1HostMessageType,
        k_runtime_host_message_type_count,
        &EconomyPhoneSystem::subscribes_to_host_message>();
}

std::optional<Gs1RuntimeProfileSystemId> EconomyPhoneSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_ECONOMY_PHONE;
}

std::optional<std::uint32_t> EconomyPhoneSystem::fixed_step_order() const noexcept
{
    return 17U;
}

Gs1Status EconomyPhoneSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<EconomyPhoneSystem>(invocation);
    (void)access;
    return with_economy_phone_context(
        invocation,
        [&](EconomyPhoneContext& context)
        {
            return process_game_message_impl(context, message);
        });
}

Gs1Status EconomyPhoneSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    auto access = make_game_state_access<EconomyPhoneSystem>(invocation);
    (void)access;
    return with_economy_phone_context(
        invocation,
        [&](EconomyPhoneContext& context)
        {
            return process_host_message_impl(context, message);
        });
}

void EconomyPhoneSystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<EconomyPhoneSystem>(invocation);
    (void)access;
    (void)with_economy_phone_context(
        invocation,
        [&](EconomyPhoneContext& context)
        {
            run_impl(context);
            return GS1_STATUS_OK;
        },
        true);
}
}  // namespace gs1

