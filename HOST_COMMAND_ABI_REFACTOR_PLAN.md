# Host Command ABI Refactor Plan

## Goal

Refactor host-to-gameplay DLL input away from the current `Gs1HostMessage` enum-plus-union transport and toward explicit typed host command entry points, while keeping internal gameplay coordination on compile-time typed message dispatch.

This plan covers the public gameplay DLL input boundary and the runtime/host bridge layers that call into it.

## Target Result

After this refactor:

- Host input no longer centers on `Gs1HostMessage` plus a union payload.
- The gameplay DLL exposes explicit typed host command entry points through concrete exported C ABI functions.
- Public host commands are distinct from internal gameplay messages.
- Internal gameplay coordination continues to use typed POD gameplay messages with compile-time subscriber derivation.
- Exported host command functions translate immediately at the runtime boundary into internal typed gameplay messages.
- Systems subscribe only to internal gameplay messages, not to raw public ABI host command structs.
- The runtime no longer needs a runtime-built host-message subscriber registry for gameplay systems once translated host-message fan-out is fully retired.
- Host bridge code (`src/host/`) calls typed command entry points directly instead of packaging `Gs1HostMessage` arrays for ordinary gameplay input.
- The legacy `Gs1HostMessage` transport path is removed as part of the refactor rather than retained as a compatibility shell.

## Non-Goals

- Do not expose the internal gameplay message bus itself as the public DLL ABI.
- Do not let hosts submit arbitrary internal gameplay follow-up messages such as runtime-owned lifecycle or consequence messages.
- Do not collapse public host commands and internal gameplay coordination into one shared public type universe.
- Do not make exported ABI entry points depend on C++ template instantiation by callers.
- Do not replace read-only state/query APIs with message traffic; host reads remain reads while host-to-runtime interaction remains message-based.
- Do not keep the legacy `Gs1HostMessage` transport alive as a compatibility bridge after the new typed host command ABI lands.

## Design Summary

### 1. Public host input becomes typed commands

The current host ABI uses:

- `gs1_submit_host_messages(...)`
- `Gs1HostMessageType`
- `Gs1HostMessagePayload`
- `Gs1HostMessage`

The target direction is a public host command surface built from concrete typed command structs and concrete exported functions.

Example direction:

```cpp
struct Gs1StartNewCampaignCommand
{
    std::uint64_t seed;
    std::uint32_t slot;
};

GS1_API Gs1Status gs1_start_new_campaign(
    Gs1RuntimeHandle* runtime,
    const Gs1StartNewCampaignCommand* command) GS1_NOEXCEPT;
```

The public ABI remains plain C-compatible data and functions.

### 2. Exported ABI stays concrete, not templated

The gameplay DLL ABI should remain concrete and explicit.

Implementation code may use internal templates or overload sets to avoid repetitive translation logic, but exported symbols should remain normal concrete functions.

Preferred direction:

- exported ABI: concrete `extern "C"` functions
- internal implementation: shared templated or overloaded translation helpers

### 3. Public host commands and internal gameplay messages stay separate

Public host commands are API inputs.
Internal gameplay messages are gameplay coordination contracts.

They may often map one-to-one, but they should remain separate concepts so the gameplay DLL does not freeze internal message semantics as part of the host ABI.

Example direction:

```cpp
Gs1Status gs1_start_new_campaign(...command...)
{
    return submit_host_command_impl(runtime, *command);
}

template <>
Gs1Status submit_host_command_impl(
    GameRuntime& runtime,
    const Gs1StartNewCampaignCommand& command)
{
    return runtime.handle_message(StartNewCampaignMessage {
        command.seed,
        command.slot});
}
```

### 4. Systems subscribe only to internal typed gameplay messages

Gameplay systems should not subscribe to raw public host command structs.

Instead:

- public host command entry points decode the command
- runtime boundary translates to internal gameplay semantics
- compile-time typed gameplay subscriber dispatch runs the subscribed systems

This keeps:

- gameplay ownership boundaries
- compile-time subscriber derivation
- freedom to change internal message topology without changing host ABI

### 5. Host-to-runtime interaction remains message-based

Host reads can continue to use direct state-view and query APIs.

But host-to-runtime interaction should stay message-based all the way through the boundary:

- host submits a typed host command
- runtime boundary translates that command into an internal typed gameplay message
- subscribed systems process that message through compile-time subscriber dispatch

This means host-driven movement, UI actions, site actions, and similar interaction paths should not bypass the message model through direct runtime-owned setter APIs just because the data is small or frequent.

### 6. Legacy host-message transport is removed during the refactor

This refactor targets a clean cut rather than a long-lived dual-path migration.

That means:

- `gs1_submit_host_messages(...)` is removed as part of the refactor
- `Gs1HostMessageType`, `Gs1HostMessagePayload`, and `Gs1HostMessage` stop being the public input path
- runtime host-message queueing and host-message subscriber fan-out tied to that transport are deleted rather than preserved behind a compatibility layer

Intermediate implementation steps can be buildable while the refactor is in flight, but the plan does not preserve a supported compatibility shell as part of the target design.

### 7. Host bridge code should move to direct typed command calls

`src/host/` currently centers on host-message submission into the gameplay DLL.

The target direction is:

- loader function table grows typed command function pointers
- runtime session exposes typed host command wrappers
- hosts and adapters stop constructing `Gs1HostMessage` payload envelopes for ordinary gameplay input

This makes host-side code clearer and removes union-payload plumbing from the primary path.

## Migration Direction

The migration should keep intermediate states buildable and host callers functional while the ABI evolves.

Recommended order:

1. Introduce typed host command structs and exported functions for a first vertical slice.
2. Implement shared internal translation helpers from public command structs to internal typed gameplay messages.
3. Update `src/host/` loader/session layers to call the new entry points.
4. Move additional host input families slice by slice until the typed command surface covers all supported host interaction.
5. Switch host bridge/session callers to the typed command entry points.
6. Remove `gs1_submit_host_messages(...)` plus the old host-message transport types and runtime dispatch infrastructure in the same refactor.
7. Remove any leftover docs, tests, wrappers, and naming tied to the retired host-message transport.

## Progress Tracker

Legend:

- `[ ]` not started
- `[>]` in progress
- `[x]` complete

## Current Notes

- [x] Internal gameplay coordination already has compile-time typed subscriber derivation through the canonical ordered `GameSystems` pack.
- [x] The runtime translates public typed host commands at the boundary into internal gameplay messages.
- [x] The public DLL host input ABI no longer centers on `gs1_submit_host_messages(...)` or `Gs1HostMessage`.
- [x] Host bridge code in `src/host/` calls typed command entry points directly.
- [x] Runtime host-message queueing and host-message subscriber registration infrastructure tied to the legacy ABI surface have been removed.
- [x] The clean-refactor target to remove that legacy transport outright has been completed.

## Implementation Status

Status: complete

Completed implementation:

- [x] Added concrete exported host command entry points for gameplay action, move direction, site action request/cancel, site context request, inventory slot tap, and site scene ready.
- [x] Cut `GameRuntime` over to a typed pending host-command queue and direct boundary translation into internal gameplay messages.
- [x] Removed runtime host-message subscriber registration and the old `IRuntimeSystem` host-message virtual hooks.
- [x] Migrated host bridge/session, smoke host, Godot adapter call sites, and focused runtime tests to the typed command ABI.
- [x] Removed `gs1_submit_host_messages(...)` and the retired public `Gs1HostMessage` enum/union transport types.
- [x] Kept host-local storage-view handling out of the runtime command path and renamed that helper shape to `Gs1SiteStorageViewRequest`.
- [x] Removed legacy-only code/comments/guideline text tied to the deleted host-message transport.

Verification completed:

- [x] Focused build passed for `gs1_gameplay_core`, `gs1_runtime_projection_test`, `gs1_runtime_message_chain_test`, `gs1_runtime_system_trait_metadata_test`, `gs1_smoke_engine_host_threading_test`, and `gs1_smoke_host`.
- [x] Code search confirms the removed runtime host-message interface and public host-message transport types no longer exist in implementation code.

Remaining follow-up outside this refactor:

- [ ] Run broader test coverage beyond the focused build targets if we want extra post-refactor confidence.

## Work Items

All checklist items below are intentionally marked `[x]` because the planned refactor scope in this document has been completed. The only unchecked item in this file is an optional broader test follow-up outside the core refactor scope.

### Phase 1: Public ABI shape

- [x] Decide the authoritative naming scheme for public typed host command structs and exported functions.
- [x] Define the first command families to migrate as vertical slices.
- [x] Decide the message shape for each existing host interaction family so all host-to-runtime interaction enters gameplay through typed command/message translation.
- [x] Decide the cutover/removal plan for `gs1_submit_host_messages(...)` and the legacy host-message transport so the refactor lands as a clean replacement.

### Phase 2: Public header scaffolding

- [x] Add typed host command structs to the public API headers.
- [x] Add concrete exported function declarations for the first migrated host command families.
- [x] Keep ABI-safe trivial layout assertions for each new public command struct where appropriate.
- [x] Document that exported host command functions are the supported input path.

### Phase 3: Runtime boundary translation

- [x] Add shared internal translation helpers from each public host command struct to the correct internal typed gameplay message.
- [x] Ensure implementation templates or overloads stay internal and are not part of the ABI contract.
- [x] Route the first migrated command families through the shared translation helpers.
- [x] Preserve existing validation behavior and status-code semantics during translation.

### Phase 4: Host bridge adoption

- [x] Extend the gameplay DLL loader function table with the new exported command entry points.
- [x] Add typed wrapper methods on the shared runtime session for migrated command families.
- [x] Update smoke/adapter/test callers to use typed command wrappers instead of constructing `Gs1HostMessage` payloads for those migrated families.
- [x] Remove host-side reliance on `Gs1HostMessage` packaging rather than preserving thin compatibility helpers.

### Phase 5: Runtime host dispatch cleanup

- [x] Stop registering gameplay systems against raw host-message subscriber tables for command families that are fully translated through typed entry points.
- [x] Remove obsolete runtime host-message queueing paths for migrated command families.
- [x] Remove the remaining raw-host-message handling surface entirely as part of the clean cutover.
- [x] Remove the host-message subscriber registry once typed command entry points fully replace the legacy transport.

### Phase 6: Compatibility retirement

- [x] Migrate all active callers off `gs1_submit_host_messages(...)`.
- [x] Delete `gs1_submit_host_messages(...)` and the retired `Gs1HostMessage` union-payload transport from the public input ABI.
- [x] Delete retired translation shims instead of preserving compatibility-only forwarding code.
- [x] Refresh docs and guidelines to describe the new host command ABI as the only supported design.

### Phase 7: Legacy code removal

- [x] Remove retired public host-message structs, aliases, enums, and payload-union members that no longer belong to any supported compatibility path.
- [x] Remove dead runtime host-message queueing, subscriber-registration, and dispatch helpers once all supported input families use typed command entry points.
- [x] Remove dead host-bridge/session wrappers that only existed to package `Gs1HostMessage` payload envelopes.
- [x] Remove dead tests and fixtures that only validate deleted legacy transport paths.
- [x] Remove obsolete comments, docs, and naming that describe `Gs1HostMessage` transport as the normal architecture after the refactor is complete.

### Phase 8: Regression coverage

- [x] Add focused API/runtime tests for each migrated typed host command entry point.
- [x] Add regression coverage that public typed commands still reach the same internal typed gameplay subscribers as before.
- [x] Add host-bridge/session tests covering loader symbol resolution and direct typed command calls.

## Exit Criteria

The refactor is complete when all of the following are true:

- The preferred public gameplay DLL input surface uses concrete typed host command entry points instead of `Gs1HostMessage` union payload transport.
- Exported ABI functions are concrete and stable, while any implementation templates remain internal only.
- Systems subscribe only to internal typed gameplay messages or other internal runtime-owned semantics, never to raw public host command structs.
- Boundary translation from public host commands into internal gameplay semantics happens immediately and directly.
- All supported host-to-runtime interaction paths enter gameplay through typed command-to-message translation rather than direct setter-style mutation APIs.
- Host bridge/session code no longer needs to construct `Gs1HostMessage` payload envelopes for ordinary gameplay command input.
- `gs1_submit_host_messages(...)` and the legacy `Gs1HostMessage` input transport are removed rather than retained behind a compatibility layer.
- Retired legacy host-message transport code has been deleted rather than left dormant once the supported refactor path is complete.
- Tests cover the new public command surface and its translation behavior without depending on a legacy compatibility bridge.

## Resolved Design Decisions

- Public host commands and internal gameplay messages are separate layers even when they map closely.
- The public ABI uses concrete exported C functions, not exported templates.
- Internal templates or overloads are allowed as implementation helpers behind concrete ABI entry points.
- Compile-time subscriber derivation remains an internal gameplay-runtime mechanism keyed by internal typed gameplay messages rather than by public ABI command structs.
- Host reads may remain direct state/query APIs, but host-to-runtime interaction remains message-based and should not bypass typed message translation through direct setter-style mutation entry points.
