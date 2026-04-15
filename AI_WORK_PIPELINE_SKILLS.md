# General AI Work Pipeline Skills

This document generalizes the original Unity-specific workflow into a tool-agnostic, Git-based AI work pipeline.

## Goal

Run all AI work through a repeatable pipeline:

1. A human proposes or updates a requirement.
2. AI normalizes the requirement into structured workspace artifacts.
3. A scheduled closed loop picks one executable task.
4. AI implements, validates, captures evidence, and updates status.
5. Human review feeds decisions back into the requirement state.
6. The next scheduled round continues from the updated state.

## Core Design Decisions

- Remove Unity-specific wording from the workflow.
- Keep the workflow tied to the currently active workspace detected by the active tool bridge or MCP.
- Use Git, not Perforce.
- Replace Perforce changelist reuse with a Git workstream concept.
- Keep human replies separate from automatic execution until a dedicated review step merges them back.

## Required Workspace Shape

Each active workspace should follow this structure:

```text
<workspace-root>/
  _ai_workspace.md
  requirements/
    <feature-a>/
      requirement.md
      checklist.md
    <feature-b>/
      requirement.md
      checklist.md
    complete/
      <archived-feature>/
  _human_review/
```

Rules:

- `_ai_workspace.md` must exist.
- Only scan direct children that match `requirements/*/checklist.md`.
- Do not recurse beyond one requirement directory level.
- Do not guess other workspaces.
- Do not process `requirements/complete/`.
- Do not process archived directories outside the active scan range.

## Git Replacement For Changelists

The Perforce `Changelist ID:` concept should be replaced by a Git workstream recorded in the checklist header.

Recommended metadata fields:

```text
Requirement ID: <stable-id>
Workstream Branch: <git-branch-name>
Workstream Status: active
Last Round: <yyyy-mm-dd or round-id>
```

If you need stricter isolation, store both:

```text
Workstream Branch: <git-branch-name>
Worktree Path: <optional-dedicated-worktree-path>
```

Behavior:

- If `Workstream Branch:` exists and is valid, reuse it.
- If it is missing or invalid, create or resolve a new branch for that requirement and write it back immediately.
- All iterations for the same requirement should continue on the same Git workstream unless it has been deleted or intentionally replaced.

## Recommended Checklist Statuses

Use ASCII status names so the workflow is stable across editors, terminals, and automation:

- `NEXT`
- `PENDING`
- `DONE`
- `DONE_PENDING_REVIEW`
- `BLOCKED`

If an existing checklist already uses emoji markers, map them to these status names internally.

## Generalized Closed-Loop Behavior

The scheduled execution loop should do the following:

1. Detect the active workspace through the active tool bridge or MCP.
2. Stop immediately if the tool bridge is unavailable, the active workspace cannot be resolved, or the workspace structure is invalid.
3. Validate `_ai_workspace.md` and the allowed requirement scan range.
4. Scan only `requirements/*/checklist.md` in the active workspace.
5. Select exactly one requirement for the round.
6. Inside that requirement, select the next executable task using this priority:
   1. `NEXT`
   2. `PENDING` whose dependencies are satisfied
   3. Later `PENDING` tasks that are independently executable and not blocked by the current dependency chain
7. Never select:
   - `DONE`
   - `DONE_PENDING_REVIEW`
   - `BLOCKED`
   - tasks with unmet dependencies
8. Reuse or create the requirement Git workstream before editing code.
9. Perform all work needed for the chosen task:
   - required code changes
   - build or compile checks
   - tool-backed validation
   - screenshots for visible changes
   - logs and evidence capture
   - checklist updates
10. If successful:
    - update the task to `DONE_PENDING_REVIEW`
    - create `_human_review/REVIEW-TASK-*.md`
11. If blocked by human decisions, missing information, or repeated failures:
    - update the task to `BLOCKED`
    - create `_human_review/QUESTION-*.md`
12. At the end of every round:
    - create `REPORT-round-*.md` for every requirement touched in that round
13. Never directly consume raw human replies from `_human_review/` inside the closed loop.
14. Allow up to 5 self-repair attempts for one failed task.
15. If the task still fails, roll back only the current attempt's changes without damaging earlier valid changes already present in that requirement's Git workstream.
16. Archive a requirement to `requirements/complete/<feature-name>/` only when all conditions are true:
    - all tasks are `DONE`
    - all `_human_review/` items are resolved
    - the requirement no longer needs ongoing AI attention

## Generalized Human-In-The-Loop Flow

```text
Human submits a requirement
  ->
AI uses ai-workspace-conventions to create or validate the workspace structure
  ->
AI uses requirement-alignment to clarify scope, constraints, and validation
  ->
AI writes requirement.md and checklist.md
  ->
Scheduler triggers workspace-ai-closed-loop on a recurring cadence
  ->
AI reads checklist.md and selects one executable task
  ->
AI implements, builds, self-checks, validates, and captures screenshots or logs
  ->
Results branch:
  - Success -> mark task as DONE_PENDING_REVIEW -> write REVIEW-TASK-*.md
  - Decision blocked -> write QUESTION-*.md and mark task as BLOCKED
  - Round complete -> write REPORT-round-*.md
  ->
Human reviews artifacts and responds
  ->
human-review merges human conclusions back into checklist.md
  ->
Next scheduled round continues
```

## Operating Without A Scheduler

Yes. The rest of the skills still work without any periodic task.

The scheduler is only a trigger mechanism for `workspace-ai-closed-loop`. It is not required for the workflow itself.

Manual operation should work like this:

1. Run `ai-workspace-conventions` to create or validate the workspace.
2. Run `requirement-alignment` when a new or updated requirement needs structured artifacts.
3. Run `workspace-ai-closed-loop` manually for one round.
4. After the human responds, run `human-review`.
5. Repeat step 3 as needed.

Recommendation:

- Build and validate the skills first.
- Run several manual rounds before adding automation.
- Add the scheduler only after the manual flow is stable.

## Session Granularity

Recommended default:

- One closed-loop round handles exactly one requirement and one executable task.

Do not try to clear the whole checklist in one session by default.

Reasons:

- smaller diffs
- clearer review artifacts
- safer rollback
- less chance of hidden dependency mistakes
- lower chance of Git conflicts
- easier human oversight

If several tiny checklist items are truly inseparable, combine them during requirement design and represent them as one task. Do not let the closed loop dynamically batch unrelated tasks just because they are available.

## Concurrency And Locking

Recommended rollout path:

### Version 1

- Allow only one active `workspace-ai-closed-loop` session per workspace.
- That session processes one requirement and one task, then exits.

This is the safest starting point and is strongly recommended for the first implementation.

### Version 2

After the workflow is stable, allow multiple sessions in the same workspace only if they work on different requirements and requirement locking is implemented.

Never allow multiple sessions to work on the same requirement at the same time.

Never allow multiple sessions to update the same `checklist.md` concurrently.

Recommendation for locking:

- Use one exclusive lock per requirement.
- Acquire the lock before claiming a task or writing any files.
- If the lock is already held and fresh, skip that requirement.
- If the lock is stale, reclaim it with an audit note.

Suggested lock file locations:

- `requirements/<feature>/.ai-lock.json`
- `_ai_workspace_locks/<feature>.json`

Suggested lock contents:

```json
{
  "session_id": "2026-04-14T15-20-00Z-host-a-1234",
  "requirement_id": "feature-a",
  "task_id": "task-03",
  "branch": "ai/feature-a",
  "started_at": "2026-04-14T15:20:00Z",
  "heartbeat_at": "2026-04-14T15:27:00Z",
  "host": "HOST-A",
  "pid": 1234
}
```

Lock behavior:

1. Scan candidate requirements.
2. Choose one requirement candidate.
3. Try to acquire its lock atomically.
4. Re-read `checklist.md` after the lock is acquired.
5. Confirm the chosen task is still executable.
6. Only then begin implementation.

Suggested stale-lock rule:

- treat a lock as stale only after a conservative timeout, such as 60 to 90 minutes without heartbeat

## Recommendation For Your Use Case

For your first version, I recommend:

1. No scheduler yet.
2. Manual closed-loop runs.
3. One session per workspace.
4. One task per round.
5. No parallel work on the same requirement.

After that works reliably, add:

1. Windows Task Scheduler or another local trigger.
2. Requirement-level locking.
3. Optional parallelism across different requirements only.

## Skills To Create

Start with these four core skills.

### 1. `ai-workspace-conventions`

Purpose:

- Create and validate the standard AI workspace layout.
- Confirm `_ai_workspace.md` exists.
- Enforce the allowed requirement scan scope.
- Refuse execution when the workspace shape is invalid.

Execute when:

- Once when onboarding a new workspace.
- At the start of every automated closed-loop round as a guard check.
- Any time the workspace structure may have drifted.

Outputs:

- `_ai_workspace.md`
- `requirements/<feature>/` directory skeleton when missing
- `_human_review/` directory when missing

### 2. `requirement-alignment`

Purpose:

- Turn a human request into an implementable requirement.
- Clarify boundaries, assumptions, dependencies, acceptance criteria, and validation.
- Generate the requirement task graph in `checklist.md`.
- Seed Git workstream metadata for the requirement.

Execute when:

- A human creates a new requirement.
- A requirement changes significantly.
- A blocked question has been answered and the scope must be rewritten.

Outputs:

- `requirements/<feature>/requirement.md`
- `requirements/<feature>/checklist.md`

### 3. `workspace-ai-closed-loop`

Purpose:

- Act as the scheduled autonomous execution loop.
- Pick one executable requirement task.
- Implement, validate, collect evidence, and update statuses.
- Create review, question, and round-report artifacts.
- Retry self-repair up to the configured limit.

Execute when:

- On every scheduled run.
- On manual "run one round now" requests.

Inputs:

- active workspace context from the tool bridge or MCP
- `_ai_workspace.md`
- `requirements/*/checklist.md`
- existing Git workstream metadata

Outputs:

- code changes
- validation evidence
- screenshots for visible changes
- updated `checklist.md`
- `_human_review/REVIEW-TASK-*.md`
- `_human_review/QUESTION-*.md`
- `REPORT-round-*.md`

### 4. `human-review`

Purpose:

- Read human feedback artifacts.
- Merge accepted conclusions back into `checklist.md`.
- Resolve review items and blocked questions.
- Decide whether tasks become `DONE`, return to `PENDING`, or stay `BLOCKED`.

Execute when:

- A human has answered a `QUESTION-*.md`.
- A human has reviewed a `REVIEW-TASK-*.md`.
- A human wants to close or reopen a requirement after reading a round report.

Outputs:

- updated `checklist.md`
- resolved or superseded `_human_review/` entries

## Optional Supporting Skills

These are useful only if you want more modularity.

### `git-workstream-manager`

Use for:

- branch creation and reuse
- optional worktree management
- scoped rollback of the current attempt

Execute when:

- inside `workspace-ai-closed-loop` before and after implementation work

Recommendation:

- fold this into `workspace-ai-closed-loop` first unless Git isolation becomes complicated

### `validation-evidence-capture`

Use for:

- standardized build, test, screenshot, and log collection
- reusable evidence formatting across different tech stacks

Execute when:

- inside `workspace-ai-closed-loop` after code changes and before final status updates

Recommendation:

- split this out only if multiple teams or stacks need the same evidence format

## Recommended Execution Order

The normal sequence should be:

1. `ai-workspace-conventions`
2. `requirement-alignment`
3. scheduler trigger for `workspace-ai-closed-loop`
4. `human-review`
5. repeat step 3

Inside every scheduled round, the order should be:

1. detect active workspace
2. run `ai-workspace-conventions` validation
3. choose one requirement and one task
4. resolve Git workstream
5. implement and validate
6. write evidence and review artifacts
7. write round report

## Minimal First Version

If you want the leanest possible rollout, create only these first:

1. `ai-workspace-conventions`
2. `requirement-alignment`
3. `workspace-ai-closed-loop`
4. `human-review`

That is enough to support the whole pipeline.
