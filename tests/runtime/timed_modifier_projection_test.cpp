#include "runtime/game_runtime.h"
#include "../system/source/split_state_test_helpers.h"
#include "content/defs/item_defs.h"
#include "site/inventory_storage.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
using gs1::CampaignState;
using gs1::GameRuntime;
using gs1::InventoryItemUseRequestedMessage;
using gs1::SiteRunState;
using gs1::StartNewCampaignMessage;
using gs1::StartSiteAttemptMessage;
}

namespace gs1
{
struct GameRuntimeProjectionTestAccess
{
    [[nodiscard]] static std::optional<CampaignState> campaign(GameRuntime& runtime)
    {
        if (!runtime.state().campaign_core.has_value())
        {
            return std::nullopt;
        }

        return assemble_campaign_state_from_state_sets(runtime.state(), runtime.state_manager());
    }

    [[nodiscard]] static std::optional<SiteRunState> active_site_run(GameRuntime& runtime)
    {
        if (!runtime.state().site_run_meta.has_value())
        {
            return std::nullopt;
        }

        return assemble_site_run_state_from_state_sets(
            runtime.state(),
            runtime.state_manager(),
            runtime.site_world_);
    }

    template <typename Func>
    static void mutate_active_site_run(GameRuntime& runtime, Func&& func)
    {
        auto site_run = active_site_run(runtime);
        assert(site_run.has_value());
        func(site_run.value());
        write_site_run_state_to_state_sets(site_run.value(), runtime.state(), runtime.state_manager());
        runtime.site_world_ = site_run->site_world;
    }
};
}  // namespace gs1

namespace
{
StartNewCampaignMessage make_start_campaign_message()
{
    return StartNewCampaignMessage {42ULL, 30U};
}

StartSiteAttemptMessage make_start_site_attempt_message(std::uint32_t site_id)
{
    return StartSiteAttemptMessage {site_id};
}

InventoryItemUseRequestedMessage make_inventory_use_message(
    std::uint32_t item_id,
    std::uint32_t quantity,
    std::uint32_t storage_id,
    std::uint32_t slot_index)
{
    return InventoryItemUseRequestedMessage {
        item_id,
        storage_id,
        static_cast<std::uint16_t>(quantity),
        static_cast<std::uint16_t>(slot_index)};
}

std::vector<Gs1RuntimeMessage> drain_runtime_messages(GameRuntime& runtime)
{
    std::vector<Gs1RuntimeMessage> messages {};
    Gs1RuntimeMessage message {};
    while (runtime.pop_runtime_message(message) == GS1_STATUS_OK)
    {
        messages.push_back(message);
    }
    return messages;
}

Gs1GameStateView read_view(GameRuntime& runtime)
{
    Gs1GameStateView view {};
    view.struct_size = sizeof(Gs1GameStateView);
    assert(runtime.get_game_state_view(view) == GS1_STATUS_OK);
    return view;
}

void run_phase1(GameRuntime& runtime, double real_delta_seconds, Gs1Phase1Result& out_result)
{
    Gs1Phase1Request request {};
    request.struct_size = sizeof(Gs1Phase1Request);
    request.real_delta_seconds = real_delta_seconds;

    out_result = {};
    assert(runtime.run_phase1(request, out_result) == GS1_STATUS_OK);
}

void run_phase2(GameRuntime& runtime, Gs1Phase2Result& out_result)
{
    Gs1Phase2Request request {};
    request.struct_size = sizeof(Gs1Phase2Request);

    out_result = {};
    assert(runtime.run_phase2(request, out_result) == GS1_STATUS_OK);
}

const Gs1SiteModifierView* find_site_modifier_view(
    const Gs1GameStateView& view,
    std::uint32_t modifier_id)
{
    if (view.active_site == nullptr || view.active_site->active_modifiers == nullptr)
    {
        return nullptr;
    }

    for (std::uint32_t index = 0U; index < view.active_site->active_modifier_count; ++index)
    {
        const auto& modifier = view.active_site->active_modifiers[index];
        if (modifier.modifier_id == modifier_id)
        {
            return &modifier;
        }
    }

    return nullptr;
}

std::uint16_t projected_remaining_game_hours(const Gs1SiteModifierView& modifier)
{
    constexpr double epsilon = 0.0001;
    if (modifier.remaining_world_minutes <= epsilon)
    {
        return 0U;
    }

    return static_cast<std::uint16_t>(std::ceil(
        std::max(0.0, modifier.remaining_world_minutes - epsilon) / 60.0));
}

void bootstrap_site_one(GameRuntime& runtime)
{
    assert(runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto campaign = gs1::GameRuntimeProjectionTestAccess::campaign(runtime);
    assert(campaign.has_value());
    assert(!campaign->sites.empty());

    const auto site_id = campaign->sites.front().site_id.value;
    assert(runtime.handle_message(make_start_site_attempt_message(site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).has_value());

    assert(runtime.submit_site_scene_ready() == GS1_STATUS_OK);
    Gs1Phase2Result ready_result {};
    run_phase2(runtime, ready_result);
}

void timed_modifier_projection_only_republishes_on_game_hour_boundaries()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};
    bootstrap_site_one(runtime);

    gs1::GameRuntimeProjectionTestAccess::mutate_active_site_run(
        runtime,
        [](SiteRunState& site_run)
        {
            gs1::inventory_storage::initialize_site_inventory_storage(site_run);
            const auto worker_pack = gs1::inventory_storage::worker_pack_container(site_run);
            assert(worker_pack.is_valid());
            const auto remaining = gs1::inventory_storage::add_item_to_container(
                site_run,
                worker_pack,
                gs1::ItemId {gs1::k_item_focus_tonic},
                1U,
                1.0f,
                1.0f);
            assert(remaining == 0U);
        });
    drain_runtime_messages(runtime);

    assert(runtime.handle_message(make_inventory_use_message(
               gs1::k_item_focus_tonic,
               1U,
               gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime)->inventory.worker_pack_storage_id,
               0U)) == GS1_STATUS_OK);

    const auto activation_view = read_view(runtime);
    const auto* activation_modifier = find_site_modifier_view(activation_view, 3003U);
    assert(activation_modifier != nullptr);
    assert(projected_remaining_game_hours(*activation_modifier) == 16U);

    Gs1Phase1Result first_tick_result {};
    run_phase1(runtime, 60.0, first_tick_result);
    const auto first_tick_view = read_view(runtime);
    const auto* first_tick_modifier = find_site_modifier_view(first_tick_view, 3003U);
    assert(first_tick_modifier != nullptr);
    assert(projected_remaining_game_hours(*first_tick_modifier) == 16U);

    Gs1Phase1Result second_tick_result {};
    run_phase1(runtime, 60.0, second_tick_result);
    const auto second_tick_view = read_view(runtime);
    const auto* second_tick_modifier = find_site_modifier_view(second_tick_view, 3003U);
    assert(second_tick_modifier != nullptr);
    assert(projected_remaining_game_hours(*second_tick_modifier) == 15U);
}
}  // namespace

int main()
{
    timed_modifier_projection_only_republishes_on_game_hour_boundaries();
    return 0;
}


