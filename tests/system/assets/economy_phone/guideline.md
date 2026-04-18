# tests/system/assets/economy_phone/

Asset-driven economy phone regressions around listings and purchase outcomes.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `insufficient_money_hire_rejected.gs1systemtest`: Verifies hires/purchases are rejected when funds are insufficient.
- `missing_listing_not_found.gs1systemtest`: Verifies a missing listing resolves to a not-found outcome.
- `purchase_unlockable_listing.gs1systemtest`: Verifies purchasing a valid unlockable listing succeeds.
