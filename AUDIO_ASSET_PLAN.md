# Audio Asset Plan

Date: 2026-04-26

## Goal

Build an audio set that matches the current design direction:

- exposed desert should feel dry, abrasive, and lonely
- planted pockets should feel calmer, softer, and slightly alive
- hazard peaks should hit hard, then release clearly when the event breaks
- the overall tone should stay grounded and present-day rather than fantasy-exotic

This direction comes directly from the current audio notes in `GDD.md`, especially the shelter/open-terrain contrast and the density-based soundscape requirements.

## Recommended Acquisition Plan

### 1. Core ambience

Primary recommendation:

- `BOOM Library - Deserts, Weather & Wildlife`
- `BOOM Library - Winds of Nature`

Why:

- strong base material for exposed desert wind, sand movement, wildlife accents, and calmer sheltered layers
- enough detail to build layered transitions instead of relying on a single looping ambience
- commercial/royalty-free library licensing

Use these for:

- exposed site bed
- medium-density recovery bed
- dense/oasis pocket bed
- pre-storm wind build
- storm peak wind and grit layers
- post-storm relief bed

Official pages:

- https://www.boomlibrary.com/sound-effects/deserts-weather-wildlife/
- https://www.boomlibrary.com/sound-effects/winds-of-nature/
- https://www.boomlibrary.com/support/faq/can-i-use-the-sounds-for-more-than-one-project/

### 2. Desert-specific supplement

Primary recommendation:

- `A Sound Effect - African Desert`
- `A Sound Effect - African Desert Sahara`
- `A Sound Effect - High Desert Winds`

Why:

- fills in harsher open-space desert detail and more aggressive wind character
- useful if the BOOM sets feel too general or too clean on their own
- good source for stronger transitional and hazard layers

Use these for:

- outer-map exposed zones
- sandstorm escalation
- intense gust passes
- abrasive wind sweeteners during hazard peaks

Official pages:

- https://www.asoundeffect.com/sound-library/african-desert/
- https://www.asoundeffect.com/sound-library/african-desert-sahara/
- https://www.asoundeffect.com/sound-library/high-desert-winds/
- https://www.asoundeffect.com/license-agreement/

### 3. General sound effects and UI coverage

Primary recommendation:

- `SONNISS - GDC Game Audio Bundle`

Why:

- large free starter pool for utility sounds that do not need bespoke desert recording
- good place to source UI confirmation, failure ticks, cloth movement, gear handling, footsteps, crate movement, device handling, and notification sweeteners
- license is suitable for games and interactive projects

Use this for:

- HUD and phone UI
- one-shot success/failure cues
- item handling
- build/place/repair sweeteners
- non-desert-specific foley

Official pages:

- https://gdc.sonniss.com/
- https://sonniss.com/gdc-bundle-license

### 4. Prototype music

Prototype-first recommendation:

- `LonePeakMusic - Free Ambient Music Pack`
- `LonePeakMusic - No Copyright Dark Ambient Music Pack`

Why:

- fast way to test whether the game wants music most of the time or only during pressure spikes
- low commitment while the game still lacks a full adaptive music system
- suitable for temporary exploration/relief and hazard-tension experimentation

Notes:

- these packs are useful for prototyping
- check the current credit and redistribution terms on the asset pages before shipping

Official pages:

- https://lonepeakmusic.itch.io/free-ambient-music
- https://lonepeakmusic.itch.io/no-copyright-dark-ambient-music-pack

## Recommended Mix By Category

If budget allows only a few purchases, buy in this order:

1. `BOOM Library - Deserts, Weather & Wildlife`
2. `SONNISS - GDC Game Audio Bundle`
3. `A Sound Effect - High Desert Winds`

Rationale:

- ambience is the core emotional layer for this game
- general one-shots can be covered cheaply at first
- the harshest wind layer is the most valuable premium supplement after the base ambience bed

## Content Matrix

### Ambience states

- `Exposed Desert`: dry wide wind bed, light grit hiss, very sparse life
- `Recovering Patch`: same wind family, but softened, with occasional leaf or brush movement
- `Dense Pocket`: softer wind, protected air feel, subtle plant movement, very occasional life cue
- `Hazard Build`: stronger directional wind, more grit, more motion, less life
- `Hazard Peak`: loud aggressive wind, strong sand texture, narrow-band stress layer
- `Post-Event Relief`: reduced wind energy, cleaner air bed, warmer calm layer

### Music states

- `No Music / Minimal Music`: default for normal field-work loops if ambience is carrying enough emotion
- `Tension Build`: low drone or restrained percussion when a major event approaches
- `Hazard Peak`: short survival-pressure layer, percussion-heavy if used at all
- `Recovery`: sparse relief cue when the event breaks
- `Site Complete`: brief satisfying resolve cue, grounded rather than triumphant-fantasy
- `Site Failed`: short low, dry failure cue

### One-shot SFX groups

- UI confirm, cancel, warning, task complete
- place, remove, repair, watering, harvest, pickup, delivery-crate interactions
- worker exertion and tool sweeteners
- device hums, clicks, sensor pings, service-station activity
- hazard peak stingers and relief stingers

## Budget Paths

### Lean prototype

- SONNISS bundle for most one-shots
- LonePeakMusic packs for temporary music
- a small number of targeted desert wind/ambience purchases only if the free material is obviously not enough

This is the cheapest route and is good enough to prove the audio direction.

### Recommended production-start package

- BOOM `Deserts, Weather & Wildlife`
- BOOM `Winds of Nature`
- SONNISS bundle
- one A Sound Effect desert wind library

This is the best balance of quality, flexibility, and cost.

### Stronger production package

- both BOOM libraries above
- two or more A Sound Effect desert/wind libraries
- SONNISS bundle
- replace prototype music with commissioned or custom-licensed tracks once music direction is locked

Use this only after the game's music-role decision is stable.

## Free-Only Shortlist

This section is the strict no-cost path.

### Free-only recommendation

Use this stack first:

- `SONNISS - #GameAudioGDC Bundle` for general one-shots and foley
- `Kenney - UI Audio` and `Kenney - Digital Audio` for simple UI feedback
- `LonePeakMusic - Free Ambient Music Pack` for calm and recovery music
- `LonePeakMusic - No Copyright Dark Ambient Music Pack` for hazard build and hazard peak music
- `Mixkit - Free Desert Sound Effects` for temporary desert wind and sand layers

This is the best free prototype stack for the current game.

### Ship-safer free sources

These have the cleanest free-use terms among the options reviewed.

#### 1. SONNISS for one-shots

- Source: `https://gdc.sonniss.com/`
- License page: `https://sonniss.com/gdc-bundle-license/`
- Best use: action complete/fail, pickup, crate handling, repair, placement, cloth, tool, UI sweeteners
- Why it is strong: the bundle uses an unlimited-user license and is explicitly framed for game audio use

#### 2. Kenney for UI

- Sources: `https://kenney.nl/assets/ui-audio` and `https://kenney.nl/assets/digital-audio`
- License/support page: `https://kenney.nl/support`
- Best use: button clicks, open/close, accept/cancel, warning pips
- Why it is strong: Kenney states its game assets are public-domain `CC0`, usable in commercial projects with no attribution required

#### 3. OpenGameArt CC0 music for calm states

- `First Light Particles`: `https://opengameart.org/content/first-light-particles-%E2%80%93-cc0-atmospheric-pianoambient-track`
- `The Budding of Consciousness`: `https://opengameart.org/content/the-budding-of-consciousness-%E2%80%93-cc0-ambient-minimalist-theme-yoiyami-blue-series-%E2%80%93-no4`
- `Yoiyami Core Theme`: `https://opengameart.org/content/yoiyami-core-theme-%E2%80%93-deep-blue-ambient-piano`
- Best use: menu, calm planted pocket, post-event relief, reflective moments
- Why it is strong: these are marked `CC0`, which avoids attribution and shipping friction

#### 4. OpenGameArt CC0 dark/tension music

- `Horror Atmosphere`: `https://opengameart.org/content/horror-atmosphere`
- `Ambient Horror Track 01`: `https://opengameart.org/content/ambient-horror-track-01`
- `Eternal Sleep`: `https://opengameart.org/content/eternal-sleep`
- Best use: hazard build, storm pressure, failure-state or low-morale variants
- Why it is strong: all three are listed as `CC0` and are usable as temporary survival-pressure beds

### Good free sources with extra conditions

These are usable, but they are less clean than the sources above.

#### 1. LonePeakMusic

- `https://lonepeakmusic.itch.io/free-ambient-music`
- `https://lonepeakmusic.itch.io/no-copyright-dark-ambient-music-pack`
- Best use: long-form prototype music while the game decides how much music it really wants
- Condition: free use is allowed for commercial projects, but the asset pages require credit for free use and list the asset license as `Creative Commons Attribution v4.0 International` on the ambient pack page

#### 2. alkakrab

- `https://alkakrab.itch.io/free-10-rpg-game-ambient-tracks-music-pack-no-copyright`
- Best use: cheap coverage for menus, map, and low-pressure exploration states
- Condition: the page says the pack is free for commercial use and credit is optional, but it does not present as formal a license statement as `CC0`

#### 3. Rosentwig

- `https://rosentwig.itch.io/atmospheric-adventures-freebie`
- Best use: exploration, oasis, recovery, and menu music
- Condition: the page says it is free for commercial and non-commercial use with no attribution required, but again this is a page-stated license rather than a standard public-domain label

### Prototype-only or caution sources

These are useful for testing, but I would not make them your primary shipping backbone without manual review of every file.

#### 1. Mixkit desert ambience

- Source: `https://mixkit.co/free-sound-effects/desert/`
- License page: `https://mixkit.co/license/`
- Best use: temporary desert wind, sand hiss, sparse animal accents
- Caution: useful for prototyping, but the desert category is small and the material is not specialized enough to carry the whole game's ambience alone

#### 2. Pixabay sound effects

- License page: `https://pixabay.com/service/license-summary/`
- Example search page: `https://pixabay.com/sound-effects/search/desert%20storm/`
- Example clip page: `https://pixabay.com/sound-effects/nature-desert-wind-gust-sound-effect-473421/`
- Best use: filler layers, temporary wind sweeteners, quick test material
- Caution: Pixabay content is free under the platform license, but individual uploads can still be low-quality, mislabeled, or AI-generated. Use only after checking the clip page carefully and avoid building your final soundscape around random uploader mixes

### Concrete free-only state mapping

#### Ambience

- `Exposed Desert`: Mixkit desert wind layers plus selected Pixabay wind/sand sweeteners only if needed
- `Recovering Patch`: exposed desert bed at lower level, with subtle plant rustle from SONNISS or OpenGameArt SFX if available
- `Dense Pocket`: calmer low-wind bed, then let music and soft foley carry the shelter feeling
- `Hazard Build`: Mixkit wind plus SONNISS impacts/rumbles layered into a pressure bed
- `Hazard Peak`: same stack, but denser and louder, with one-shot hazard stingers from SONNISS
- `Post-Event Relief`: drop back to a thin calm wind bed and bring in a gentle music layer

#### Music

- `Menu / Calm / Oasis`: OpenGameArt `First Light Particles`, `The Budding of Consciousness`, or `Yoiyami Core Theme`
- `Recovery`: LonePeak `Distant Memories` or Rosentwig free pack material
- `Hazard Build`: OpenGameArt `Horror Atmosphere` or LonePeak dark ambient pack
- `Hazard Peak`: LonePeak dark ambient pack or `Ambient Horror Track 01`
- `Failure / Hard Aftermath`: `Eternal Sleep`

#### One-shots

- `UI`: Kenney first, then SONNISS if you need more weight
- `Actions`: SONNISS first
- `Notifications`: Kenney or SONNISS
- `Hazard stingers`: SONNISS first

### Free-only final recommendation

If you want one practical no-cost setup right now:

1. take `SONNISS` for most one-shots
2. take `Kenney` for UI clicks and notifications
3. take `Yoiyami CC0` tracks from OpenGameArt for calm and relief states
4. take `LonePeakMusic` dark pack for hazard prototyping if attribution is acceptable
5. use `Mixkit` desert wind only as a temporary ambience layer until you can replace it

This avoids the worst licensing friction while still giving you enough material to start implementing audio immediately.

## Free-Only Implementation Checklist

This is the concrete first-pass set to download and wire into the game.

### Exact ambience picks

Use these as the first prototype ambience layers:

1. `Mixkit - Desert ambience`
   Source: `https://mixkit.co/free-sound-effects/desert/`
   Use for: base `Exposed Desert` loop
2. `Mixkit - Wind in the top of the mountain`
   Source: `https://mixkit.co/free-sound-effects/desert/`
   Use for: extra top-layer wind movement on exposed tiles
3. `Mixkit - Sand in the desert`
   Source: `https://mixkit.co/free-sound-effects/desert/`
   Use for: grit/sand sweetener during stronger wind
4. `Mixkit - Ambient sound in the desert`
   Source: `https://mixkit.co/free-sound-effects/desert/`
   Use for: alternate low-level air bed for `Recovering Patch`
5. `Mixkit - Blizzard cold winds`
   Source: `https://mixkit.co/free-sound-effects/desert/`
   Use for: `Hazard Build` and `Hazard Peak`
6. `Pixabay - Desert Sand Dunes`
   Source: `https://pixabay.com/sound-effects/search/desert/`
   Use for: long-form filler layer if Mixkit feels too short
7. `Pixabay - Desert Sand Rustling`
   Source: `https://pixabay.com/sound-effects/search/desert/`
   Use for: sheltered rustle bed near planted pockets

### Exact music picks

These are the cleanest `CC0` music choices from the free pass:

1. `Yoiyami - First Light Particles`
   Source: `https://opengameart.org/content/first-light-particles-%E2%80%93-cc0-atmospheric-pianoambient-track`
   License: `CC0`
   Use for: title, pause, calm menu, early morning relief
2. `Yoiyami - The Budding of Consciousness`
   Source: `https://opengameart.org/content/the-budding-of-consciousness-%E2%80%93-cc0-ambient-minimalist-theme-yoiyami-blue-series-%E2%80%93-no4`
   License: `CC0`
   Use for: dense pocket / oasis / recovery
3. `Yoiyami - Yoiyami Core Theme`
   Source: `https://opengameart.org/content/yoiyami-core-theme-%E2%80%93-deep-blue-ambient-piano`
   License: `CC0`
   Use for: regional map, low-pressure exploration, end-of-day calm
4. `SubspaceAudio - Horror Atmosphere`
   Source: `https://opengameart.org/content/horror-atmosphere`
   License: `CC0`
   Use for: hazard build, incoming event pressure
5. `Cleyton Kauffman - Ambient Horror Track 01`
   Source: `https://opengameart.org/content/ambient-horror-track-01?PageSpeed=noscript`
   License: `CC0`
   Use for: hazard peak, failure, or near-collapse state
6. `The Oracle - Eternal Sleep`
   Source: `https://opengameart.org/content/eternal-sleep`
   License: `CC0`
   Use for: site failed, severe aftermath, hard setback screen

### Exact SFX / UI picks

Use these first before digging deeper into larger libraries:

1. `Kenney - UI Audio`
   Source: `https://kenney.nl/assets/ui-audio`
   License: `CC0`
   Use for: button confirm, cancel, open, close, warning pips
2. `Kenney - Digital Audio`
   Source: `https://kenney.nl/assets/digital-audio`
   License: `CC0`
   Use for: field phone, task notifications, menu transitions
3. `Mixkit - Select click`
   Source: `https://mixkit.co/free-sound-effects/click/`
   Use for: positive UI confirm fallback
4. `Mixkit - Click error`
   Source: `https://mixkit.co/free-sound-effects/click/`
   Use for: invalid action or unavailable placement
5. `Mixkit - Modern technology select`
   Source: `https://mixkit.co/free-sound-effects/technology/`
   Use for: phone section switch or task accept
6. `Mixkit - Interface option select`
   Source: `https://mixkit.co/free-sound-effects/technology/`
   Use for: list navigation and phone cursor movement
7. `Mixkit - Gear metallic lock sound`
   Source: `https://mixkit.co/free-sound-effects/click/`
   Use for: crate open/close, storage transfer, lock-in actions
8. `Mixkit - Quick positive video game notification interface`
   Source: `https://mixkit.co/free-sound-effects/click/`
   Use for: task complete or action completed cue
9. `Mixkit - Game quick warning notification`
   Source: `https://mixkit.co/free-sound-effects/click/`
   Use for: weather warning or low-resource alert
10. `Mixkit - Body impact falling into the sand`
    Source: `https://mixkit.co/free-sound-effects/desert/`
    Use for: harsh failure, placement break, or impact sweetener

### State-to-asset mapping

Use this first-pass mapping:

- `Menu`: `First Light Particles`
- `Regional Map`: `Yoiyami Core Theme`
- `Normal Site / Exposed Desert`: `Desert ambience` + `Wind in the top of the mountain`
- `Recovering Patch`: `Ambient sound in the desert` at low level, mixed with reduced exposed layer
- `Dense Pocket / Oasis`: `The Budding of Consciousness` at low volume plus a softened ambience bed
- `Hazard Build`: `Horror Atmosphere` + `Blizzard cold winds` + `Sand in the desert`
- `Hazard Peak`: `Ambient Horror Track 01` + `Blizzard cold winds` pushed louder + warning one-shots
- `Post-Event Relief`: `First Light Particles` or `The Budding of Consciousness`
- `Site Complete`: `Quick positive video game notification interface` over the current calm music bed
- `Site Failed`: `Eternal Sleep` + a short failure one-shot from `Kenney` or `Mixkit Click error`

### Minimum viable download set

If you want the smallest useful first pass, download only these:

1. `Kenney - UI Audio`
2. `Kenney - Digital Audio`
3. `Mixkit - Desert ambience`
4. `Mixkit - Blizzard cold winds`
5. `Mixkit - Sand in the desert`
6. `Yoiyami - First Light Particles`
7. `Yoiyami - The Budding of Consciousness`
8. `SubspaceAudio - Horror Atmosphere`
9. `Ambient Horror Track 01`

That is enough to implement:

- one normal desert ambience state
- one sheltered/recovery state
- one hazard escalation state
- one hazard peak state
- basic UI and notification feedback

## Integration Notes

The current public gameplay boundary already exposes one-shot cue transport:

- `Gs1OneShotCueKind` in `include/gs1/types.h`
- `GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE` in `include/gs1/types.h`

That is enough for early one-shot playback, but not enough on its own for persistent ambience and music state.

Recommended host-side persistent audio parameters:

- `weatherWind`
- `weatherDust`
- `eventPeakActive`
- `localShelterFactor`
- `localDensityFactor`
- `insideDensePocket`

The host can derive an early version of these from existing projection data:

- `GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE`
- local tile density around the worker from tile projection data
- distance from dense plant clusters or shelter structures
- hazard/event timing from site weather update fields

## Practical Next Step

Do this in order:

1. buy or gather the `lean prototype` set
2. build `6` ambience states and `5-8` one-shot cue groups first
3. test whether the game works better with sparse music than with constant looping music
4. only then commit to a larger music purchase or custom composition path

## Recommendation Summary

If choosing only one concrete plan now:

- buy `BOOM Library - Deserts, Weather & Wildlife`
- use `SONNISS` for utility one-shots
- use temporary `LonePeakMusic` tracks for prototype music
- add `A Sound Effect - High Desert Winds` if the hazard peak lacks bite

That is the most practical starting package for the current game.
