# Task Board Open Questions

Most of the earlier contract-board questions are now resolved:

- reward drafts should be surfaced through the phone flow
- claimed rewards should move tasks into a separate claimed/history presentation
- unlockable rewards take effect immediately
- item rewards should use the delivery flow
- every task always grants guaranteed publisher `Faction Reputation`
- the immediate reward draft should come from a shared eligible pool, not a faction-specific reward table
- `Site 1` should use an onboarding-focused task pool built around buy, sell, transfer, plant, craft, and consume actions

## Remaining open point

- If onboarding tasks should also have per-task time limits, what should the timer do on expiry?
- Possible behaviors would be: stay visible but stop counting as onboarding, rotate out on board refresh, or fail/expire into history.

Current implementation assumption:

- onboarding tasks do not yet have their own per-task expiry timer
- the shared board refresh timer remains the only time-based board control
