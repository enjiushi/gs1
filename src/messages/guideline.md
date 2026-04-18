# src/messages/

Gameplay message contracts and dispatcher helpers for cross-system coordination.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `game_message.h`: Core internal gameplay message enums and payload structures, including phone panel section-switch requests.
- `handlers/`: Handler interface types used by message subscribers.
- `message_dispatcher.h`: Dispatcher declarations for routing subscribed messages to systems.
- `message_dispatcher.cpp`: Dispatcher implementation.
- `message_ids.h`: Shared message ID definitions and indexing helpers.
