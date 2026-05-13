#include "site/systems/site_protection_presentation_system.h"

namespace gs1
{
namespace
{
[[nodiscard]] Gs1RuntimeMessage make_engine_message(Gs1EngineMessageType type)
{
    Gs1RuntimeMessage message {};
    message.type = type;
    return message;
}

void queue_runtime_message(RuntimeInvocation& invocation, const Gs1RuntimeMessage& message)
{
    invocation.push_runtime_message(message);
}

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

void queue_close_site_protection_selector_ui(RuntimeInvocation& invocation)
{
    auto close_message = make_engine_message(GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP);
    auto& close_payload = close_message.emplace_payload<Gs1EngineMessageCloseUiSetupData>();
    close_payload.setup_id = GS1_UI_SETUP_SITE_PROTECTION_SELECTOR;
    close_payload.presentation_type = GS1_UI_SETUP_PRESENTATION_OVERLAY;
    queue_runtime_message(invocation, close_message);
}

void queue_site_protection_overlay_state_message(
    RuntimeInvocation& invocation,
    const std::optional<SiteRunState>& active_site_run,
    const SiteProtectionPresentationState& protection)
{
    if (!active_site_run.has_value())
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteProtectionOverlayData>();
    payload.mode = protection.overlay_mode;
    payload.reserved0[0] = 0U;
    payload.reserved0[1] = 0U;
    payload.reserved0[2] = 0U;
    queue_runtime_message(invocation, message);

    auto visibility_message = make_engine_message(GS1_ENGINE_MESSAGE_SET_UI_SURFACE_VISIBILITY);
    auto& visibility_payload =
        visibility_message.emplace_payload<Gs1EngineMessageUiSurfaceVisibilityData>();
    visibility_payload.surface_id = GS1_UI_SURFACE_SITE_OVERLAY_PANEL;
    visibility_payload.visible = protection.overlay_mode != GS1_SITE_PROTECTION_OVERLAY_NONE ? 1U : 0U;
    visibility_payload.reserved0 = 0U;
    queue_runtime_message(invocation, visibility_message);
}

void queue_site_protection_selector_ui_messages(
    RuntimeInvocation& invocation,
    Gs1AppState app_state,
    const std::optional<SiteRunState>& active_site_run)
{
    if (!active_site_run.has_value() || app_state != GS1_APP_STATE_SITE_ACTIVE)
    {
        return;
    }

    auto close_message = make_engine_message(GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP);
    auto& close_payload = close_message.emplace_payload<Gs1EngineMessageCloseUiSetupData>();
    close_payload.setup_id = GS1_UI_SETUP_SITE_PROTECTION_SELECTOR;
    close_payload.presentation_type = GS1_UI_SETUP_PRESENTATION_OVERLAY;
    queue_runtime_message(invocation, close_message);

    auto begin_message = make_engine_message(GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP);
    auto& begin_payload = begin_message.emplace_payload<Gs1EngineMessageUiSetupData>();
    begin_payload.setup_id = GS1_UI_SETUP_SITE_PROTECTION_SELECTOR;
    begin_payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    begin_payload.presentation_type = GS1_UI_SETUP_PRESENTATION_OVERLAY;
    begin_payload.element_count = 6U;
    begin_payload.context_id = active_site_run->site_id.value;
    queue_runtime_message(invocation, begin_message);

    const auto queue_element = [&invocation](
                                   std::uint32_t element_id,
                                   Gs1UiElementType element_type,
                                   std::uint32_t flags,
                                   const Gs1UiAction& action,
                                   std::uint8_t content_kind,
                                   std::uint32_t primary_id = 0U,
                                   std::uint32_t secondary_id = 0U)
    {
        auto element_message = make_engine_message(GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT);
        auto& element_payload = element_message.emplace_payload<Gs1EngineMessageUiElementData>();
        element_payload.element_id = element_id;
        element_payload.element_type = element_type;
        element_payload.flags = static_cast<std::uint16_t>(flags);
        element_payload.action = action;
        element_payload.content_kind = content_kind;
        element_payload.primary_id = primary_id;
        element_payload.secondary_id = secondary_id;
        element_payload.quantity = 0U;
        queue_runtime_message(invocation, element_message);
    };

    Gs1UiAction no_action {};
    queue_element(1U, GS1_UI_ELEMENT_LABEL, GS1_UI_ELEMENT_FLAG_NONE, no_action, 3U);

    const auto queue_overlay_button = [&queue_element](std::uint32_t element_id, Gs1SiteProtectionOverlayMode mode)
    {
        Gs1UiAction action {};
        action.type = GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
        action.arg0 = mode;
        queue_element(
            element_id,
            GS1_UI_ELEMENT_BUTTON,
            GS1_UI_ELEMENT_FLAG_PRIMARY,
            action,
            4U,
            mode);
    };

    queue_overlay_button(2U, GS1_SITE_PROTECTION_OVERLAY_WIND);
    queue_overlay_button(3U, GS1_SITE_PROTECTION_OVERLAY_HEAT);
    queue_overlay_button(4U, GS1_SITE_PROTECTION_OVERLAY_DUST);
    queue_overlay_button(5U, GS1_SITE_PROTECTION_OVERLAY_OCCUPANT_CONDITION);

    Gs1UiAction close_action {};
    close_action.type = GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI;
    queue_element(
        6U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_BACKGROUND_CLICK,
        close_action,
        5U);

    queue_runtime_message(invocation, make_engine_message(GS1_ENGINE_MESSAGE_END_UI_SETUP));
}
}  // namespace

template <>
struct system_state_tags<SiteProtectionPresentationSystem>
{
    using type = type_list<
        RuntimeAppStateTag,
        RuntimeCampaignTag,
        RuntimeActiveSiteRunTag,
        RuntimePhonePanelPresentationTag,
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
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::OpenRegionalMapTechTree,
        GameMessageType::PhonePanelSectionRequested,
        GameMessageType::InventoryStorageViewRequest,
        GameMessageType::StartSiteAttempt,
        GameMessageType::ReturnToRegionalMap,
        GameMessageType::SiteAttemptEnded,
        GameMessageType::OpenSiteProtectionSelector,
        GameMessageType::CloseSiteProtectionUi,
        GameMessageType::SetSiteProtectionOverlayMode,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan SiteProtectionPresentationSystem::subscribed_host_messages() const noexcept
{
    static constexpr Gs1HostMessageType subscriptions[] = {GS1_HOST_EVENT_SITE_STORAGE_VIEW};
    return subscriptions;
}

std::optional<Gs1RuntimeProfileSystemId> SiteProtectionPresentationSystem::profile_system_id() const noexcept
{
    return std::nullopt;
}

std::optional<std::uint32_t> SiteProtectionPresentationSystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}
Gs1Status SiteProtectionPresentationSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    auto access = make_game_state_access<SiteProtectionPresentationSystem>(invocation);
    auto& app_state = access.template read<RuntimeAppStateTag>();
    auto& campaign = access.template read<RuntimeCampaignTag>();
    auto& active_site_run = access.template read<RuntimeActiveSiteRunTag>();
    const auto& phone_panel = access.template read<RuntimePhonePanelPresentationTag>();
    auto& protection = access.template write<RuntimeSiteProtectionPresentationTag>();

    if (message.type == GS1_HOST_EVENT_SITE_STORAGE_VIEW)
    {
        if (message.payload.site_storage_view.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
        {
            if (protection.selector_open || protection.overlay_mode != GS1_SITE_PROTECTION_OVERLAY_NONE)
            {
                clear_protection_ui_state(protection);
            }
            if (active_site_run.has_value() && phone_panel.open)
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
    const auto& phone_panel = access.template read<RuntimePhonePanelPresentationTag>();
    auto& protection = access.template write<RuntimeSiteProtectionPresentationTag>();

    switch (message.type)
    {
    case GameMessageType::OpenRegionalMapTechTree:
        if (protection.selector_open || protection.overlay_mode != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            if (protection.selector_open)
            {
                queue_close_site_protection_selector_ui(invocation);
            }
            clear_protection_ui_state(protection);
        }
        queue_site_protection_overlay_state_message(invocation, active_site_run, protection);
        if (active_site_run.has_value())
        {
            queue_close_site_inventory_panels(invocation, *active_site_run);
            if (phone_panel.open)
            {
                queue_close_phone_panel(invocation);
            }
        }
        return GS1_STATUS_OK;

    case GameMessageType::PhonePanelSectionRequested:
        if (protection.selector_open || protection.overlay_mode != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            if (protection.selector_open)
            {
                queue_close_site_protection_selector_ui(invocation);
            }
            clear_protection_ui_state(protection);
        }
        queue_site_protection_overlay_state_message(invocation, active_site_run, protection);
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
                if (protection.selector_open)
                {
                    queue_close_site_protection_selector_ui(invocation);
                }
                clear_protection_ui_state(protection);
            }
            queue_site_protection_overlay_state_message(invocation, active_site_run, protection);
            if (active_site_run.has_value() && phone_panel.open)
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
        if (protection.selector_open)
        {
            queue_close_site_protection_selector_ui(invocation);
        }
        clear_protection_ui_state(protection);
        queue_site_protection_overlay_state_message(invocation, active_site_run, protection);
        return GS1_STATUS_OK;

    case GameMessageType::OpenSiteProtectionSelector:
        if (!active_site_run.has_value() || app_state != GS1_APP_STATE_SITE_ACTIVE)
        {
            return GS1_STATUS_OK;
        }
        queue_close_site_inventory_panels(invocation, *active_site_run);
        if (phone_panel.open)
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
        queue_site_protection_selector_ui_messages(invocation, app_state, active_site_run);
        return GS1_STATUS_OK;

    case GameMessageType::CloseSiteProtectionUi:
        if (protection.selector_open)
        {
            queue_close_site_protection_selector_ui(invocation);
            protection.selector_open = false;
        }
        else if (protection.overlay_mode != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            protection.overlay_mode = GS1_SITE_PROTECTION_OVERLAY_NONE;
        }
        queue_site_protection_overlay_state_message(invocation, active_site_run, protection);
        return GS1_STATUS_OK;

    case GameMessageType::SetSiteProtectionOverlayMode:
        if (!active_site_run.has_value() || app_state != GS1_APP_STATE_SITE_ACTIVE)
        {
            return GS1_STATUS_OK;
        }
        if (protection.selector_open)
        {
            queue_close_site_protection_selector_ui(invocation);
        }
        protection.selector_open = false;
        protection.overlay_mode =
            message.payload_as<SetSiteProtectionOverlayModeMessage>().mode;
        queue_site_protection_overlay_state_message(invocation, active_site_run, protection);
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
