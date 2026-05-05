# project/

Shared project-owned configuration and content that is not specific to one engine adapter.

## Usage
- Read this file before scanning the folder in detail.
- If you open a child folder, read its `guideline.md` first when present.
- When files or folders in this directory change, update this file in the same change.

## Contents
- `content/`: Shared gameplay-authored TOML configuration loaded by hosts and engine adapters during gameplay DLL initialization, including campaign, site, unlock, tuning, task, reward, and technology tables that now live outside any engine-specific project shell.
