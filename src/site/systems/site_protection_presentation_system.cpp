#include "site/systems/site_protection_presentation_system.h"

namespace gs1
{
namespace
{
[[nodiscard]] bool app_state_supports_technology_tree(Gs1AppState app_state) noexcept
{
    return app_state == GS1_APP_STATE_REGIONAL_MAP ||
        app_state == GS1_APP_STATE_SITE_ACTIVE;
}

void queue_close_regional_map_tech_tree(RuntimeInvocation& invocation)
{
    GameMessage close_tech_tree {};
    close_tech_tree.type = GameMessageType::CloseRegionalMapTechTree;
    close_tech_tree.set_payload(CloseRegionalMapTechTreeMessage {});
    invocation.push_game_message(close_tech_tree);
}

void queue_close_phone_panel(RuntimeInvocation& invocation)
{
    GameMessage close_phone_panel {};
    close_phone_panel.type = GameMessageType::ClosePhonePanel;
    close_phone_panel.set_payload(ClosePhonePanelMessage {});
    invocation.push_game_message(close_phone_panel);
}

void queue_storage_close_request(
    RuntimeInvocation& invocation,
    std::uint32_t storage_id)
{
    if (storage_id == 0U)
    {
        return;
    }

    GameMessage close_storage {};
    close_storage.type = GameMessageType::InventoryStorageViewRequest;
    close_storage.set_payload(InventoryStorageViewRequestMessage {
        storage_id,
        GS1_INVENTORY_VIEW_EVENT_CLOSE,
        {0U, 0U, 0U}});
    invocation.push_game_message(close_storage);
}

void queue_close_site_inventory_panels(
    RuntimeInvocation& invocation,
    const SiteRunState& site_run)
{
    const auto& inventory = site_run.inventory;
    if (inventory.worker_pack_panel_open)
    {
        queue_storage_close_request(invocation, inventory.worker_pack_storage_id);
    }
    if (inventory.opened_device_storage_id != 0U)
    {
        queue_storage_close_request(invocation, inventory.opened_device_storage_id);
    }
    if (inventory.pending_device_storage_open.active &&
        inventory.pending_device_storage_open.storage_id != 0U &&
        inventory.pending_device_storage_open.storage_id != inventory.opened_device_storage_id)
    {
        queue_storage_close_request(invocation, inventory.pending_device_storage_open.storage_id);
    }
}

void clear_protection_ui_state(SiteProtectionPresentationState& protection) noexcept
{
    protection.selector_open = false;
    protection.overlay_mode = GS1_SITE_PROTECTION_OVERLAY_NONE;
}
}  // namespace

template <>
struct system_state_tags<SiteProtectionPresentationSystem>
{
    using type = type_list<
        RuntimeAppStateTag,
        RuntimeCampaignTag,
        RuntimeActiveSiteRunTag,
        RuntimeSiteProtectionPresentationTag>;
};

template <>
struct state_owner<RuntimeSiteProtectionPresentationTag>
{
    using type = SiteProtectionPresentationSystem;
};

const char* SiteProtectionPresentationSystem::name() const noexcept
{
    return "SiteProtectionPresentationSystem";
}

GameMessageSubscriptionSpan SiteProtectionPresentationSystem::subscribed_game_messages() const noexcept
{
    return runtime_subscription_list<
        GameMessageType,
        k_game_message_type_count,
        SiteProtectionPresentationSystem::subscribes_to>();
}

HostMessageSubscriptionSpan SiteProtectionPresentationSystem::subscribed_host_messages() const noexcept
{
    return runtime_subscription_list<
        Gs1HostMessageType,
        k_runtime_host_message_type_count,
        SiteProtectionPresentationSystem::subscribes_to_host_message>();
}

std::optional<Gs1RuntimeProfileSystemId> SiteProtectionPresentationSystem::profile_system_id() const noexcept
{
    return std::nullopt;
}

std::optional<std::uint32_t> SiteProtectionPresentationSystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}

bool SiteProtectionPresentationSystem::subscribes_to_host_message(Gs1HostMessageType type) noexcept
{
    return type == GS1_HOST_EVENT_SITE_STORAGE_VIEW;
}

bool SiteProtectionPresentationSystem::subscribes_to(GameMessageType type) noexcept
{
    switch (type)
    {
    case GameMessageType::OpenRegionalMapTechTree:
    case GameMessageType::PhonePanelSectionRequested:
    case GameMessageType::InventoryStorageViewRequest:
    case GameMessageType::StartSiteAttempt:
    case GameMessageType::ReturnToRegionalMap:
    case GameMessageType::SiteAttemptEnded:
    case GameMessageType::OpenSiteProtectionSelector:
    case GameMessageType::CloseSiteProtectionUi:
    case GameMessageType::SetSiteProtectionOverlayMode:
        return true;
    default:
        return false;
    }
}

Gs1Status SiteProtectionPresentationSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    auto access = make_game_state_access<SiteProtectionPresentationSystem>(invocation);
    auto& app_state = access.template read<RuntimeAppStateTag>();
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& active_site_run = access.template read<RuntimeActiveSiteRunTag>();
    auto& protection = access.template write<RuntimeSiteProtectionPresentationTag>();

    if (message.type == GS1_HOST_EVENT_SITE_STORAGE_VIEW)
    {
        if (message.payload.site_storage_view.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
        {
            if (protection.selector_open || protection.overlay_mode != GS1_SITE_PROTECTION_OVERLAY_NONE)
            {
                clear_protection_ui_state(protection);
            }
            if (active_site_run.has_value() && active_site_run->phone_panel.open)
            {
                queue_close_phone_panel(invocation);
            }
            if (campaign.has_value() &&
                app_state_supports_technology_tree(app_state) &&
                campaign->regional_map_state.tech_tree_open)
            {
                queue_close_regional_map_tech_tree(invocation);
            }
        }
        return GS1_STATUS_OK;
    }

    return GS1_STATUS_OK;
}

Gs1Status SiteProtectionPresentationSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<SiteProtectionPresentationSystem>(invocation);
    auto& app_state = access.template read<RuntimeAppStateTag>();
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& active_site_run = access.template read<RuntimeActiveSiteRunTag>();
    auto& protection = access.template write<RuntimeSiteProtectionPresentationTag>();

    switch (message.type)
    {
    case GameMessageType::OpenRegionalMapTechTree:
        if (protection.selector_open || protection.overlay_mode != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            clear_protection_ui_state(protection);
        }
        if (active_site_run.has_value())
        {
            queue_close_site_inventory_panels(invocation, *active_site_run);
            if (active_site_run->phone_panel.open)
            {
                queue_close_phone_panel(invocation);
            }
        }
        return GS1_STATUS_OK;

    case GameMessageType::PhonePanelSectionRequested:
        if (protection.selector_open || protection.overlay_mode != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            clear_protection_ui_state(protection);
        }
        if (active_site_run.has_value())
        {
            queue_close_site_inventory_panels(invocation, *active_site_run);
        }
        if (campaign.has_value() &&
            app_state_supports_technology_tree(app_state) &&
            campaign->regional_map_state.tech_tree_open)
        {
            queue_close_regional_map_tech_tree(invocation);
        }
        return GS1_STATUS_OK;

    case GameMessageType::InventoryStorageViewRequest:
        if (message.payload_as<InventoryStorageViewRequestMessage>().event_kind ==
            GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
        {
            if (protection.selector_open || protection.overlay_mode != GS1_SITE_PROTECTION_OVERLAY_NONE)
            {
                clear_protection_ui_state(protection);
            }
            if (active_site_run.has_value() && active_site_run->phone_panel.open)
            {
                queue_close_phone_panel(invocation);
            }
            if (campaign.has_value() &&
                app_state_supports_technology_tree(app_state) &&
                campaign->regional_map_state.tech_tree_open)
            {
                queue_close_regional_map_tech_tree(invocation);
            }
        }
        return GS1_STATUS_OK;

    case GameMessageType::StartSiteAttempt:
    case GameMessageType::ReturnToRegionalMap:
    case GameMessageType::SiteAttemptEnded:
        clear_protection_ui_state(protection);
        return GS1_STATUS_OK;

    case GameMessageType::OpenSiteProtectionSelector:
        if (!active_site_run.has_value() || app_state != GS1_APP_STATE_SITE_ACTIVE)
        {
            return GS1_STATUS_OK;
        }
        queue_close_site_inventory_panels(invocation, *active_site_run);
        if (active_site_run->phone_panel.open)
        {
            queue_close_phone_panel(invocation);
        }
        if (campaign.has_value() &&
            app_state_supports_technology_tree(app_state) &&
            campaign->regional_map_state.tech_tree_open)
        {
            queue_close_regional_map_tech_tree(invocation);
        }
        protection.selector_open = true;
        protection.overlay_mode = GS1_SITE_PROTECTION_OVERLAY_NONE;
        return GS1_STATUS_OK;

    case GameMessageType::CloseSiteProtectionUi:
        if (protection.selector_open)
        {
            protection.selector_open = false;
        }
        else if (protection.overlay_mode != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            protection.overlay_mode = GS1_SITE_PROTECTION_OVERLAY_NONE;
        }
        return GS1_STATUS_OK;

    case GameMessageType::SetSiteProtectionOverlayMode:
        if (!active_site_run.has_value() || app_state != GS1_APP_STATE_SITE_ACTIVE)
        {
            return GS1_STATUS_OK;
        }
        protection.selector_open = false;
        protection.overlay_mode =
            message.payload_as<SetSiteProtectionOverlayModeMessage>().mode;
        return GS1_STATUS_OK;

    default:
        return GS1_STATUS_OK;
    }
}

void SiteProtectionPresentationSystem::run(RuntimeInvocation& invocation)
{
    (void)invocation;
}
}  // namespace gs1
