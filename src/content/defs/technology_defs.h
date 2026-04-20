#pragma once

#include "content/defs/faction_defs.h"
#include "support/id_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace gs1
{
enum class TechnologyEntryKind : std::uint8_t
{
    BaseTech = 0,
    Amplification = 1
};

enum class TechnologySignatureFamily : std::uint8_t
{
    None = 0,
    Recipe = 1,
    Plant = 2,
    Device = 3
};

enum class TechnologyUpdateFamily : std::uint8_t
{
    None = 0,
    RecoveryBlend = 1,
    EnduranceBlend = 2,
    ComfortBlend = 3,
    EstablishmentUpdate = 4,
    ProtectionUpdate = 5,
    RehabUpdate = 6,
    CalibrationUpdate = 7,
    HardeningUpdate = 8,
    FieldEnvelopeUpdate = 9
};

struct TechnologyTierDef final
{
    FactionId faction_id {};
    std::uint8_t tier_index {0};
    std::uint8_t reserved0[3] {};
    std::int32_t base_reputation_cost {0};
    std::string_view display_name {};
};

struct TechnologyNodeDef final
{
    TechNodeId tech_node_id {};
    FactionId faction_id {};
    std::uint8_t tier_index {0};
    std::uint8_t base_slot_index {0};
    TechnologyEntryKind entry_kind {TechnologyEntryKind::BaseTech};
    std::uint8_t amplification_choice_index {0};
    TechNodeId parent_base_tech_id {};
    TechnologySignatureFamily signature_family {TechnologySignatureFamily::None};
    TechnologyUpdateFamily update_family {TechnologyUpdateFamily::None};
    std::uint32_t linked_unlockable_id {0};
    ModifierId linked_modifier_id {};
    std::string_view unlockable_display_name {};
    std::string_view modifier_display_name {};
    std::string_view display_name {};
    std::string_view description {};
};

struct TechnologyBaseAuthoring final
{
    std::string_view display_name {};
    std::string_view unlockable_display_name {};
    std::string_view description {};
};

struct TechnologyAmplificationAuthoring final
{
    std::string_view display_name {};
    TechnologyUpdateFamily update_family {TechnologyUpdateFamily::None};
};

inline constexpr std::uint8_t k_faction_tech_tier_count = 4U;
inline constexpr std::uint8_t k_faction_tech_base_choices_per_tier = 3U;
inline constexpr std::uint8_t k_faction_tech_amplification_choices_per_base = 2U;
inline constexpr float k_tier_additional_purchase_cost_multiplier = 1.2f;
inline constexpr std::array<std::int32_t, k_faction_tech_tier_count> k_faction_tech_tier_base_costs {{
    10,
    18,
    28,
    42,
}};

[[nodiscard]] constexpr std::uint32_t technology_family_base(FactionId faction_id) noexcept
{
    return faction_id.value * 1000U;
}

[[nodiscard]] constexpr std::uint32_t technology_base_node_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index) noexcept
{
    return technology_family_base(faction_id) +
        static_cast<std::uint32_t>(tier_index - 1U) * 100U +
        static_cast<std::uint32_t>(base_slot_index + 1U);
}

[[nodiscard]] constexpr std::uint32_t technology_amplification_node_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index,
    std::uint8_t amplification_choice_index) noexcept
{
    return technology_family_base(faction_id) +
        static_cast<std::uint32_t>(tier_index - 1U) * 100U +
        static_cast<std::uint32_t>(base_slot_index + 1U) * 10U +
        static_cast<std::uint32_t>(amplification_choice_index + 1U);
}

[[nodiscard]] constexpr std::uint32_t technology_unlockable_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index) noexcept
{
    return technology_family_base(faction_id) +
        500U +
        static_cast<std::uint32_t>(tier_index - 1U) * 10U +
        static_cast<std::uint32_t>(base_slot_index + 1U);
}

[[nodiscard]] constexpr std::uint32_t technology_modifier_id(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index,
    std::uint8_t amplification_choice_index) noexcept
{
    return technology_family_base(faction_id) +
        800U +
        static_cast<std::uint32_t>(tier_index - 1U) * 10U +
        static_cast<std::uint32_t>(base_slot_index + 1U) * 2U +
        static_cast<std::uint32_t>(amplification_choice_index);
}

inline constexpr std::uint32_t k_tech_node_village_t1_field_rations =
    technology_base_node_id(FactionId {k_faction_village_committee}, 1U, 0U);
inline constexpr std::uint32_t k_tech_node_village_t1_shift_meals =
    technology_base_node_id(FactionId {k_faction_village_committee}, 1U, 1U);
inline constexpr std::uint32_t k_tech_node_village_t1_supply_runners =
    technology_base_node_id(FactionId {k_faction_village_committee}, 1U, 2U);
inline constexpr std::uint32_t k_tech_node_village_t1_dust_kettle_tea =
    k_tech_node_village_t1_supply_runners;
inline constexpr std::uint32_t k_tech_node_village_t1_field_rations_amp_recovery =
    technology_amplification_node_id(FactionId {k_faction_village_committee}, 1U, 0U, 0U);
inline constexpr std::uint32_t k_tech_node_village_t1_field_rations_amp_comfort =
    technology_amplification_node_id(FactionId {k_faction_village_committee}, 1U, 0U, 1U);
inline constexpr std::uint32_t k_tech_node_village_t1_field_rations_amp_endurance =
    k_tech_node_village_t1_field_rations_amp_comfort;
inline constexpr std::uint32_t k_tech_node_village_t2_stove_network =
    technology_base_node_id(FactionId {k_faction_village_committee}, 2U, 0U);
inline constexpr std::uint32_t k_tech_node_village_t2_repair_broth =
    k_tech_node_village_t2_stove_network;

[[nodiscard]] constexpr TechnologySignatureFamily signature_family_for_faction(FactionId faction_id) noexcept
{
    switch (faction_id.value)
    {
    case k_faction_village_committee:
        return TechnologySignatureFamily::Recipe;
    case k_faction_forestry_bureau:
        return TechnologySignatureFamily::Plant;
    case k_faction_agricultural_university:
        return TechnologySignatureFamily::Device;
    default:
        return TechnologySignatureFamily::None;
    }
}

[[nodiscard]] constexpr TechnologyTierDef make_tier(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::int32_t base_reputation_cost,
    std::string_view display_name) noexcept
{
    return TechnologyTierDef {
        faction_id,
        tier_index,
        {0U, 0U, 0U},
        base_reputation_cost,
        display_name};
}

[[nodiscard]] constexpr TechnologyNodeDef make_base_tech(
    std::uint32_t tech_node_id,
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index,
    TechnologySignatureFamily signature_family,
    std::uint32_t linked_unlockable_id,
    std::string_view unlockable_display_name,
    std::string_view display_name,
    std::string_view description) noexcept
{
    return TechnologyNodeDef {
        TechNodeId {tech_node_id},
        faction_id,
        tier_index,
        base_slot_index,
        TechnologyEntryKind::BaseTech,
        0U,
        TechNodeId {},
        signature_family,
        TechnologyUpdateFamily::None,
        linked_unlockable_id,
        ModifierId {},
        unlockable_display_name,
        {},
        display_name,
        description};
}

[[nodiscard]] constexpr TechnologyNodeDef make_amplification(
    std::uint32_t tech_node_id,
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index,
    std::uint8_t amplification_choice_index,
    std::uint32_t parent_base_tech_id,
    TechnologySignatureFamily signature_family,
    TechnologyUpdateFamily update_family,
    std::uint32_t linked_unlockable_id,
    std::uint32_t linked_modifier_id,
    std::string_view unlockable_display_name,
    std::string_view modifier_display_name,
    std::string_view display_name,
    std::string_view description) noexcept
{
    return TechnologyNodeDef {
        TechNodeId {tech_node_id},
        faction_id,
        tier_index,
        base_slot_index,
        TechnologyEntryKind::Amplification,
        amplification_choice_index,
        TechNodeId {parent_base_tech_id},
        signature_family,
        update_family,
        linked_unlockable_id,
        ModifierId {linked_modifier_id},
        unlockable_display_name,
        modifier_display_name,
        display_name,
        description};
}

inline constexpr std::array<std::array<std::string_view, k_faction_tech_tier_count>, 3>
    k_faction_tier_display_names {{
        {{"Starter Canteen", "Roadside Kitchen", "Camp Coordination", "Relief Network"}},
        {{"Seedling Rescue", "Soil Holding", "Recovery Belts", "Refuge Canopy"}},
        {{"Field Instruments", "Support Devices", "Control Networks", "Research Operations"}},
    }};

inline constexpr std::array<std::array<std::array<TechnologyBaseAuthoring, 3>, k_faction_tech_tier_count>, 3>
    k_faction_base_authoring {{
        {{
            {{
                {"Field Rations", "Field Rations Recipe",
                 "Unlocks a cheap staple ration recipe for early site recovery and dependable day-one nourishment."},
                {"Shift Meals", "Shift Meals Recipe",
                 "Unlocks a sturdier cooked meal recipe built for longer work shifts and fewer output crashes."},
                {"Dust-Kettle Tea", "Dust-Kettle Tea Recipe",
                 "Unlocks a dust-soothing tea recipe that steadies morale and keeps rough weather from dragging the camp down."},
            }},
            {{
                {"Repair Broth", "Repair Broth Recipe",
                 "Unlocks a hot recovery broth recipe meant for post-storm cleanup, injury recovery, and hard repair days."},
                {"Route Jerky", "Route Jerky Recipe",
                 "Unlocks a portable jerky recipe that keeps long hauling routes productive without repeated return trips."},
                {"Cool Jar Pickles", "Cool Jar Pickles Recipe",
                 "Unlocks a preserved pickle recipe that carries well, stays usable longer, and softens camp fatigue."},
            }},
            {{
                {"Campfire Congee", "Campfire Congee Recipe",
                 "Unlocks a flexible comfort meal recipe that turns mixed supplies into reliable recovery after setbacks."},
                {"Convoy Noodle Pot", "Convoy Noodle Pot Recipe",
                 "Unlocks a high-yield shared meal recipe tuned for contractor pushes, build bursts, and tempo spikes."},
                {"Comfort Herb Brew", "Comfort Herb Brew Recipe",
                 "Unlocks a restorative camp brew recipe focused on morale recovery and steadier routine output."},
            }},
            {{
                {"Relief Feast Packs", "Relief Feast Pack Recipe",
                 "Unlocks a high-end packaged meal recipe for major program pushes, long deployments, and trusted support crews."},
                {"Stormwatch Stew", "Stormwatch Stew Recipe",
                 "Unlocks a resilient stew recipe meant to rebuild the worker after severe weather windows and overnight strain."},
                {"Lantern Rest Tonic", "Lantern Rest Tonic Recipe",
                 "Unlocks an advanced tonic recipe that protects morale, steadies recovery, and keeps late-campaign sites manageable."},
            }},
        }},
        {{
            {{
                {"Wind Reed Lines", "Wind Reed",
                 "Unlocks Wind Reed as a dependable early protection plant for exposed approaches and storm-facing lanes."},
                {"Root Binder Beds", "Root Binder",
                 "Unlocks Root Binder as a groundwork plant that prepares difficult soil for denser follow-up restoration."},
                {"Dew Grass Patches", "Dew Grass",
                 "Unlocks Dew Grass as a low-demand bridge plant for dry expansion lanes and soft soil recovery."},
            }},
            {{
                {"Salt Bean Rows", "Salt Bean",
                 "Unlocks Salt Bean as the branch's main early salinity-rehab crop with modest output upside."},
                {"Shade Cactus Belts", "Shade Cactus",
                 "Unlocks Shade Cactus as a heat-softening support plant for camp edges and worker safety pockets."},
                {"Thorn Shrub Hedges", "Thorn Shrub",
                 "Unlocks Thorn Shrub as a tougher perimeter plant that mixes protection with modest site yield."},
            }},
            {{
                {"Medicinal Sage Plots", "Medicinal Sage",
                 "Unlocks Medicinal Sage as a stable-zone support plant that improves comfort and adds herb value."},
                {"Sand Willow Anchors", "Sand Willow",
                 "Unlocks Sand Willow as a major refuge-core plant for harsh zones that can finally be held."},
                {"Sunfruit Vine Trellises", "Sunfruit Vine",
                 "Unlocks Sunfruit Vine as a greedy payoff crop that rewards prepared shelter and fertility lanes."},
            }},
            {{
                {"Dune Clover Plots", "Dune Clover",
                 "Unlocks Dune Clover as a dense rehab plant that captures scarce moisture and helps tired ground recover."},
                {"Resin Thorn Groves", "Resin Thorn",
                 "Unlocks Resin Thorn as a late tougher barrier crop with stronger edge holding and practical yield."},
                {"Shelter Tamarisk", "Shelter Tamarisk",
                 "Unlocks Shelter Tamarisk as a late canopy anchor that turns one hard-won patch into a durable refuge."},
            }},
        }},
        {{
            {{
                {"Water Tank Network", "Water Tank",
                 "Unlocks Water Tank deployment as a reliable on-site reserve for watering flow and linked irrigation."},
                {"Drip Irrigator Lines", "Drip Irrigator",
                 "Unlocks Drip Irrigator deployment for sustained local moisture support around fragile plant clusters."},
                {"Survey Stakes", "Survey Stakes",
                 "Unlocks Survey Stakes as a simple technical unlockable that improves risk reading before the site becomes fully instrumented."},
            }},
            {{
                {"Solar Array Frames", "Solar Array",
                 "Unlocks Solar Array deployment for device efficiency support, heat relief, and light surplus utility."},
                {"Forecast Mast", "Forecast Mast",
                 "Unlocks a Forecast Mast unlockable that improves Heat, Wind, and Dust warning clarity before dangerous windows."},
                {"Calibration Bench", "Calibration Bench",
                 "Unlocks a Calibration Bench unlockable that keeps technical kits tuned and improves follow-up device reliability."},
            }},
            {{
                {"Dust Screen Tower", "Dust Screen Tower",
                 "Unlocks a Dust Screen Tower unlockable for exposed support lines that need harder storm shielding."},
                {"Microclimate Relay", "Microclimate Relay",
                 "Unlocks a Microclimate Relay unlockable that extends technical support effects deeper into active restoration pockets."},
                {"Precision Valve Rack", "Precision Valve Rack",
                 "Unlocks a Precision Valve Rack unlockable for finer irrigation control and tighter water-device coordination."},
            }},
            {{
                {"Thermal Shade Grid", "Thermal Shade Grid",
                 "Unlocks a Thermal Shade Grid unlockable that builds a stronger heat-softening support envelope for advanced sites."},
                {"Storm Buffer Array", "Storm Buffer Array",
                 "Unlocks a Storm Buffer Array unlockable that stabilizes fragile technical infrastructure through severe hazard windows."},
                {"Control Caravan", "Control Caravan",
                 "Unlocks a Control Caravan unlockable that acts as a late campaign command node for high-precision site optimization."},
            }},
        }},
    }};

inline constexpr std::array<
    std::array<std::array<std::array<TechnologyAmplificationAuthoring, 2>, 3>, k_faction_tech_tier_count>,
    3>
    k_faction_amplification_authoring {{
        {{
            {{{
                TechnologyAmplificationAuthoring {"Field Rations: Recovery Blend", TechnologyUpdateFamily::RecoveryBlend},
                TechnologyAmplificationAuthoring {"Field Rations: Endurance Blend", TechnologyUpdateFamily::EnduranceBlend},
            }, {
                TechnologyAmplificationAuthoring {"Shift Meals: Endurance Blend", TechnologyUpdateFamily::EnduranceBlend},
                TechnologyAmplificationAuthoring {"Shift Meals: Comfort Blend", TechnologyUpdateFamily::ComfortBlend},
            }, {
                TechnologyAmplificationAuthoring {"Dust-Kettle Tea: Recovery Blend", TechnologyUpdateFamily::RecoveryBlend},
                TechnologyAmplificationAuthoring {"Dust-Kettle Tea: Comfort Blend", TechnologyUpdateFamily::ComfortBlend},
            }}},
            {{{
                TechnologyAmplificationAuthoring {"Repair Broth: Recovery Blend", TechnologyUpdateFamily::RecoveryBlend},
                TechnologyAmplificationAuthoring {"Repair Broth: Comfort Blend", TechnologyUpdateFamily::ComfortBlend},
            }, {
                TechnologyAmplificationAuthoring {"Route Jerky: Endurance Blend", TechnologyUpdateFamily::EnduranceBlend},
                TechnologyAmplificationAuthoring {"Route Jerky: Recovery Blend", TechnologyUpdateFamily::RecoveryBlend},
            }, {
                TechnologyAmplificationAuthoring {"Cool Jar Pickles: Comfort Blend", TechnologyUpdateFamily::ComfortBlend},
                TechnologyAmplificationAuthoring {"Cool Jar Pickles: Endurance Blend", TechnologyUpdateFamily::EnduranceBlend},
            }}},
            {{{
                TechnologyAmplificationAuthoring {"Campfire Congee: Recovery Blend", TechnologyUpdateFamily::RecoveryBlend},
                TechnologyAmplificationAuthoring {"Campfire Congee: Comfort Blend", TechnologyUpdateFamily::ComfortBlend},
            }, {
                TechnologyAmplificationAuthoring {"Convoy Noodle Pot: Endurance Blend", TechnologyUpdateFamily::EnduranceBlend},
                TechnologyAmplificationAuthoring {"Convoy Noodle Pot: Recovery Blend", TechnologyUpdateFamily::RecoveryBlend},
            }, {
                TechnologyAmplificationAuthoring {"Comfort Herb Brew: Comfort Blend", TechnologyUpdateFamily::ComfortBlend},
                TechnologyAmplificationAuthoring {"Comfort Herb Brew: Recovery Blend", TechnologyUpdateFamily::RecoveryBlend},
            }}},
            {{{
                TechnologyAmplificationAuthoring {"Relief Feast Packs: Endurance Blend", TechnologyUpdateFamily::EnduranceBlend},
                TechnologyAmplificationAuthoring {"Relief Feast Packs: Comfort Blend", TechnologyUpdateFamily::ComfortBlend},
            }, {
                TechnologyAmplificationAuthoring {"Stormwatch Stew: Recovery Blend", TechnologyUpdateFamily::RecoveryBlend},
                TechnologyAmplificationAuthoring {"Stormwatch Stew: Endurance Blend", TechnologyUpdateFamily::EnduranceBlend},
            }, {
                TechnologyAmplificationAuthoring {"Lantern Rest Tonic: Comfort Blend", TechnologyUpdateFamily::ComfortBlend},
                TechnologyAmplificationAuthoring {"Lantern Rest Tonic: Recovery Blend", TechnologyUpdateFamily::RecoveryBlend},
            }}},
        }},
        {{
            {{{
                TechnologyAmplificationAuthoring {"Wind Reed Lines: Establishment Update", TechnologyUpdateFamily::EstablishmentUpdate},
                TechnologyAmplificationAuthoring {"Wind Reed Lines: Protection Update", TechnologyUpdateFamily::ProtectionUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Root Binder Beds: Rehab Update", TechnologyUpdateFamily::RehabUpdate},
                TechnologyAmplificationAuthoring {"Root Binder Beds: Protection Update", TechnologyUpdateFamily::ProtectionUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Dew Grass Patches: Establishment Update", TechnologyUpdateFamily::EstablishmentUpdate},
                TechnologyAmplificationAuthoring {"Dew Grass Patches: Rehab Update", TechnologyUpdateFamily::RehabUpdate},
            }}},
            {{{
                TechnologyAmplificationAuthoring {"Salt Bean Rows: Establishment Update", TechnologyUpdateFamily::EstablishmentUpdate},
                TechnologyAmplificationAuthoring {"Salt Bean Rows: Rehab Update", TechnologyUpdateFamily::RehabUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Shade Cactus Belts: Protection Update", TechnologyUpdateFamily::ProtectionUpdate},
                TechnologyAmplificationAuthoring {"Shade Cactus Belts: Establishment Update", TechnologyUpdateFamily::EstablishmentUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Thorn Shrub Hedges: Protection Update", TechnologyUpdateFamily::ProtectionUpdate},
                TechnologyAmplificationAuthoring {"Thorn Shrub Hedges: Rehab Update", TechnologyUpdateFamily::RehabUpdate},
            }}},
            {{{
                TechnologyAmplificationAuthoring {"Medicinal Sage Plots: Establishment Update", TechnologyUpdateFamily::EstablishmentUpdate},
                TechnologyAmplificationAuthoring {"Medicinal Sage Plots: Protection Update", TechnologyUpdateFamily::ProtectionUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Sand Willow Anchors: Protection Update", TechnologyUpdateFamily::ProtectionUpdate},
                TechnologyAmplificationAuthoring {"Sand Willow Anchors: Rehab Update", TechnologyUpdateFamily::RehabUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Sunfruit Vine Trellises: Establishment Update", TechnologyUpdateFamily::EstablishmentUpdate},
                TechnologyAmplificationAuthoring {"Sunfruit Vine Trellises: Protection Update", TechnologyUpdateFamily::ProtectionUpdate},
            }}},
            {{{
                TechnologyAmplificationAuthoring {"Dune Clover Plots: Establishment Update", TechnologyUpdateFamily::EstablishmentUpdate},
                TechnologyAmplificationAuthoring {"Dune Clover Plots: Rehab Update", TechnologyUpdateFamily::RehabUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Resin Thorn Groves: Protection Update", TechnologyUpdateFamily::ProtectionUpdate},
                TechnologyAmplificationAuthoring {"Resin Thorn Groves: Establishment Update", TechnologyUpdateFamily::EstablishmentUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Shelter Tamarisk: Protection Update", TechnologyUpdateFamily::ProtectionUpdate},
                TechnologyAmplificationAuthoring {"Shelter Tamarisk: Rehab Update", TechnologyUpdateFamily::RehabUpdate},
            }}},
        }},
        {{
            {{{
                TechnologyAmplificationAuthoring {"Water Tank Network: Hardening Update", TechnologyUpdateFamily::HardeningUpdate},
                TechnologyAmplificationAuthoring {"Water Tank Network: Field Envelope Update", TechnologyUpdateFamily::FieldEnvelopeUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Drip Irrigator Lines: Calibration Update", TechnologyUpdateFamily::CalibrationUpdate},
                TechnologyAmplificationAuthoring {"Drip Irrigator Lines: Field Envelope Update", TechnologyUpdateFamily::FieldEnvelopeUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Survey Stakes: Calibration Update", TechnologyUpdateFamily::CalibrationUpdate},
                TechnologyAmplificationAuthoring {"Survey Stakes: Hardening Update", TechnologyUpdateFamily::HardeningUpdate},
            }}},
            {{{
                TechnologyAmplificationAuthoring {"Solar Array Frames: Calibration Update", TechnologyUpdateFamily::CalibrationUpdate},
                TechnologyAmplificationAuthoring {"Solar Array Frames: Field Envelope Update", TechnologyUpdateFamily::FieldEnvelopeUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Forecast Mast: Calibration Update", TechnologyUpdateFamily::CalibrationUpdate},
                TechnologyAmplificationAuthoring {"Forecast Mast: Hardening Update", TechnologyUpdateFamily::HardeningUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Calibration Bench: Calibration Update", TechnologyUpdateFamily::CalibrationUpdate},
                TechnologyAmplificationAuthoring {"Calibration Bench: Hardening Update", TechnologyUpdateFamily::HardeningUpdate},
            }}},
            {{{
                TechnologyAmplificationAuthoring {"Dust Screen Tower: Hardening Update", TechnologyUpdateFamily::HardeningUpdate},
                TechnologyAmplificationAuthoring {"Dust Screen Tower: Field Envelope Update", TechnologyUpdateFamily::FieldEnvelopeUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Microclimate Relay: Calibration Update", TechnologyUpdateFamily::CalibrationUpdate},
                TechnologyAmplificationAuthoring {"Microclimate Relay: Field Envelope Update", TechnologyUpdateFamily::FieldEnvelopeUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Precision Valve Rack: Calibration Update", TechnologyUpdateFamily::CalibrationUpdate},
                TechnologyAmplificationAuthoring {"Precision Valve Rack: Hardening Update", TechnologyUpdateFamily::HardeningUpdate},
            }}},
            {{{
                TechnologyAmplificationAuthoring {"Thermal Shade Grid: Field Envelope Update", TechnologyUpdateFamily::FieldEnvelopeUpdate},
                TechnologyAmplificationAuthoring {"Thermal Shade Grid: Hardening Update", TechnologyUpdateFamily::HardeningUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Storm Buffer Array: Hardening Update", TechnologyUpdateFamily::HardeningUpdate},
                TechnologyAmplificationAuthoring {"Storm Buffer Array: Field Envelope Update", TechnologyUpdateFamily::FieldEnvelopeUpdate},
            }, {
                TechnologyAmplificationAuthoring {"Control Caravan: Calibration Update", TechnologyUpdateFamily::CalibrationUpdate},
                TechnologyAmplificationAuthoring {"Control Caravan: Hardening Update", TechnologyUpdateFamily::HardeningUpdate},
            }}},
        }},
    }};

[[nodiscard]] constexpr std::size_t faction_authoring_index(FactionId faction_id) noexcept
{
    return faction_id.value <= 1U ? 0U : static_cast<std::size_t>(faction_id.value - 1U);
}

[[nodiscard]] constexpr std::string_view technology_update_family_display_name(
    TechnologyUpdateFamily update_family) noexcept
{
    switch (update_family)
    {
    case TechnologyUpdateFamily::RecoveryBlend:
        return "Recovery Blend";
    case TechnologyUpdateFamily::EnduranceBlend:
        return "Endurance Blend";
    case TechnologyUpdateFamily::ComfortBlend:
        return "Comfort Blend";
    case TechnologyUpdateFamily::EstablishmentUpdate:
        return "Establishment Update";
    case TechnologyUpdateFamily::ProtectionUpdate:
        return "Protection Update";
    case TechnologyUpdateFamily::RehabUpdate:
        return "Rehab Update";
    case TechnologyUpdateFamily::CalibrationUpdate:
        return "Calibration Update";
    case TechnologyUpdateFamily::HardeningUpdate:
        return "Hardening Update";
    case TechnologyUpdateFamily::FieldEnvelopeUpdate:
        return "Field Envelope Update";
    default:
        return {};
    }
}

[[nodiscard]] constexpr std::string_view amplification_description(
    TechnologySignatureFamily signature_family,
    TechnologyUpdateFamily update_family) noexcept
{
    if (signature_family == TechnologySignatureFamily::Recipe)
    {
        switch (update_family)
        {
        case TechnologyUpdateFamily::RecoveryBlend:
            return "Applies a recovery-focused modifier to the linked recipe, improving playerHealth, playerHydration, and playerNourishment restoration.";
        case TechnologyUpdateFamily::EnduranceBlend:
            return "Applies an endurance-focused modifier to the linked recipe, improving playerEnergyCap, playerEnergy, and playerWorkEfficiency sustain.";
        case TechnologyUpdateFamily::ComfortBlend:
            return "Applies a comfort-focused modifier to the linked recipe, improving playerMorale support and itemFreshness resilience.";
        default:
            return {};
        }
    }

    if (signature_family == TechnologySignatureFamily::Plant)
    {
        switch (update_family)
        {
        case TechnologyUpdateFamily::EstablishmentUpdate:
            return "Applies an establishment-focused modifier to the linked plant, improving growthPressure, plantDensityModifier, and salinityDensityCap support.";
        case TechnologyUpdateFamily::ProtectionUpdate:
            return "Applies a protection-focused modifier to the linked plant, improving windProtectionPower, windProtectionRange, heatProtectionPower, dustProtectionPower, and auraSize.";
        case TechnologyUpdateFamily::RehabUpdate:
            return "Applies a rehabilitation-focused modifier to the linked plant, improving fertilityImprovePower and salinityReductionPower.";
        default:
            return {};
        }
    }

    if (signature_family == TechnologySignatureFamily::Device)
    {
        switch (update_family)
        {
        case TechnologyUpdateFamily::CalibrationUpdate:
            return "Applies a calibration-focused modifier to the linked device unlockable, improving deviceEfficiency and its primary technical output values.";
        case TechnologyUpdateFamily::HardeningUpdate:
            return "Applies a hardening-focused modifier to the linked device unlockable, improving deviceIntegrity and reducing burialSensitivity.";
        case TechnologyUpdateFamily::FieldEnvelopeUpdate:
            return "Applies a field-envelope modifier to the linked device unlockable, improving windProtectionPower, windProtectionRange, and nearby support values.";
        default:
            return {};
        }
    }

    return {};
}

[[nodiscard]] constexpr std::string_view technology_tier_display_name(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept
{
    return k_faction_tier_display_names[faction_authoring_index(faction_id)][tier_index - 1U];
}

[[nodiscard]] constexpr const TechnologyBaseAuthoring& technology_base_authoring(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index) noexcept
{
    return k_faction_base_authoring[faction_authoring_index(faction_id)][tier_index - 1U][base_slot_index];
}

[[nodiscard]] constexpr const TechnologyAmplificationAuthoring& technology_amplification_authoring(
    FactionId faction_id,
    std::uint8_t tier_index,
    std::uint8_t base_slot_index,
    std::uint8_t amplification_choice_index) noexcept
{
    return k_faction_amplification_authoring[faction_authoring_index(faction_id)][tier_index - 1U][base_slot_index]
        [amplification_choice_index];
}

inline constexpr std::size_t k_prototype_technology_tier_count =
    k_prototype_faction_defs.size() * static_cast<std::size_t>(k_faction_tech_tier_count);
inline constexpr std::size_t k_prototype_technology_node_count =
    k_prototype_faction_defs.size() *
    static_cast<std::size_t>(k_faction_tech_tier_count) *
    static_cast<std::size_t>(k_faction_tech_base_choices_per_tier) *
    (1U + static_cast<std::size_t>(k_faction_tech_amplification_choices_per_base));

inline const auto k_prototype_technology_tier_defs = []()
{
    std::array<TechnologyTierDef, k_prototype_technology_tier_count> tier_defs {};
    std::size_t index = 0U;
    for (const auto& faction_def : k_prototype_faction_defs)
    {
        for (std::uint8_t tier_index = 1U; tier_index <= k_faction_tech_tier_count; ++tier_index)
        {
            tier_defs[index++] = make_tier(
                faction_def.faction_id,
                tier_index,
                k_faction_tech_tier_base_costs[tier_index - 1U],
                technology_tier_display_name(faction_def.faction_id, tier_index));
        }
    }
    return tier_defs;
}();

inline const auto k_prototype_technology_node_defs = []()
{
    std::array<TechnologyNodeDef, k_prototype_technology_node_count> node_defs {};
    std::size_t index = 0U;
    for (const auto& faction_def : k_prototype_faction_defs)
    {
        const auto signature_family = signature_family_for_faction(faction_def.faction_id);
        for (std::uint8_t tier_index = 1U; tier_index <= k_faction_tech_tier_count; ++tier_index)
        {
            for (std::uint8_t base_slot_index = 0U;
                 base_slot_index < k_faction_tech_base_choices_per_tier;
                 ++base_slot_index)
            {
                const auto base_node_id =
                    technology_base_node_id(faction_def.faction_id, tier_index, base_slot_index);
                const auto unlockable_id =
                    technology_unlockable_id(faction_def.faction_id, tier_index, base_slot_index);
                const auto& base_authoring =
                    technology_base_authoring(faction_def.faction_id, tier_index, base_slot_index);

                node_defs[index++] = make_base_tech(
                    base_node_id,
                    faction_def.faction_id,
                    tier_index,
                    base_slot_index,
                    signature_family,
                    unlockable_id,
                    base_authoring.unlockable_display_name,
                    base_authoring.display_name,
                    base_authoring.description);

                for (std::uint8_t amplification_choice_index = 0U;
                     amplification_choice_index < k_faction_tech_amplification_choices_per_base;
                     ++amplification_choice_index)
                {
                    const auto& amplification_authoring = technology_amplification_authoring(
                        faction_def.faction_id,
                        tier_index,
                        base_slot_index,
                        amplification_choice_index);

                    node_defs[index++] = make_amplification(
                        technology_amplification_node_id(
                            faction_def.faction_id,
                            tier_index,
                            base_slot_index,
                            amplification_choice_index),
                        faction_def.faction_id,
                        tier_index,
                        base_slot_index,
                        amplification_choice_index,
                        base_node_id,
                        signature_family,
                        amplification_authoring.update_family,
                        unlockable_id,
                        technology_modifier_id(
                            faction_def.faction_id,
                            tier_index,
                            base_slot_index,
                            amplification_choice_index),
                        base_authoring.unlockable_display_name,
                        technology_update_family_display_name(amplification_authoring.update_family),
                        amplification_authoring.display_name,
                        amplification_description(signature_family, amplification_authoring.update_family));
                }
            }
        }
    }
    return node_defs;
}();

[[nodiscard]] inline constexpr const TechnologyTierDef* find_technology_tier_def(
    FactionId faction_id,
    std::uint8_t tier_index) noexcept
{
    for (const auto& tier_def : k_prototype_technology_tier_defs)
    {
        if (tier_def.faction_id == faction_id && tier_def.tier_index == tier_index)
        {
            return &tier_def;
        }
    }

    return nullptr;
}

[[nodiscard]] inline constexpr const TechnologyNodeDef* find_technology_node_def(
    TechNodeId tech_node_id) noexcept
{
    for (const auto& node_def : k_prototype_technology_node_defs)
    {
        if (node_def.tech_node_id == tech_node_id)
        {
            return &node_def;
        }
    }

    return nullptr;
}
}  // namespace gs1
