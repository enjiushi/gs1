# src/ui/

Projected UI state and view models consumed by the host/frontend layer.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `hud_view_model.h`: HUD-facing view-model fields for the active runtime/site, including player-facing cash values exposed as cash floats rather than internal cash points.
- `inspect_panel_view_model.h`: Inspect-panel view-model state for selected entities/items.
- `notification_view_model.h`: Notification feed/toast view-model state.
- `phone_view_model.h`: In-game phone/shop/task UI view-model state.
- `regional_map_view_model.h`: Campaign/regional map view-model state.
- `ui_presenter.h`: UI presenter coordination surface between gameplay and host rendering.
- `ui_state.h`: Aggregate UI state container projected by the runtime.
