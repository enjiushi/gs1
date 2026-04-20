#include "site/systems/phone_panel_system.h"

#include "content/defs/item_defs.h"
#include "site/craft_logic.h"
#include "site/inventory_storage.h"
#include "site/site_projection_update_flags.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace gs1
{
namespace
{
constexpr std::uint32_t k_sell_listing_id_base = 1000U;

void mark_phone_dirty(SiteSystemContext<PhonePanelSystem>& context) noexcept
{
    context.world.mark_projection_dirty(SITE_PROJECTION_UPDATE_PHONE);
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

std::vector<std::uint32_t> phone_item_instance_ids(SiteSystemContext<PhonePanelSystem>& context)
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
    SiteSystemContext<PhonePanelSystem>& context,
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

PhoneListingState make_sell_listing(ItemId item_id, std::uint32_t quantity) noexcept
{
    const auto* item_def = find_item_def(item_id);
    return PhoneListingState {
        PhoneListingKind::SellItem,
        item_id,
        make_sell_listing_id(item_id),
        item_def == nullptr ? 0 : item_def->sell_price,
        quantity,
        0U,
        true};
}

void build_projected_listings(
    SiteSystemContext<PhonePanelSystem>& context,
    std::vector<PhoneListingState>& out_listings)
{
    out_listings.clear();
    const auto& economy = context.world.read_economy();
    out_listings.reserve(economy.available_phone_listings.size());

    for (const auto& listing : economy.available_phone_listings)
    {
        if (listing.kind == PhoneListingKind::SellItem)
        {
            continue;
        }

        auto projected = listing;
        clamp_cart_quantity(projected);
        out_listings.push_back(projected);
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

        sell_listings.push_back(make_sell_listing(item_def.item_id, available_quantity));
    }

    std::sort(sell_listings.begin(), sell_listings.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.item_id.value < rhs.item_id.value;
    });
    out_listings.insert(out_listings.end(), sell_listings.begin(), sell_listings.end());
}

void count_projected_tasks(
    const TaskBoardState& task_board,
    std::uint32_t& out_visible_count,
    std::uint32_t& out_accepted_count,
    std::uint32_t& out_completed_count,
    std::uint32_t& out_claimed_count) noexcept
{
    out_visible_count = 0U;
    out_accepted_count = 0U;
    out_completed_count = 0U;
    out_claimed_count = 0U;
    for (const auto& task : task_board.visible_tasks)
    {
        if (task.runtime_list_kind == TaskRuntimeListKind::Accepted)
        {
            out_accepted_count += 1U;
        }
        else if (task.runtime_list_kind == TaskRuntimeListKind::Completed)
        {
            out_completed_count += 1U;
        }
        else if (task.runtime_list_kind == TaskRuntimeListKind::Claimed)
        {
            out_claimed_count += 1U;
        }
        else if (task.runtime_list_kind == TaskRuntimeListKind::Visible)
        {
            out_visible_count += 1U;
        }
    }
}

void count_projected_listings(
    const std::vector<PhoneListingState>& listings,
    std::uint32_t& out_buy_count,
    std::uint32_t& out_sell_count,
    std::uint32_t& out_service_count,
    std::uint32_t& out_cart_item_count) noexcept
{
    out_buy_count = 0U;
    out_sell_count = 0U;
    out_service_count = 0U;
    out_cart_item_count = 0U;

    for (const auto& listing : listings)
    {
        switch (listing.kind)
        {
        case PhoneListingKind::BuyItem:
            out_buy_count += 1U;
            out_cart_item_count += listing.cart_quantity;
            break;
        case PhoneListingKind::SellItem:
            out_sell_count += 1U;
            break;
        case PhoneListingKind::HireContractor:
        case PhoneListingKind::PurchaseUnlockable:
            out_service_count += 1U;
            break;
        }
    }
}

void sync_phone_panel_projection(
    SiteSystemContext<PhonePanelSystem>& context,
    bool force_dirty = false)
{
    auto& phone_panel = context.world.own_phone_panel();

    std::vector<PhoneListingState> projected_listings {};
    build_projected_listings(context, projected_listings);

    std::uint32_t visible_task_count = 0U;
    std::uint32_t accepted_task_count = 0U;
    std::uint32_t completed_task_count = 0U;
    std::uint32_t claimed_task_count = 0U;
    count_projected_tasks(
        context.world.read_task_board(),
        visible_task_count,
        accepted_task_count,
        completed_task_count,
        claimed_task_count);

    std::uint32_t buy_listing_count = 0U;
    std::uint32_t sell_listing_count = 0U;
    std::uint32_t service_listing_count = 0U;
    std::uint32_t cart_item_count = 0U;
    count_projected_listings(
        projected_listings,
        buy_listing_count,
        sell_listing_count,
        service_listing_count,
        cart_item_count);

    const bool changed =
        !same_listing_vector(projected_listings, phone_panel.listings) ||
        phone_panel.visible_task_count != visible_task_count ||
        phone_panel.accepted_task_count != accepted_task_count ||
        phone_panel.completed_task_count != completed_task_count ||
        phone_panel.claimed_task_count != claimed_task_count ||
        phone_panel.buy_listing_count != buy_listing_count ||
        phone_panel.sell_listing_count != sell_listing_count ||
        phone_panel.service_listing_count != service_listing_count ||
        phone_panel.cart_item_count != cart_item_count;

    phone_panel.listings = std::move(projected_listings);
    phone_panel.visible_task_count = visible_task_count;
    phone_panel.accepted_task_count = accepted_task_count;
    phone_panel.completed_task_count = completed_task_count;
    phone_panel.claimed_task_count = claimed_task_count;
    phone_panel.buy_listing_count = buy_listing_count;
    phone_panel.sell_listing_count = sell_listing_count;
    phone_panel.service_listing_count = service_listing_count;
    phone_panel.cart_item_count = cart_item_count;

    if (changed || force_dirty)
    {
        mark_phone_dirty(context);
    }
}
}  // namespace

bool PhonePanelSystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::SiteRunStarted:
    case GameMessageType::PhonePanelSectionRequested:
        return true;
    default:
        return false;
    }
}

Gs1Status PhonePanelSystem::process_message(
    SiteSystemContext<PhonePanelSystem>& context,
    const GameMessage& message)
{
    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        context.world.own_phone_panel() = PhonePanelState {};
        sync_phone_panel_projection(context, true);
        return GS1_STATUS_OK;

    case GameMessageType::PhonePanelSectionRequested:
    {
        const auto requested_section =
            message.payload_as<PhonePanelSectionRequestedMessage>().section;
        auto& phone_panel = context.world.own_phone_panel();
        PhonePanelSection section = PhonePanelSection::Marketplace;
        switch (requested_section)
        {
        case GS1_PHONE_PANEL_SECTION_MARKETPLACE:
            section = PhonePanelSection::Marketplace;
            break;
        case GS1_PHONE_PANEL_SECTION_CART:
            section = PhonePanelSection::Cart;
            break;
        default:
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        if (phone_panel.active_section != section)
        {
            phone_panel.active_section = section;
            mark_phone_dirty(context);
        }
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}

void PhonePanelSystem::run(SiteSystemContext<PhonePanelSystem>& context)
{
    sync_phone_panel_projection(context);
}
}  // namespace gs1
