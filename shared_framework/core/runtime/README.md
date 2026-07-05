# Core Runtime

Engine-free runtime framework code.

This layer is intended for reusable execution, timing, state-ownership, and messaging infrastructure that does not depend on one game's systems or any engine SDK headers.

Current shared areas:

- neutral runtime lifecycle and split-step ABI declarations under `api/`
- low-level runtime utilities such as fixed-step, ids, random streams, and time-scale helpers
- compile-time `type_list` and `system_pack` primitives
- generic system traits for subscribed messages, owned states, and accessed states
- generic state descriptor, state storage/catalog, contract validation, and `state_manager<SystemPack>` primitives

Intended direction:

- games declare their concrete systems in a compile-time `system_pack`
- each system declares its subscribed messages, owned state descriptors, and accessed state keys
- the shared framework derives the concrete state catalog and message subscriber graph from that pack
- duplicate subscribed messages, duplicate state claims, multi-owner states, and accessed-but-unowned states are rejected at compile time
- runtime ownership enforcement then operates on that derived catalog rather than on a hand-maintained global list
