# tools/

Repository-maintenance and infrastructure helpers that are not part of the normal gameplay/test script workflow.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `gcs_common.ps1`: Shared Google Cloud Storage manifest plus `gcloud` and `gsutil` helper functions used by the content sync/push scripts.
- `gcs_content.json`: Git-tracked manifest that points to the configured external-content storage prefix and vendor for ignored assets such as the Godot project assets and addons.
- `gcs_workflow.md`: Team-facing notes for the external-content bucket layout, vendor-specific setup, sync flow, and authentication setup.
- `push_gcs_content.ps1`: Uploads the configured external-content entries into the configured storage vendor `latest` path or a named snapshot, and also exposes a first-time `-Setup` mode for bucket/auth verification.
- `sync_gcs_content.ps1`: Restores the configured external-content entries from the configured storage vendor into the ignored local folders used by the repo.
