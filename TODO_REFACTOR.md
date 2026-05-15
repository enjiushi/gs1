# Refactor Todo

- [ ] Remove the remaining broad compatibility aggregate shapes if we decide they are no longer needed by tests or adapter code.
- [ ] Replace any remaining compatibility-only pointer-shaped handles with pointer-free ids or explicit runtime-owned access where practical.
- [ ] Decide whether the `StateManager` default-resolver baseline should be implemented now or kept as future work.
- [ ] Review the remaining MSVC `C4324` alignment warnings and decide whether they are acceptable or worth suppressing.
- [ ] Sweep the remaining test helpers that seed `SiteRunState` to make sure they still match the final compatibility contract.
