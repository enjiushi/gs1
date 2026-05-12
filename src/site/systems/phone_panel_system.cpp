#include "site/systems/phone_panel_system.h"

#include "content/defs/item_defs.h"
#include "runtime/game_runtime.h"
#include "site/craft_logic.h"
#include "site/inventory_storage.h"
#include "site/site_projection_update_flags.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace gs1
{
template <>
struct system_state_tags<PhonePanelSystem>
{
    using type = type_list<
        RuntimeCampaignTag,
        RuntimeActiveSiteRunTag,
        RuntimeFixedStepSecondsTag,
        RuntimeMoveDirectionTag>;
};

namespace
{
constexpr std::uint32_t k_sell_listing_id_base = 1000U;
constexpr std::uint64_t k_phone_signature_offset = 14695981039346656037ULL;
constexpr std::uint64_t k_phone_signature_prime = 1099511628211ULL;

[[nodiscard]] auto phone_panel_access(RuntimeInvocation& invocation)
    -> GameStateAccess<PhonePanelSystem>
{
    return make_game_state_access<PhonePanelSystem>(invocation);
}

[[nodiscard]] SiteRunState& phone_panel_site_run(RuntimeInvocation& invocation)
{
    auto access = phone_panel_access(invocation);
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    return *site_run;
}

[[nodiscard]] SiteWorldAccess<PhonePanelSystem> phone_panel_world(RuntimeInvocation& invocation)
{
    return SiteWorldAccess<PhonePanelSystem> {phone_panel_site_run(invocation)};
}

[[nodiscard]] GameMessageQueue& phone_panel_message_queue(RuntimeInvocation& invocation)
{
    return invocation.game_message_queue();
}

void mark_phone_dirty(RuntimeInvocation& invocation) noexcept
{
    phone_panel_world(invocation).mark_projection_dirty(SITE_PROJECTION_UPDATE_PHONE);
}

std::uint64_t mix_phone_signature(std::uint64_t signature, std::uint64_t value) noexcept
{
    return (signature ^ value) * k_phone_signature_prime;
}

std::uint32_t make_sell_listing_id(ItemId item_id) noexcept
{
    return k_sell_listing_id_base + item_id.value;
}

std::uint32_t badge_flag_for_section(PhonePanelSection section) noexcept
{
    switch (section)
    {
    case PhonePanelSection::Tasks:
        return GS1_PHONE_PANEL_FLAG_TASKS_BADGE;
    case PhonePanelSection::Buy:
        return GS1_PHONE_PANEL_FLAG_BUY_BADGE;
    case PhonePanelSection::Sell:
        return GS1_PHONE_PANEL_FLAG_SELL_BADGE;
    case PhonePanelSection::Hire:
        return GS1_PHONE_PANEL_FLAG_HIRE_BADGE;
    default:
        return 0U;
    }
}

bool section_is_visible(
    const PhonePanelState& phone_panel,
    PhonePanelSection section) noexcept
{
    return phone_panel.open && phone_panel.active_section == section;
}

bool try_map_phone_panel_section(
    Gs1PhonePanelSection requested_section,
    PhonePanelSection& out_section) noexcept
{
    switch (requested_section)
    {
    case GS1_PHONE_PANEL_SECTION_HOME:
        out_section = PhonePanelSection::Home;
        return true;
    case GS1_PHONE_PANEL_SECTION_TASKS:
        out_section = PhonePanelSection::Tasks;
        return true;
    case GS1_PHONE_PANEL_SECTION_BUY:
        out_section = PhonePanelSection::Buy;
        return true;
    case GS1_PHONE_PANEL_SECTION_SELL:
        out_section = PhonePanelSection::Sell;
        return true;
    case GS1_PHONE_PANEL_SECTION_HIRE:
        out_section = PhonePanelSection::Hire;
        return true;
    case GS1_PHONE_PANEL_SECTION_CART:
        out_section = PhonePanelSection::Cart;
        return true;
    default:
        return false;
    }
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

std::vector<std::uint64_t> phone_item_instance_ids(RuntimeInvocation& invocation)
{
    const auto& craft = phone_panel_world(invocation).read_craft();
    const auto& inventory = phone_panel_world(invocation).read_inventory();
    if (craft.phone_cache.source_membership_revision == inventory.item_membership_revision)
    {
        return craft.phone_cache.item_instance_ids;
    }

    return inventory_storage::collect_item_instance_ids_in_containers(
        phone_panel_site_run(invocation),
        inventory_storage::collect_all_storage_containers(phone_panel_site_run(invocation), true));
}

std::uint32_t available_global_item_quantity(
    RuntimeInvocation& invocation,
    ItemId item_id)
{
    const auto available_quantity = craft_logic::available_cached_item_quantity(
        phone_panel_site_run(invocation),
        phone_item_instance_ids(invocation),
        item_id);
    std::uint32_t reserved_quantity = 0U;
    for (const auto& reserved_stack : phone_panel_world(invocation).read_action().reserved_input_item_stacks)
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
            left.stock_refresh_generation != right.stock_refresh_generation ||
            left.occupied != right.occupied ||
            left.generated_from_stock != right.generated_from_stock)
        {
            return false;
        }
    }

    return true;
}

std::uint64_t task_snapshot_signature(const TaskBoardState& task_board) noexcept
{
    std::uint64_t signature =
        mix_phone_signature(
            mix_phone_signature(k_phone_signature_offset, task_board.visible_tasks.size()),
            task_board.refresh_generation);
    for (const auto& task : task_board.visible_tasks)
    {
        signature = mix_phone_signature(signature, task.task_instance_id.value);
        signature = mix_phone_signature(signature, task.task_template_id.value);
        signature = mix_phone_signature(signature, task.publisher_faction_id.value);
        signature = mix_phone_signature(signature, task.current_progress_amount);
        signature = mix_phone_signature(signature, task.target_amount);
        signature = mix_phone_signature(signature, task.required_count);
        signature = mix_phone_signature(signature, static_cast<std::uint64_t>(task.runtime_list_kind));
        signature = mix_phone_signature(signature, task.reward_draft_options.size());
    }

    return signature;
}

std::uint64_t listing_snapshot_signature(
    const std::vector<PhoneListingState>& listings,
    PhoneListingKind listing_kind) noexcept
{
    std::uint64_t signature =
        mix_phone_signature(k_phone_signature_offset, static_cast<std::uint64_t>(listing_kind));
    for (const auto& listing : listings)
    {
        if (listing.kind != listing_kind)
        {
            continue;
        }

        signature = mix_phone_signature(signature, listing.item_id.value);
        signature = mix_phone_signature(signature, listing.listing_id);
        signature = mix_phone_signature(signature, static_cast<std::uint64_t>(listing.price));
        signature = mix_phone_signature(signature, listing.quantity);
        signature = mix_phone_signature(signature, listing.stock_refresh_generation);
        signature = mix_phone_signature(signature, listing.occupied ? 1U : 0U);
        signature = mix_phone_signature(signature, listing.generated_from_stock ? 1U : 0U);
    }

    return signature;
}

PhoneListingState make_sell_listing(ItemId item_id, std::uint32_t quantity) noexcept
{
    return PhoneListingState {
        PhoneListingKind::SellItem,
        item_id,
        make_sell_listing_id(item_id),
        static_cast<std::int32_t>(item_sell_price_cash_points(item_id)),
        quantity,
        0U,
        0U,
        true,
        false};
}

void build_projected_listings(
    RuntimeInvocation& invocation,
    std::vector<PhoneListingState>& out_listings)
{
    out_listings.clear();
    const auto& economy = phone_panel_world(invocation).read_economy();
    out_listings.reserve(economy.available_phone_listings.size());

    for (const auto& listing : economy.available_phone_listings)
    {
        if (listing.kind == PhoneListingKind::SellItem ||
            listing.kind == PhoneListingKind::HireContractor)
        {
            continue;
        }

        auto projected = listing;
        clamp_cart_quantity(projected);
        out_listings.push_back(projected);
    }

    std::vector<PhoneListingState> sell_listings {};
    for (const auto& item_def : all_item_defs())
    {
        if (!item_has_capability(item_def, ITEM_CAPABILITY_SELL))
        {
            continue;
        }

        const auto available_quantity = available_global_item_quantity(invocation, item_def.item_id);
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
        else if (task.runtime_list_kind == TaskRuntimeListKind::PendingClaim)
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
    RuntimeInvocation& invocation,
    bool force_dirty = false)
{
    auto& phone_panel = phone_panel_world(invocation).own_phone_panel();

    std::vector<PhoneListingState> projected_listings {};
    build_projected_listings(invocation, projected_listings);

    std::uint32_t visible_task_count = 0U;
    std::uint32_t accepted_task_count = 0U;
    std::uint32_t completed_task_count = 0U;
    std::uint32_t claimed_task_count = 0U;
    count_projected_tasks(
        phone_panel_world(invocation).read_task_board(),
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

    const auto next_task_signature = task_snapshot_signature(phone_panel_world(invocation).read_task_board());
    const auto next_buy_signature =
        listing_snapshot_signature(projected_listings, PhoneListingKind::BuyItem);
    const auto next_sell_signature =
        listing_snapshot_signature(projected_listings, PhoneListingKind::SellItem);
    const auto next_service_signature =
        listing_snapshot_signature(projected_listings, PhoneListingKind::PurchaseUnlockable) ^
        listing_snapshot_signature(projected_listings, PhoneListingKind::HireContractor);
    const auto previous_badge_flags = phone_panel.badge_flags;

    if (phone_panel.notification_state_initialized)
    {
        std::uint32_t new_badge_flags = 0U;
        if (phone_panel.task_snapshot_signature != next_task_signature &&
            !section_is_visible(phone_panel, PhonePanelSection::Tasks))
        {
            new_badge_flags |= GS1_PHONE_PANEL_FLAG_TASKS_BADGE;
        }
        if (phone_panel.buy_snapshot_signature != next_buy_signature &&
            !section_is_visible(phone_panel, PhonePanelSection::Buy))
        {
            new_badge_flags |= GS1_PHONE_PANEL_FLAG_BUY_BADGE;
        }
        if (phone_panel.sell_snapshot_signature != next_sell_signature &&
            !section_is_visible(phone_panel, PhonePanelSection::Sell))
        {
            new_badge_flags |= GS1_PHONE_PANEL_FLAG_SELL_BADGE;
        }
        if (phone_panel.service_snapshot_signature != next_service_signature &&
            !section_is_visible(phone_panel, PhonePanelSection::Hire))
        {
            new_badge_flags |= GS1_PHONE_PANEL_FLAG_HIRE_BADGE;
        }

        if (new_badge_flags != 0U)
        {
            phone_panel.badge_flags |= new_badge_flags | GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE;
        }
    }

    const bool changed =
        !same_listing_vector(projected_listings, phone_panel.listings) ||
        phone_panel.visible_task_count != visible_task_count ||
        phone_panel.accepted_task_count != accepted_task_count ||
        phone_panel.completed_task_count != completed_task_count ||
        phone_panel.claimed_task_count != claimed_task_count ||
        phone_panel.buy_listing_count != buy_listing_count ||
        phone_panel.sell_listing_count != sell_listing_count ||
        phone_panel.service_listing_count != service_listing_count ||
        phone_panel.cart_item_count != cart_item_count ||
        previous_badge_flags != phone_panel.badge_flags;

    phone_panel.listings = std::move(projected_listings);
    phone_panel.visible_task_count = visible_task_count;
    phone_panel.accepted_task_count = accepted_task_count;
    phone_panel.completed_task_count = completed_task_count;
    phone_panel.claimed_task_count = claimed_task_count;
    phone_panel.buy_listing_count = buy_listing_count;
    phone_panel.sell_listing_count = sell_listing_count;
    phone_panel.service_listing_count = service_listing_count;
    phone_panel.cart_item_count = cart_item_count;
    phone_panel.task_snapshot_signature = next_task_signature;
    phone_panel.buy_snapshot_signature = next_buy_signature;
    phone_panel.sell_snapshot_signature = next_sell_signature;
    phone_panel.service_snapshot_signature = next_service_signature;
    phone_panel.notification_state_initialized = true;

    if (changed || force_dirty)
    {
        mark_phone_dirty(invocation);
    }
}
}  // namespace

const char* PhonePanelSystem::name() const noexcept
{
    return "PhonePanelSystem";
}

GameMessageSubscriptionSpan PhonePanelSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::SiteRunStarted,
        GameMessageType::PhonePanelSectionRequested,
        GameMessageType::ClosePhonePanel,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan PhonePanelSystem::subscribed_host_messages() const noexcept
{
    static constexpr Gs1HostMessageType subscriptions[] = {GS1_HOST_EVENT_UI_ACTION};
    return subscriptions;
}

std::optional<Gs1RuntimeProfileSystemId> PhonePanelSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_PHONE_PANEL;
}

std::optional<std::uint32_t> PhonePanelSystem::fixed_step_order() const noexcept
{
    return 18U;
}

Gs1Status PhonePanelSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<PhonePanelSystem>(invocation);
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }
    switch (message.type)
    {
    case GameMessageType::SiteRunStarted:
        phone_panel_world(invocation).own_phone_panel() = PhonePanelState {};
        sync_phone_panel_projection(invocation, true);
        return GS1_STATUS_OK;

    case GameMessageType::PhonePanelSectionRequested:
    {
        const auto requested_section =
            message.payload_as<PhonePanelSectionRequestedMessage>().section;
        auto& phone_panel = phone_panel_world(invocation).own_phone_panel();
        PhonePanelSection section = PhonePanelSection::Home;
        if (!try_map_phone_panel_section(requested_section, section))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }

        bool dirty = false;
        const auto updated_badge_flags =
            (phone_panel.badge_flags & ~GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE) &
            ~badge_flag_for_section(section);
        if (phone_panel.badge_flags != updated_badge_flags)
        {
            phone_panel.badge_flags = updated_badge_flags;
            dirty = true;
        }
        if (!phone_panel.open)
        {
            phone_panel.open = true;
            dirty = true;
        }
        if (phone_panel.active_section != section)
        {
            phone_panel.active_section = section;
            dirty = true;
        }
        if (dirty)
        {
            mark_phone_dirty(invocation);
        }
        return GS1_STATUS_OK;
    }

    case GameMessageType::ClosePhonePanel:
    {
        auto& phone_panel = phone_panel_world(invocation).own_phone_panel();
        bool dirty = false;
        const auto updated_badge_flags =
            phone_panel.badge_flags & ~GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE;
        if (phone_panel.badge_flags != updated_badge_flags)
        {
            phone_panel.badge_flags = updated_badge_flags;
            dirty = true;
        }
        if (phone_panel.open)
        {
            phone_panel.open = false;
            dirty = true;
        }
        if (dirty)
        {
            mark_phone_dirty(invocation);
        }
        return GS1_STATUS_OK;
    }

    default:
        return GS1_STATUS_OK;
    }
}

Gs1Status PhonePanelSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

void PhonePanelSystem::run(RuntimeInvocation& invocation)
{
    auto access = make_game_state_access<PhonePanelSystem>(invocation);
    auto& site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!site_run.has_value())
    {
        return;
    }
    sync_phone_panel_projection(invocation);
}
}  // namespace gs1

