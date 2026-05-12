# src/messages/

Gameplay message contracts and dispatcher helpers for cross-system coordination.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `game_message.h`: Core internal gameplay message enums and payload structures, including phone panel section-switch and close requests, the shared periodic site-refresh tick used by task-board and phone-stock rerolls, site-protection selector/open-close and overlay-mode requests, a gameplay-owned inventory-slot-tapped intent translated from host UI taps before inventory decides whether that means transfer, use, or no-op, campaign faction-reputation and technology requests, task-reward routing messages for economy/unlockable-reveal/run-modifier awards plus the post-award task-reward-claim-resolved follow-up consumed by runtime projection, manual timed site-modifier end requests, owner-confirmed onboarding-action completion messages for purchase, sale, transfer, item submission, use, and crafting, plus owner-emitted tile/device and living-plant-stability state-change messages and batched ecology tile summaries that let the task board track threshold tasks through subscriptions instead of direct peer writes, with internal present-log payloads carrying a log level so temporary diagnostics can stay out of activity-only smoke output.
- `handlers/`: Handler interface types used by message subscribers.
- `message_dispatcher.h`: Dispatcher declarations for routing subscribed messages to systems.
- `message_dispatcher.cpp`: Dispatcher implementation, now forwarding drained gameplay messages through the runtime-owned aggregate `GameState` presentation context instead of assembling loose runtime field references.
- `message_ids.h`: Shared message ID definitions and indexing helpers.
