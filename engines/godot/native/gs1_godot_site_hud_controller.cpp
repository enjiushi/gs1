#include "gs1_godot_site_hud_controller.h"

#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <utility>

using namespace godot;

namespace
{
constexpr std::int64_t k_ui_action_set_phone_panel_section = 17;
constexpr std::int64_t k_ui_action_close_phone_panel = 22;
constexpr std::int64_t k_ui_action_open_regional_map_tech_tree = 18;
constexpr std::int64_t k_ui_action_open_site_protection_selector = 24;
constexpr int k_inventory_view_event_open_snapshot = 1;

Gs1GodotSiteHudController* resolve_controller(std::int64_t controller_bits)
{
    return reinterpret_cast<Gs1GodotSiteHudController*>(static_cast<std::uintptr_t>(controller_bits));
}

void dispatch_phone_pressed(std::int64_t controller_bits)
{
    if (Gs1GodotSiteHudController* controller = resolve_controller(controller_bits))
    {
        controller->handle_phone_pressed();
    }
}

void dispatch_pack_pressed(std::int64_t controller_bits)
{
    if (Gs1GodotSiteHudController* controller = resolve_controller(controller_bits))
    {
        controller->handle_pack_pressed();
    }
}

void dispatch_tasks_pressed(std::int64_t controller_bits)
{
    if (Gs1GodotSiteHudController* controller = resolve_controller(controller_bits))
    {
        controller->handle_tasks_pressed();
    }
}

void dispatch_craft_pressed(std::int64_t controller_bits)
{
    if (Gs1GodotSiteHudController* controller = resolve_controller(controller_bits))
    {
        controller->handle_craft_pressed();
    }
}

void dispatch_protection_pressed(std::int64_t controller_bits)
{
    if (Gs1GodotSiteHudController* controller = resolve_controller(controller_bits))
    {
        controller->handle_protection_pressed();
    }
}

void dispatch_tech_pressed(std::int64_t controller_bits)
{
    if (Gs1GodotSiteHudController* controller = resolve_controller(controller_bits))
    {
        controller->handle_tech_pressed();
    }
}

template <typename Fn>
void bind_hud_button(Button*& button, Control& owner, const char* name, std::int64_t controller_bits, Fn callback)
{
    if (button == nullptr)
    {
        button = Object::cast_to<Button>(owner.find_child(name, true, false));
    }
    if (button == nullptr)
    {
        return;
    }
    const Callable callable = callable_mp_static(callback).bind(controller_bits);
    if (!button->is_connected("pressed", callable))
    {
        button->connect("pressed", callable);
    }
}
}

void Gs1GodotSiteHudController::cache_ui_references(Control& owner)
{
    if (hud_root_ == nullptr)
    {
        hud_root_ = Object::cast_to<Control>(owner.find_child("SiteHud", true, false));
    }
    if (health_bar_ == nullptr) health_bar_ = Object::cast_to<ProgressBar>(owner.find_child("HudHealthBar", true, false));
    if (hydration_bar_ == nullptr) hydration_bar_ = Object::cast_to<ProgressBar>(owner.find_child("HudHydrationBar", true, false));
    if (nourishment_bar_ == nullptr) nourishment_bar_ = Object::cast_to<ProgressBar>(owner.find_child("HudNourishmentBar", true, false));
    if (energy_bar_ == nullptr) energy_bar_ = Object::cast_to<ProgressBar>(owner.find_child("HudEnergyBar", true, false));
    if (morale_bar_ == nullptr) morale_bar_ = Object::cast_to<ProgressBar>(owner.find_child("HudMoraleBar", true, false));
    if (health_label_ == nullptr) health_label_ = Object::cast_to<Label>(owner.find_child("HudHealthLabel", true, false));
    if (hydration_label_ == nullptr) hydration_label_ = Object::cast_to<Label>(owner.find_child("HudHydrationLabel", true, false));
    if (nourishment_label_ == nullptr) nourishment_label_ = Object::cast_to<Label>(owner.find_child("HudNourishmentLabel", true, false));
    if (energy_label_ == nullptr) energy_label_ = Object::cast_to<Label>(owner.find_child("HudEnergyLabel", true, false));
    if (morale_label_ == nullptr) morale_label_ = Object::cast_to<Label>(owner.find_child("HudMoraleLabel", true, false));
    if (cash_label_ == nullptr) cash_label_ = Object::cast_to<Label>(owner.find_child("HudCashLabel", true, false));
    if (completion_label_ == nullptr) completion_label_ = Object::cast_to<Label>(owner.find_child("HudCompletionLabel", true, false));
    if (phone_badge_label_ == nullptr) phone_badge_label_ = Object::cast_to<Label>(owner.find_child("HudPhoneBadgeLabel", true, false));
    if (task_badge_label_ == nullptr) task_badge_label_ = Object::cast_to<Label>(owner.find_child("HudTaskBadgeLabel", true, false));

    const std::int64_t controller_bits = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this));
    bind_hud_button(phone_button_, owner, "HudPhoneButton", controller_bits, &dispatch_phone_pressed);
    bind_hud_button(pack_button_, owner, "HudPackButton", controller_bits, &dispatch_pack_pressed);
    bind_hud_button(tasks_button_, owner, "HudTasksButton", controller_bits, &dispatch_tasks_pressed);
    bind_hud_button(craft_button_, owner, "HudCraftButton", controller_bits, &dispatch_craft_pressed);
    bind_hud_button(protection_button_, owner, "HudProtectionButton", controller_bits, &dispatch_protection_pressed);
    bind_hud_button(tech_button_, owner, "HudTechButton", controller_bits, &dispatch_tech_pressed);
    refresh_if_needed();
}

void Gs1GodotSiteHudController::set_submit_ui_action_callback(SubmitUiActionFn callback)
{
    submit_ui_action_ = std::move(callback);
}

void Gs1GodotSiteHudController::set_submit_storage_view_callback(SubmitStorageViewFn callback)
{
    submit_storage_view_ = std::move(callback);
}

void Gs1GodotSiteHudController::set_submit_context_request_callback(SubmitContextRequestFn callback)
{
    submit_context_request_ = std::move(callback);
}

void Gs1GodotSiteHudController::set_selected_tile(int tile_x, int tile_y)
{
    if (selected_tile_x_ == tile_x && selected_tile_y_ == tile_y)
    {
        return;
    }
    selected_tile_x_ = tile_x;
    selected_tile_y_ = tile_y;
}

void Gs1GodotSiteHudController::handle_phone_pressed()
{
    if (!submit_ui_action_)
    {
        return;
    }
    const bool open = phone_panel_.has_value() && (phone_panel_->flags & GS1_PHONE_PANEL_FLAG_OPEN) != 0U;
    submit_ui_action_(open ? k_ui_action_close_phone_panel : k_ui_action_set_phone_panel_section, 0, open ? 0 : GS1_PHONE_PANEL_SECTION_HOME, 0);
}

void Gs1GodotSiteHudController::handle_pack_pressed()
{
    if (submit_storage_view_)
    {
        const int storage_id = worker_pack_storage_id();
        if (storage_id != 0)
        {
            submit_storage_view_(storage_id, k_inventory_view_event_open_snapshot);
        }
    }
}

void Gs1GodotSiteHudController::handle_tasks_pressed()
{
    if (submit_ui_action_)
    {
        submit_ui_action_(k_ui_action_set_phone_panel_section, 0, GS1_PHONE_PANEL_SECTION_TASKS, 0);
    }
}

void Gs1GodotSiteHudController::handle_craft_pressed()
{
    if (submit_context_request_)
    {
        submit_context_request_(selected_tile_x_, selected_tile_y_, 0);
    }
}

void Gs1GodotSiteHudController::handle_protection_pressed()
{
    if (submit_ui_action_)
    {
        submit_ui_action_(k_ui_action_open_site_protection_selector, 0, 0, 0);
    }
}

void Gs1GodotSiteHudController::handle_tech_pressed()
{
    if (submit_ui_action_)
    {
        submit_ui_action_(k_ui_action_open_regional_map_tech_tree, 0, 0, 0);
    }
}

bool Gs1GodotSiteHudController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_HUD_STATE:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

void Gs1GodotSiteHudController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
        current_app_state_ = message.payload_as<Gs1EngineMessageSetAppStateData>().app_state;
        if (!site_visible())
        {
            hud_.reset();
            phone_panel_.reset();
            protection_overlay_.reset();
            inventory_storages_.clear();
        }
        break;
    case GS1_ENGINE_MESSAGE_HUD_STATE:
        hud_ = message.payload_as<Gs1EngineMessageHudStateData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
        phone_panel_ = message.payload_as<Gs1EngineMessagePhonePanelData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
        protection_overlay_ = message.payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
        break;
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteSnapshotData>();
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            inventory_storages_.clear();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
    {
        const auto projection = message.payload_as<Gs1EngineMessageInventoryStorageData>();
        const auto found = std::find_if(inventory_storages_.begin(), inventory_storages_.end(), [&](const auto& existing) {
            return existing.storage_id == projection.storage_id;
        });
        if (found != inventory_storages_.end())
        {
            *found = projection;
        }
        else
        {
            inventory_storages_.push_back(projection);
        }
        break;
    }
    default:
        break;
    }
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotSiteHudController::handle_runtime_message_reset()
{
    current_app_state_.reset();
    hud_.reset();
    phone_panel_.reset();
    protection_overlay_.reset();
    inventory_storages_.clear();
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotSiteHudController::refresh_if_needed()
{
    if (!dirty_)
    {
        return;
    }
    if (hud_root_ != nullptr)
    {
        hud_root_->set_visible(site_visible());
    }
    if (!site_visible())
    {
        dirty_ = false;
        return;
    }

    const Gs1RuntimeHudProjection fallback {};
    const Gs1RuntimeHudProjection& hud = hud_.has_value() ? hud_.value() : fallback;
    refresh_meter(health_bar_, health_label_, "Health", hud.player_health);
    refresh_meter(hydration_bar_, hydration_label_, "Water", hud.player_hydration);
    refresh_meter(nourishment_bar_, nourishment_label_, "Food", hud.player_nourishment);
    refresh_meter(energy_bar_, energy_label_, "Energy", hud.player_energy);
    refresh_meter(morale_bar_, morale_label_, "Morale", hud.player_morale);
    if (cash_label_ != nullptr)
    {
        cash_label_->set_text(vformat("$%.2f", hud.current_money));
    }
    if (completion_label_ != nullptr)
    {
        completion_label_->set_text(vformat("Site %d%%", static_cast<int>(std::clamp(hud.site_completion_normalized, 0.0F, 1.0F) * 100.0F)));
    }
    refresh_button_badges();
    dirty_ = false;
}

int Gs1GodotSiteHudController::worker_pack_storage_id() const noexcept
{
    for (const auto& storage : inventory_storages_)
    {
        if (storage.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK)
        {
            return static_cast<int>(storage.storage_id);
        }
    }
    return 0;
}

bool Gs1GodotSiteHudController::site_visible() const noexcept
{
    if (!current_app_state_.has_value())
    {
        return false;
    }
    return current_app_state_.value() >= GS1_APP_STATE_SITE_LOADING &&
        current_app_state_.value() <= GS1_APP_STATE_SITE_RESULT;
}

void Gs1GodotSiteHudController::refresh_meter(ProgressBar* bar, Label* label, const char* name, float value)
{
    const double clamped = std::clamp(static_cast<double>(value), 0.0, 100.0);
    if (bar != nullptr)
    {
        bar->set_value(clamped);
    }
    if (label != nullptr)
    {
        label->set_text(vformat("%s %.0f", name, clamped));
    }
}

void Gs1GodotSiteHudController::refresh_button_badges()
{
    const std::uint32_t flags = phone_panel_.has_value() ? phone_panel_->flags : 0U;
    const bool phone_open = (flags & GS1_PHONE_PANEL_FLAG_OPEN) != 0U;
    if (phone_button_ != nullptr)
    {
        phone_button_->set_pressed(phone_open);
    }
    if (phone_badge_label_ != nullptr)
    {
        phone_badge_label_->set_visible((flags & GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE) != 0U);
    }
    if (task_badge_label_ != nullptr)
    {
        const std::uint32_t ready_count = phone_panel_.has_value() ? phone_panel_->completed_task_count : 0U;
        task_badge_label_->set_visible(ready_count > 0U || (flags & GS1_PHONE_PANEL_FLAG_TASKS_BADGE) != 0U);
        task_badge_label_->set_text(ready_count > 0U ? String::num_int64(ready_count) : String("!"));
    }
    if (pack_button_ != nullptr)
    {
        pack_button_->set_disabled(worker_pack_storage_id() == 0);
    }
    if (protection_button_ != nullptr && protection_overlay_.has_value())
    {
        protection_button_->set_pressed(protection_overlay_->mode != GS1_SITE_PROTECTION_OVERLAY_NONE);
    }
}
