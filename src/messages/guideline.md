# src/messages/

Gameplay message contracts and dispatcher helpers for cross-system coordination.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `game_message.h`: Core internal gameplay message enums and payload structures, including phone panel section-switch and close requests, campaign faction-reputation and technology requests, task-reward routing messages for economy/unlockable-reveal/run-modifier awards, owner-confirmed onboarding-action completion messages for purchase, sale, transfer, use, and crafting, plus owner-emitted tile/device and living-plant-stability state-change messages that let the task board track threshold tasks through subscriptions instead of direct peer writes.
- `handlers/`: Handler interface types used by message subscribers.
- `message_dispatcher.h`: Dispatcher declarations for routing subscribed messages to systems.
- `message_dispatcher.cpp`: Dispatcher implementation.
- `message_ids.h`: Shared message ID definitions and indexing helpers.
