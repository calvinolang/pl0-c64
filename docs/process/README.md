# The process record

The working papers of the build, kept so that the story in
[HOW-THIS-WAS-BUILT.md](../HOW-THIS-WAS-BUILT.md) can be checked. They
were written during the work by the controller (the session that ran the
plan with subagent-driven development) and are copied here as they were.

| File | What it is |
|---|---|
| [ledger.md](ledger.md) | The controller's ledger: every dispatch, review result, fix round, ruling, deferred minor finding and decision of the author, in order. A copy of `.superpowers/sdd/2026-11-15-pl0-c64/progress.md` (a git-ignored working directory) taken when Task 18 started, unchanged. |
| [task-12b-brief.md](task-12b-brief.md) | The brief of Task 12b, a task the controller inserted after the Task 12 review: ASCII inside the C64 program, and the first run of the real compiler and VM on the C64. |
| [task-15-design.md](task-15-design.md) | The expanded design of Task 15 (run flow, program list, disk), with the memory rules that the Task 14 numbers made necessary. |
| [task-16-design.md](task-16-design.md) | The expanded design of Task 16 (build products, acceptance gates, CI). |
| [task-17-design.md](task-17-design.md) | The revised Task 17 after the author decided against a browser emulator: media and a static showcase site. |
| [task-18-design.md](task-18-design.md) | The design of Task 18, the documentation, including this record. |

The other documents of the process are in the repository already:

- the design (spec): [docs/superpowers/specs/2026-11-15-pl0-c64-design.md](../superpowers/specs/2026-11-15-pl0-c64-design.md);
- the plan with its amendments: [docs/superpowers/plans/2026-11-15-pl0-c64.md](../superpowers/plans/2026-11-15-pl0-c64.md);
- the git history of the `impl` branch, one or more commits per step.

Not copied: the implementers' reports (`task-N-report.md`, 5 to 384
lines each) and the per-task briefs, which repeat the plan; they stay in
the working directory. Absolute paths of a temporary directory were
replaced by `<scratch directory>` in the copied designs; nothing else
was changed.
