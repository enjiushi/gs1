# Content Authoring Contract

Sources:

- [GDD.md](GDD.md)
- [GAME_STRUCTURE.md](GAME_STRUCTURE.md)
- [SYSTEM_DESIGN_STATUS.md](SYSTEM_DESIGN_STATUS.md)

Purpose: lock the authored content contracts that must exist before detailed system-design work can safely define runtime structs, content-definition tables, generators, and validation tools.

Boundary rules:

- `GDD.md` stays at gameplay-contract, pacing, and meter-relationship level.
- `GAME_STRUCTURE.md` owns the global ECS, queue, and engine-boundary framework.
- This document owns authoring-side data contracts, generation inputs, and validation rules for generated and authored-override gameplay content.
- Host-language structs may differ from the names used here, but the authored fields and meanings should stay equivalent.
- Exact balance values may still change later; field presence, field meaning, and validation behavior should not.

## 1. Scope

This contract is required for the next system-design stage because the current prototype and full-board model both depend on authored content that is later instantiated at runtime.

In scope:

- `TaskTier` authored lookup data
- `RewardBand` authored lookup data
- task templates for normal generated tasks
- onboarding-task and tutorial-task authored overrides
- task-chain authored definitions
- reward-draft candidate definitions and reward-draft profile rules
- event templates and forecast profiles
- authored prototype support packages for restored sites

Out of scope:

- runtime task instance state
- runtime save/load boundaries
- final tuning values for economy, pressure, or spawn weights
- UI layout implementation
- engine adapter implementation

## 2. Shared Authoring Rules

All authored content rows in this document should follow these shared rules:

| Rule | Contract |
|---|---|
| Stable ids | Every authored row must have one unique stable string id. Ids are the only allowed cross-row references. |
| Display text | Human-facing title and description text should be stored through text keys or localizable string ids, not embedded runtime logic text. |
| Optional fields | Optional fields should be absent or null when unused, not filled with fake placeholder ids. |
| Positive authored counts | `targetAmount`, fixed reward amounts, duration mins, duration maxes, and authored export counts must all be positive when present. |
| Reference validity | Any referenced `factionId`, `itemId`, `unlockableId`, `modifierId`, `taskTemplateId`, `eventArchetypeId`, or tech id must resolve to a real row. |
| Internal cash points | Any authored row that uses hidden valuation should either author `internalCashPoint`-style values directly or derive them from shared authored meter cash-point tuning; rows that still surface player-facing cash should keep that visible cash integral where required. |
| Onboarding separation | Authored onboarding-only content must be explicitly flagged so the normal generator cannot surface it in the repeatable runtime pools. |
| Prototype-first simplicity | If a row can be expressed through one existing shared content family, do not add a new one-off schema just for one prototype case. |

## 3. Required Authored Tables

The next system-design pass should assume these authored definition tables exist:

- `TaskTierDef`
- `RewardBandDef`
- `RewardDraftProfileDef`
- `RewardCandidateDef`
- `TaskTemplateDef`
- `TutorialTaskSetDef`
- `TaskChainDef`
- `EventTemplateDef`
- `ForecastProfileDef`
- `SiteSupportPackageDef`

Not every future version must keep these exact filenames or one-table-per-type storage, but the authored contract should preserve these meanings.

## 3.1 Item, Structure, And Crafting Authoring Clarification

The current prototype also depends on these authoring-side gameplay rows:

- `ItemDef`
- `StructureDef`
- `CraftRecipeDef`

Required item authoring rules:

- authored crafting materials such as wood, iron, harvested fiber, water, and seeds are ordinary `ItemDef` rows
- an item may be both `consumable` and usable as a craft ingredient
- deployable kits are ordinary items with a linked authored `StructureDef`
- authored item source rules should distinguish at least `BuyOnly`, `CraftOnly`, `BuyOrCraft`, `HarvestOnly`, and `ExcavationOnly`
- authored stack size is the hard cap for one runtime item stack
- item-level internal cash-point values remain the hidden valuation source for reward and task scoring even when the player only sees normal cash prices
- consumable item rows with player-meter refill should derive that hidden valuation from shared authored player-meter cash-point tuning instead of duplicating per-item internal cash-point authoring
- non-meter goods may still author fallback internal cash-point values when no player-meter-derived valuation path exists yet

Economy valuation clarification:

- long-term tech rows and direct-purchase unlockable offers should author internal cash-point values as their source of truth
- player-facing cash prices should be derived from those internal values using `100` cash points = `1` cash

Excavation merchandise clarification:

- excavation-only stone goods should remain ordinary `ItemDef` rows
- prototype excavation-merchandise items should expose `Sell` capability only
- prototype excavation-merchandise items should not link plants or structures and should not act as consume/build/plant inputs
- provisional excavation-merchandise internal cash-point values should stay within `100-20000` until the broader economy rebalance pass
- more valuable excavation items should generally be authored with lower excavation-table probability than cheaper stones in the same table

Excavation-depth authoring clarification:

- excavation depth should support at least `Rough`, `Careful`, and `Thorough`
- the default ruleset should expose only `Rough` until tech or enhancement content explicitly unlocks deeper passes
- later depth levels may author their own base energy cost, discovery chance, or loot-table overrides, but they should still use the same weight-plus-bias tuning pattern as the base excavation action
- excavation visuals should be authored or mapped distinctly enough that the player can tell rough, careful, and thorough depth at a glance

Technology authoring clarification:

- each faction tech row should author whether it is a base tech or an enhancement plus its `enhancementChoiceIndex`
- faction base techs use faction-reputation tiers `1-8`; paired enhancements use faction-reputation tiers `9-16`
- an enhancement row must pair to the base tech in the same faction+tier and use one of the two exclusive enhancement choice slots
- a tech row may optionally author a granted content payload such as an `ItemDef`, `PlantDef`, `StructureDef`, or `CraftRecipeDef` id in addition to any modifier or mechanism effect metadata

Required excavation loot-table authoring rules:

- each excavation loot table entry must author a valid excavation `itemId` plus its probability inside that table
- each excavation loot table must sum to exactly `100%`
- the chance to find any item from excavation is a separate action/mechanism tuning value and must not be folded into the loot-table entry totals
- excavation loot tables should reference only prototype merchandise items intended to be sold rather than planted, consumed, or deployed
- site or region content may point to different excavation loot tables, but each table must still validate independently against the same `100%` total rule
- if different excavation depths use different loot tables, each depth-bound table must independently satisfy the same `100%` total rule

Required structure authoring rules:

- a structure that grants local storage must author its slot count directly on `StructureDef`
- a structure that acts as a crafting station must author its crafting-station kind directly on `StructureDef`
- a deployed storage crate is a structure with storage but no crafting recipes

Required crafting authoring rules:

- each `CraftRecipeDef` must name one station structure id, one output item id, one output quantity, one craft duration, and an explicit list of ingredient item ids plus counts
- recipes are selected by station plus output item; the output item must therefore be unique per station in the current prototype contract
- crafting input items are removed across matching stacks one after another, so authored recipes should assume stack-spanning consumption is valid

## 4. Task Authoring Contract

### 4.1 `TaskTierDef`

`TaskTierDef` is the shared authored lookup for spawn rarity, reward quality, and guaranteed faction-trust payout direction.

Required fields:

| Field | Type | Notes |
|---|---|---|
| `taskTierId` | id | Stable tier id such as `Routine`, `Standard`, `Priority`, `Elite`, or `Jackpot`. |
| `displayNameKey` | text key | Player-facing tier label. |
| `spawnWeightClass` | enum/string | Relative rarity bucket used by task generation. |
| `rewardBandMinId` | id | Lowest reward band this tier may use. |
| `rewardBandMaxId` | id | Highest reward band this tier may use. |
| `baseMoneyValue` | positive number | Authoring-side value baseline used to scale non-money rewards. |
| `baseFactionReputationReward` | positive integer | Guaranteed same-faction trust payout granted on task completion. |
| `allowsJackpotModifierAccess` | bool | Marks whether the tier may surface top-end modifier candidates. |

Rules:

- `rewardBandMinId` must not rank above `rewardBandMaxId`.
- `baseMoneyValue` is a balance value, but it must still exist because reward derivation depends on it structurally.
- `baseFactionReputationReward` belongs to the tier table rather than every task row unless a later system explicitly needs per-template override.

### 4.2 `TaskTemplateDef`

`TaskTemplateDef` is the main authored row for one repeatable or authored-only task template.

Required fields:

| Field | Type | Notes |
|---|---|---|
| `taskTemplateId` | id | Stable task-template id. |
| `titleTextKey` | text key | Player-facing task title. |
| `descriptionTextKey` | text key | Player-facing task description. |
| `publisherFactionId` | id | Which faction publishes the task. |
| `taskType` | enum | `CollectItem` or `ReachProgressTarget` for the current prototype contract. |
| `taskTierId` | id | References `TaskTierDef`. |
| `rewardDraftProfileId` | id | References `RewardDraftProfileDef`. |
| `isOnboardingOnly` | bool | Prevents the normal generator from surfacing the task. |
| `targetAmount` | positive integer | Shared completion target `y` for the `x / y` progress model. |

Optional fields:

| Field | Type | Notes |
|---|---|---|
| `tutorialTaskSetId` | id | Set only when the task belongs to a tutorial-only board set. |
| `requiredConceptUnlockIds[]` | list of ids | Prevents surfacing before the required onboarding or concept gates are open. |
| `requiredSiteTags[]` | list of ids/strings | Site-context filters such as exposed-lane, erosion, irrigation, or output tags. |
| `blockedSiteTags[]` | list of ids/strings | Site-context disqualifiers. |
| `chainId` | id | References the chain this task belongs to when applicable. |
| `chainStepIndex` | positive integer | Step index inside the linked chain. |
| `followUpTaskTemplateId` | id | Optional explicit next step reference for chain-safe validation. |

Task-type-specific required fields:

| `taskType` | Required payload |
|---|---|
| `CollectItem` | `itemId` |
| `ReachProgressTarget` | `progressTargetKind` and `progressTargetId` |

Current supported `progressTargetKind` values:

- `Plant`
- `Device`

Rules:

- normal repeatable tasks must set `isOnboardingOnly = false`
- onboarding tasks and tutorial tasks must set `isOnboardingOnly = true`
- one task row must never mix `itemId` with `progressTargetKind` or `progressTargetId`
- one task row must never define more than one completion payload family
- the prototype generator should instantiate runtime progress as one shared `currentProgressAmount` against the authored `targetAmount`

### 4.3 `TutorialTaskSetDef`

`TutorialTaskSetDef` is the authored override group that pins a featured faction's board during onboarding.

Required fields:

| Field | Type | Notes |
|---|---|---|
| `tutorialTaskSetId` | id | Stable tutorial-set id. |
| `featuredFactionId` | id | The faction taught by this set. |
| `taskTemplateIds[]` | ordered list of ids | The authored tasks surfaced while the set is active. |
| `completionRule` | enum | For the current prototype, always `CompleteAllRequiredOnce`. |
| `blocksNormalRefresh` | bool | Must be `true` for the current onboarding contract. |
| `acceptedTaskCapOverride` | positive integer | For the current prototype, usually `2`. |

Rules:

- every referenced task in `taskTemplateIds[]` must also reference the same `tutorialTaskSetId`
- all tasks in one tutorial set must belong to the same `featuredFactionId`
- tutorial-set tasks must never appear in the normal multi-faction generator unless their onboarding-only flag is explicitly cleared later

### 4.4 `TaskChainDef`

`TaskChainDef` is the authored sequence definition for chain tasks.

Required fields:

| Field | Type | Notes |
|---|---|---|
| `taskChainId` | id | Stable chain id. |
| `chainStepTaskTemplateIds[]` | ordered list of ids | For the current prototype, exactly `3` task template ids. |
| `completionBonusRewardDraftProfileId` | id | Bonus reward profile granted after the final chain step completes. |
| `completionBonusFactionReputationReward` | non-negative integer | Optional extra trust payout on full chain completion. |

Rules:

- chain length must be exactly `3` in the current prototype contract
- all chain step task ids must be unique
- all chain step tasks must reference the same `taskChainId`
- chain steps must use consecutive `chainStepIndex` values
- the final step must not point to a further follow-up task
- the bonus reward profile must resolve to a valid draft or fixed bonus definition

## 5. Reward Authoring Contract

### 5.1 `RewardBandDef`

`RewardBandDef` is the shared quality bucket used before exact reward selection.

Required fields:

| Field | Type | Notes |
|---|---|---|
| `rewardBandId` | id | Stable reward-band id such as `BandA` through `BandE`. |
| `displayNameKey` | text key | Optional player/dev label. |
| `relativeValueClass` | enum/string | Relative strength bucket such as low, medium, high. |
| `allowedRewardFamilies[]` | list | Any of `Money`, `Item`, `Unlockable`, `Modifier`. |
| `allowsUnlockable` | bool | Convenience validation flag. |
| `allowsModifier` | bool | Convenience validation flag. |

Rules:

- `allowedRewardFamilies[]` must not be empty
- `allowsUnlockable` and `allowsModifier` must agree with `allowedRewardFamilies[]`
- reward generation may step down from a blocked band to a lower valid band, but may never step above the current tier's allowed max band

### 5.2 `RewardDraftProfileDef`

`RewardDraftProfileDef` defines how one task or chain-completion draft should be assembled.

Required fields:

| Field | Type | Notes |
|---|---|---|
| `rewardDraftProfileId` | id | Stable draft profile id. |
| `optionCount` | positive integer | For the current prototype, usually `2`. |
| `preferPublisherSignatureReward` | bool | When true, at least one slot should try the publisher-biased pool first. |
| `fallbackFamilyOrder[]` | ordered list | Family fallback order used when preferred options are blocked. |
| `allowDuplicateRewardFamilies` | bool | Defaults to `false` for prototype readability. |

Rules:

- `optionCount` should usually be `2` in the current prototype
- fallback order must end in a safe always-valid family such as `Money` or `Item`
- if duplicates are disallowed, the generator should retry other valid families before falling back to duplicate families

### 5.3 `RewardCandidateDef`

`RewardCandidateDef` is one authored reward candidate that may appear inside a draft when all gates pass.

Required fields:

| Field | Type | Notes |
|---|---|---|
| `rewardCandidateId` | id | Stable reward-candidate id. |
| `rewardFamily` | enum | `Money`, `Item`, `Unlockable`, or `Modifier`. |
| `candidateLabelKey` | text key | Optional visible label for the choice. |
| `amountMode` | enum | `Fixed` or `ScaledFromTierMoneyBaseline`. |

Family-specific required references:

| `rewardFamily` | Required reference field |
|---|---|
| `Money` | none |
| `Item` | `itemId` |
| `Unlockable` | `unlockableId` |
| `Modifier` | `modifierId` |

Required amount fields:

| `amountMode` | Required fields |
|---|---|
| `Fixed` | `fixedAmount` |
| `ScaledFromTierMoneyBaseline` | `tierMoneyScale` |

Optional gating fields:

| Field | Type | Notes |
|---|---|---|
| `allowedFactionIds[]` | list of ids | Empty means any faction. |
| `allowedTaskTierIds[]` | list of ids | Empty means any eligible tier. |
| `requiredPermanentTechIds[]` | list of ids | Blocks candidates until long-term tech allows them. |
| `requiredConceptUnlockIds[]` | list of ids | Prevents onboarding leaks. |
| `candidatePoolTags[]` | list | Used by faction-biased and general pool selection. |
| `isDirectPurchaseFallbackEligible` | bool | Relevant only for `Unlockable` family rows. |

Rules:

- `Money` candidates must not carry `itemId`, `unlockableId`, or `modifierId`
- `Item`, `Unlockable`, and `Modifier` candidates must reference a valid row of the matching family
- `isDirectPurchaseFallbackEligible` may be `true` only for `Unlockable` candidates
- `tierMoneyScale` must resolve to a positive result when combined with the current tier's `baseMoneyValue`
- reward candidates should stay content-facing; they should not contain direct runtime mutation scripts

### 5.4 Reward Generation Order

The generator-facing order is now fixed:

1. Choose the task's authored `TaskTemplateDef`.
2. Read the linked `TaskTierDef`.
3. Resolve the allowed reward-band range from the tier.
4. Build the eligible reward-candidate pool using faction, tech, onboarding, and task-tier gates.
5. For each draft slot, try the draft profile's preferred family or publisher-signature direction first.
6. If no valid candidate exists in the current target band, step down to the next lower valid band.
7. If the preferred family still fails, use the draft profile's `fallbackFamilyOrder[]`.
8. If everything else fails, produce a safe `Money` or `Item` candidate so the draft never shows an empty slot.

This order should be implemented once in system design and then driven entirely by authored data.

## 6. Event Authoring Contract

### 6.1 `ForecastProfileDef`

`ForecastProfileDef` defines how a generated event is surfaced before it reaches dangerous phases.

Required fields:

| Field | Type | Notes |
|---|---|---|
| `forecastProfileId` | id | Stable forecast-profile id. |
| `displayNameKey` | text key | Forecast-facing label. |
| `forecastLeadTimeClass` | enum/string | Relative warning lead such as short, medium, long. |
| `certaintyClass` | enum/string | Relative clarity such as low, medium, high. |
| `showsExactArchetype` | bool | Whether the forecast names the exact event archetype or only signals risk direction. |

### 6.2 `EventTemplateDef`

`EventTemplateDef` is the authored row for one generated harsh-event template.

Required fields:

| Field | Type | Notes |
|---|---|---|
| `eventTemplateId` | id | Stable event-template id. |
| `eventArchetypeId` | id | References a valid event archetype such as `HeatWave`, `Sandstorm`, or `CompoundFront`. |
| `forecastProfileId` | id | References `ForecastProfileDef`. |
| `warningDurationMin` | positive number | Authoring-side minimum duration for `Warning`. |
| `warningDurationMax` | positive number | Authoring-side maximum duration for `Warning`. |
| `buildDurationMin` | positive number | Minimum `Build` duration. |
| `buildDurationMax` | positive number | Maximum `Build` duration. |
| `peakDurationMin` | positive number | Minimum `Peak` duration. |
| `peakDurationMax` | positive number | Maximum `Peak` duration. |
| `decayDurationMin` | positive number | Minimum `Decay` duration. |
| `decayDurationMax` | positive number | Maximum `Decay` duration. |
| `warningHeatPressure` | non-negative number | Event-side pressure contribution during `Warning`. |
| `warningWindPressure` | non-negative number | Event-side pressure contribution during `Warning`. |
| `warningDustPressure` | non-negative number | Event-side pressure contribution during `Warning`. |
| `buildHeatPressure` | non-negative number | Event-side pressure contribution during `Build`. |
| `buildWindPressure` | non-negative number | Event-side pressure contribution during `Build`. |
| `buildDustPressure` | non-negative number | Event-side pressure contribution during `Build`. |
| `peakHeatPressure` | non-negative number | Event-side pressure contribution during `Peak`. |
| `peakWindPressure` | non-negative number | Event-side pressure contribution during `Peak`. |
| `peakDustPressure` | non-negative number | Event-side pressure contribution during `Peak`. |
| `decayHeatPressure` | non-negative number | Event-side pressure contribution during `Decay`. |
| `decayWindPressure` | non-negative number | Event-side pressure contribution during `Decay`. |
| `decayDustPressure` | non-negative number | Event-side pressure contribution during `Decay`. |

Rules:

- for every phase, `min` must be less than or equal to `max`
- every event template must produce a meaningful hazard profile, so at least one of the build or peak channel groups must be non-zero
- `Peak` should usually carry the highest total pressure for the archetype unless the archetype intentionally uses a flatter profile
- event templates are authored content; the actual event instance is generated at runtime from eligible templates
- event templates are not the same thing as `Per-Site Modifier`s

## 7. Plant Authoring Contract

The prototype now needs one plant-authored action-timing field so planting can project a stable adapter-side progress bar without hardcoding every seed's timing in runtime code.

### 7.1 `PlantDef`

Required fields in addition to the existing plant ecology fields:

| Field | Type | Notes |
|---|---|---|
| `focus` | enum | One of `Setup`, `Protection`, `OutputWorkerSupport`, `SalinityOutput`, `SoilSupport`, `ProtectionOutput`, `SoilSupportSalinity`, `ProtectionWorkerSupport`, `Output`, or `ProtectionSoilSupport`. |
| `auraSize` | integer `0-3` | Shared non-wind reach for plant-side heat, dust, fertility, and focus-derived salinity support. |
| `windProtectionRange` | integer `0-3` | Directional lee-side wind reach. |
| `protectionRatio` | number `0-1` | Multiplier on outward heat, wind, and dust protection only. |
| `saltTolerance` | number `0-100` | Salinity resistance value and, for support-focused plants, the source value for plant-side salinity rehabilitation. |
| `heatTolerance` | number `0-100` | Heat resistance and outward heat-shelter source. |
| `windResistance` | number `0-100` | Wind resistance and outward wind-shelter source. |
| `dustTolerance` | number `0-100` | Dust resistance and outward dust-shelter source. |
| `fertilityImprovePower` | number `0-100` | Plant-side fertility support meter. |
| `outputPower` | number `0-100` | Plant-side output share of the authored roster pool; should stay aligned with the linked harvest item's internal cash-point value. |
| `plantActionDurationMinutes` | positive number | Per-seed planting duration used when a `Plant` site action starts from an inventory seed item linked to this plant. |

Rules:

- plant rows no longer author a separate `salinityReductionPower`; runtime derives plant-side salinity rehabilitation from `saltTolerance` plus `focus`
- the authored plant meter pool is `saltTolerance + heatTolerance + windResistance + dustTolerance + fertilityImprovePower + outputPower`
- starter plants must use total pool `90`
- each later plant unlock step must add `10` more total pool than the previous unlock step
- only protection-focused or support-focused plants may use non-zero `auraSize`
- only protection-focused plants may use non-zero `windProtectionRange`
- `Support` plants must keep `protectionRatio = 0` and `windProtectionRange = 0`
- `Output` and `Self` plants must keep `auraSize = 0`, `windProtectionRange = 0`, and `protectionRatio = 0`
- plants without harvest output must keep `outputPower = 0`
- plants with harvest output must use positive `outputPower` and keep it within the allowed band for the linked harvest item's internal cash-point value
- `plantActionDurationMinutes` must be positive
- item-based planting must resolve its action duration from the linked plant row, not from a hardcoded per-item switch
- adapter progress bars should treat this authored value as total action duration and animate locally after the start message arrives

## 8. Authored Prototype Support Package Contract

The prototype uses authored regional support packages rather than ecology-derived formulas. That authoring should be explicit rather than hidden in site notes.

### 8.1 `SiteSupportPackageDef`

Required fields:

| Field | Type | Notes |
|---|---|---|
| `siteSupportPackageId` | id | Stable support-package id. |
| `sourceSiteId` | id | The completed site that exports this package. |
| `exportedItemEntries[]` | list | Per-item exported counts available for pre-deployment loadout. |
| `nearbyAuraModifierIds[]` | list of ids | Passive modifier ids applied at deployment. |
| `siteOutputModifierId` | optional id | Optional persistent site-output trait row for this site. |

Per-item entry fields:

- `itemId`
- `exportCount`

Rules:

- `exportCount` must be positive
- exported item counts should already respect the current prototype stack-cap assumptions
- `nearbyAuraModifierIds[]` should contain only modifiers valid for always-on deployment support
- this contract is authored per source site, not recomputed from plant composition during the prototype

## 9. Validation Rules

Content tools should reject invalid authored content before runtime. At minimum, validation must enforce these rules:

- every authored id is unique inside its row family
- every cross-row reference resolves
- every `TaskTemplateDef` picks exactly one supported `taskType`
- every `CollectItem` task has a valid `itemId` and a positive `targetAmount`
- every `ReachProgressTarget` task has a valid `progressTargetKind`, a valid `progressTargetId`, and a positive `targetAmount`
- one task template does not mix `CollectItem` fields with `ReachProgressTarget` fields
- onboarding-only tasks are not included in normal runtime generator pools
- every tutorial set references tasks from exactly one featured faction
- every chain has exactly `3` unique task steps in the current prototype contract
- every chain step and follow-up reference is internally consistent
- every task tier used by tasks resolves to a real `TaskTierDef`
- every reward profile resolves to a real `RewardDraftProfileDef`
- every reward band resolves to a real `RewardBandDef`
- every reward candidate's family matches its referenced content id
- every fixed reward amount is positive
- every scaled reward amount resolves to a positive result for all allowed task tiers
- every draft profile can always reach at least one safe fallback family
- every event template uses valid non-negative durations and pressure values
- every event template has at least one meaningful hazardous pressure channel in build or peak
- every plant definition uses a positive `plantActionDurationMinutes`
- every plant definition satisfies the authored meter-pool ladder and focus/range rules above
- every support package references a real source site and real exported content rows

Validation should happen in content tools, spreadsheet import, or build-time data compilation. Runtime should not be the first place where broken content is discovered.

## 10. Runtime Boundary Clarification

This document intentionally does not define runtime task instance structs in full detail. It defines the authored content that runtime systems must consume.

Runtime state still belongs to later system-design documents such as:

- `TaskInstanceState`
- `EventState`
- `RewardDraftState`
- `SiteSupportRuntimeState`

The important handoff rule is:

- authored content rows define legal generator inputs
- runtime structs define live instantiated state
- save boundaries, when added later, serialize runtime state rather than authored definitions

## 11. Immediate System-Design Impact

With this contract in place, the next system-design pass can safely define:

- authored content-definition schemas
- import/build validation
- generator service boundaries for task, event, and reward selection
- runtime task and event instance structs
- planting-duration lookup from authored plant rows into runtime action state
- authored prototype-support package loading

The only remaining major formal contract still intentionally deferred for the prototype path is save-data boundaries.
