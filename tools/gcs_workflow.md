# GCS Content Workflow

This repo keeps code and shared gameplay configuration in Git, while large Godot project assets and addon/plugin payloads are restored from external object storage into their expected local paths.

## Recommended Bucket Layout

Use a single bucket prefix with a simple `latest/` path for day-to-day use, plus optional immutable snapshots when you want reproducibility or rollback.

```text
gs://<bucket>/gs1-content/
  latest/
    engines/godot/project/assets/...
    engines/godot/project/addons/...
  snapshots/
    2026-05-05-menu-pass/
      engines/godot/project/assets/...
      engines/godot/project/addons/...
    2026-05-06-terrain-refresh/
      engines/godot/project/assets/...
      engines/godot/project/addons/...
```

`tools/gcs_content.json` is the Git-tracked manifest that points the repo at the storage vendor, bucket prefix, and default live label. The local folders stay the same, so Godot scenes and `project.godot` can keep using `res://assets/...` and `res://addons/...` paths.

Each manifest entry can represent either a `directory` or a `file`, and both the push and pull scripts can operate on one entry or multiple entries in the same command.

## Why This Shape Works

- GitHub remains the source of truth for code, shared gameplay config, scene files, project config, and the sync configuration.
- The configured object storage vendor remains the source of truth for large binaries and third-party plugin payloads.
- The simplest path is syncing `latest`, while explicit snapshots remain available for rollback, bisecting, or reproducing an older content set.

## Setup

- Choose a storage vendor. `gcs` is the default. `cloudflare` is also supported.
- For `gcs`, install the Google Cloud SDK so `gcloud` and `gsutil` are available on `PATH`.
- For `cloudflare`, install `rclone` and configure a remote that points at your Cloudflare R2 bucket.
- Run the first-time setup command to write the vendor and bucket prefix, then verify access.

```powershell
./tools/push_gcs_content.ps1 -Setup -BucketUri gs://your-bucket-name/gs1-content -AuthLogin -AuthApplicationDefault
```

```powershell
./tools/push_gcs_content.ps1 -Setup -Vendor cloudflare -BucketUri your-r2-remote:gs1-content
```

- You can also set a different live label if you do not want to use `latest`.

```powershell
./tools/push_gcs_content.ps1 -Setup -BucketUri gs://your-bucket-name/gs1-content -LatestLabel live
```

## Authentication

Use one of these approaches:

- Local developer machine: run `gcloud auth login`, then `gcloud auth application-default login` if needed by your local setup.
- CI or shared automation: use a service account and set `GOOGLE_APPLICATION_CREDENTIALS` to the JSON key file path.
- Preconfigured environment: if `gsutil` already works in your shell, the sync script will reuse that auth context.

For Cloudflare R2 through `rclone`, configure the remote once with `rclone config`, then point `bucketUri` at the remote path, for example `your-r2-remote:gs1-content`.

Examples:

```powershell
gcloud auth login
gcloud auth application-default login
```

```powershell
$env:GOOGLE_APPLICATION_CREDENTIALS = 'E:\secrets\gs1-gcs-reader.json'
```

The scripts never need your Google account password. Authentication should be handled by the Google Cloud SDK or a service-account credential file outside the repo.

## Daily Push Flow

Push the current local assets/addons into the configured live path:

```powershell
./tools/push_gcs_content.ps1
```

Override the vendor explicitly for one run:

```powershell
./tools/push_gcs_content.ps1 -Vendor cloudflare
./tools/sync_gcs_content.ps1 -Vendor gcs
```

Push only one content group:

```powershell
./tools/push_gcs_content.ps1 -EntryName godot-assets
```

Push multiple content groups in one command:

```powershell
./tools/push_gcs_content.ps1 -EntryName godot-assets, godot-addons
```

Push a named snapshot for rollback or sharing:

```powershell
./tools/push_gcs_content.ps1 -SnapshotId 2026-05-05-menu-pass
```

Preview what would upload without changing GCS:

```powershell
./tools/push_gcs_content.ps1 -DryRun
```

## Daily Pull Flow

1. Pull the GitHub branch as usual.
2. Run `./tools/sync_gcs_content.ps1` from the repo root to pull `latest`, or pass `-SnapshotId <id>` to restore a specific snapshot.
3. Open the Godot project or run the normal build/test flow after the external content has been restored.

Optional examples:

```powershell
./tools/sync_gcs_content.ps1
./tools/sync_gcs_content.ps1 -Latest
./tools/sync_gcs_content.ps1 -SnapshotId 2026-05-05-menu-pass
./tools/sync_gcs_content.ps1 -EntryName godot-assets
./tools/sync_gcs_content.ps1 -EntryName godot-assets, godot-addons
```

## Notes

- The scripts use `gsutil` for `gcs` and `rclone` for `cloudflare`.
- Directory entries use sync-style transfers, while file entries use copy-style transfers.
- For file entries, `-DryRun` prints the planned copy operation instead of mutating GCS or the local file.
- The sync script intentionally excludes `guideline.md` so the repo can keep lightweight documentation in Git while the heavy payload stays external.
- `engines/godot/project/assets/` and `engines/godot/project/addons/` are ignored in Git except for their guideline files.
- The gameplay-authoritative shared config under `project/content/` stays in Git and is not part of the cloud payload.
- If a teammate only changes code or scene wiring and does not touch the external payload, they only need the normal Git push.
