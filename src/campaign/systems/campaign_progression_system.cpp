#include "campaign/systems/campaign_progression_system.h"

#include "content/defs/faction_defs.h"
#include "content/defs/progression_defs.h"
#include "runtime/game_runtime.h"

#include <algorithm>
#include <cassert>

namespace gs1
{
namespace
{
FactionProgressState* find_faction_progress_mut(
    std::vector<FactionProgressState>& faction_progress,
    FactionId faction_id) noexcept
{
    for (auto& state : faction_progress)
    {
        if (state.faction_id == faction_id)
        {
            return &state;
        }
    }

    return nullptr;
}

CampaignTokenBalanceState* find_token_balance_mut(
    CampaignProgressionState& progression,
    std::uint32_t token_kind_id,
    std::uint32_t scope_id) noexcept
{
    for (std::uint16_t index = 0; index < progression.token_balance_count; ++index)
    {
        auto& balance = progression.token_balances[index];
        if (balance.token_kind_id == token_kind_id &&
            balance.scope_id == scope_id)
        {
            return &balance;
        }
    }

    return nullptr;
}

CampaignTokenBalanceState& require_token_balance_mut(
    CampaignProgressionState& progression,
    std::uint32_t token_kind_id,
    std::uint32_t scope_id)
{
    if (auto* existing = find_token_balance_mut(progression, token_kind_id, scope_id);
        existing != nullptr)
    {
        return *existing;
    }

    assert(progression.token_balance_count < k_max_campaign_token_balance_count);
    auto& balance = progression.token_balances[progression.token_balance_count++];
    balance = CampaignTokenBalanceState {
        token_kind_id,
        scope_id,
        0};
    return balance;
}

bool threshold_fired(
    const CampaignProgressionState& progression,
    std::uint32_t threshold_unlock_id) noexcept
{
    for (std::uint16_t index = 0; index < progression.fired_threshold_count; ++index)
    {
        if (progression.fired_threshold_ids[index] == threshold_unlock_id)
        {
            return true;
        }
    }

    return false;
}

void mark_threshold_fired(
    CampaignProgressionState& progression,
    std::uint32_t threshold_unlock_id)
{
    assert(!threshold_fired(progression, threshold_unlock_id));
    assert(progression.fired_threshold_count < k_max_campaign_progression_threshold_count);
    progression.fired_threshold_ids[progression.fired_threshold_count++] = threshold_unlock_id;
}

void emit_threshold_grants(
    RuntimeInvocation& invocation,
    std::uint32_t token_kind_id,
    std::uint32_t scope_id,
    std::int32_t amount)
{
    auto& progression = runtime_invocation_state_ref<RuntimeCampaignProgressionTag>(invocation);
    for (const auto& def : all_threshold_unlock_defs())
    {
        if (def.token_kind_id != token_kind_id ||
            def.scope_id != scope_id ||
            amount < def.threshold_amount ||
            threshold_fired(progression, def.threshold_unlock_id))
        {
            continue;
        }

        mark_threshold_fired(progression, def.threshold_unlock_id);

        invocation.emit_game_message(TargetGrantedMessage {
            def.target_kind_id,
            def.target_id,
            def.scope_id,
            static_cast<std::uint8_t>(def.grant_kind),
            {0U, 0U, 0U}});
    }
}

void apply_total_reputation_delta(
    RuntimeInvocation& invocation,
    std::int32_t delta)
{
    auto& progression = runtime_invocation_state_ref<RuntimeCampaignProgressionTag>(invocation);
    auto& balance = require_token_balance_mut(
        progression,
        k_progression_token_kind_total_reputation,
        0U);
    const std::int64_t next_amount = static_cast<std::int64_t>(balance.amount) + delta;
    assert(next_amount >= 0);
    balance.amount = static_cast<std::int32_t>(std::max<std::int64_t>(0, next_amount));
    progression.total_reputation = balance.amount;
    emit_threshold_grants(
        invocation,
        k_progression_token_kind_total_reputation,
        0U,
        balance.amount);
}

void apply_faction_reputation_delta(
    RuntimeInvocation& invocation,
    FactionId faction_id,
    std::int32_t delta)
{
    auto& progression = runtime_invocation_state_ref<RuntimeCampaignProgressionTag>(invocation);
    auto& balance = require_token_balance_mut(
        progression,
        k_progression_token_kind_faction_reputation,
        faction_id.value);
    const std::int64_t next_amount = static_cast<std::int64_t>(balance.amount) + delta;
    assert(next_amount >= 0);
    balance.amount = static_cast<std::int32_t>(std::max<std::int64_t>(0, next_amount));

    auto* faction_progress =
        find_faction_progress_mut(
            runtime_invocation_state_ref<RuntimeCampaignFactionProgressTag>(invocation),
            faction_id);
    assert(faction_progress != nullptr);
    if (faction_progress == nullptr)
    {
        return;
    }

    faction_progress->faction_reputation = balance.amount;
    if (const auto* faction_def = find_faction_def(faction_id);
        faction_def != nullptr &&
        !faction_progress->has_unlocked_assistant_package &&
        faction_progress->faction_reputation >= faction_def->assistant_unlock_reputation)
    {
        faction_progress->unlocked_assistant_package_id = faction_def->assistant_package_id;
        faction_progress->has_unlocked_assistant_package = faction_def->assistant_package_id != 0U;
    }

    emit_threshold_grants(
        invocation,
        k_progression_token_kind_faction_reputation,
        faction_id.value,
        balance.amount);
}

Gs1Status process_progression_event(
    RuntimeInvocation& invocation,
    const ProgressionEventOccurredMessage& payload)
{
    const auto* def = find_progression_event_def(payload.progression_event_id);
    if (def == nullptr)
    {
        assert(false);
        return GS1_STATUS_NOT_FOUND;
    }

    switch (def->token_kind_id)
    {
    case k_progression_token_kind_total_reputation:
        apply_total_reputation_delta(invocation, payload.amount);
        return GS1_STATUS_OK;

    case k_progression_token_kind_faction_reputation:
        if (payload.scope_id == 0U)
        {
            assert(false);
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        apply_faction_reputation_delta(invocation, FactionId {payload.scope_id}, payload.amount);
        return GS1_STATUS_OK;

    default:
        return GS1_STATUS_OK;
    }
}

Gs1Status process_purchase_entry(
    RuntimeInvocation& invocation,
    const PurchaseEntrySelectedMessage& payload)
{
    const auto* purchase_def = find_purchase_def(payload.purchase_entry_id);
    if (purchase_def == nullptr)
    {
        assert(false);
        return GS1_STATUS_NOT_FOUND;
    }

    if (purchase_def->token_kind_id == k_progression_token_kind_total_reputation)
    {
        auto& progression = runtime_invocation_state_ref<RuntimeCampaignProgressionTag>(invocation);
        auto& balance = require_token_balance_mut(
            progression,
            purchase_def->token_kind_id,
            purchase_def->scope_id);
        assert(balance.amount >= purchase_def->price_amount);
        balance.amount -= purchase_def->price_amount;
        progression.total_reputation = balance.amount;
    }
    else if (purchase_def->token_kind_id == k_progression_token_kind_faction_reputation)
    {
        auto& progression = runtime_invocation_state_ref<RuntimeCampaignProgressionTag>(invocation);
        auto& balance = require_token_balance_mut(
            progression,
            purchase_def->token_kind_id,
            purchase_def->scope_id);
        assert(balance.amount >= purchase_def->price_amount);
        balance.amount -= purchase_def->price_amount;

        auto* faction_progress =
            find_faction_progress_mut(
                runtime_invocation_state_ref<RuntimeCampaignFactionProgressTag>(invocation),
                FactionId {purchase_def->scope_id});
        assert(faction_progress != nullptr);
        if (faction_progress != nullptr)
        {
            faction_progress->faction_reputation = balance.amount;
        }
    }
    else
    {
        return GS1_STATUS_OK;
    }

    invocation.emit_game_message(TargetGrantedMessage {
        purchase_def->target_kind_id,
        purchase_def->target_id,
        purchase_def->scope_id,
        static_cast<std::uint8_t>(purchase_def->grant_kind),
        {0U, 0U, 0U}});
    return GS1_STATUS_OK;
}
}  // namespace

template <>
struct system_state_tags<CampaignProgressionSystem>
{
    using type = type_list<RuntimeCampaignFactionProgressTag, RuntimeCampaignProgressionTag>;
};

const char* CampaignProgressionSystem::name() const noexcept
{
    return "CampaignProgressionSystem";
}

GameMessageSubscriptionSpan CampaignProgressionSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::ProgressionEventOccurred,
        GameMessageType::PurchaseEntrySelected,
        GameMessageType::CampaignReputationAwardRequested,
        GameMessageType::FactionReputationAwardRequested};
    return subscriptions;
}

HostMessageSubscriptionSpan CampaignProgressionSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> CampaignProgressionSystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_FACTION_REPUTATION;
}

std::optional<std::uint32_t> CampaignProgressionSystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}

Gs1Status CampaignProgressionSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    switch (message.type)
    {
    case GameMessageType::ProgressionEventOccurred:
        return handle(invocation, message.payload_as<ProgressionEventOccurredMessage>());

    case GameMessageType::PurchaseEntrySelected:
        return handle(invocation, message.payload_as<PurchaseEntrySelectedMessage>());

    case GameMessageType::CampaignReputationAwardRequested:
        return handle(invocation, message.payload_as<CampaignReputationAwardRequestedMessage>());

    case GameMessageType::FactionReputationAwardRequested:
        return handle(invocation, message.payload_as<FactionReputationAwardRequestedMessage>());

    default:
        return GS1_STATUS_OK;
    }
}

Gs1Status CampaignProgressionSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

Gs1Status CampaignProgressionSystem::handle(
    RuntimeInvocation& invocation,
    const ProgressionEventOccurredMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    return process_progression_event(invocation, message);
}

Gs1Status CampaignProgressionSystem::handle(
    RuntimeInvocation& invocation,
    const PurchaseEntrySelectedMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    return process_purchase_entry(invocation, message);
}

Gs1Status CampaignProgressionSystem::handle(
    RuntimeInvocation& invocation,
    const CampaignReputationAwardRequestedMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    apply_total_reputation_delta(invocation, message.delta);
    return GS1_STATUS_OK;
}

Gs1Status CampaignProgressionSystem::handle(
    RuntimeInvocation& invocation,
    const FactionReputationAwardRequestedMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    apply_faction_reputation_delta(invocation, FactionId {message.faction_id}, message.delta);
    return GS1_STATUS_OK;
}

void CampaignProgressionSystem::run(RuntimeInvocation& invocation)
{
    (void)invocation;
}
}  // namespace gs1
