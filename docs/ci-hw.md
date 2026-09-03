# Hardware CI

The `test-full` check runs the full test suite, aisio included, on a
hardware target. It is opt-in per PR and human-triggered per run.

A maintainer puts the `test-full` label on the PR. That starts a
`ci / test-full (contact operator to run)` check gated on the hosted
`lint` and `test-ref` jobs; once they pass, the run waits for
approval in the `manual-operator` environment. Nothing executes on
hardware yet. The check is required, so a labeled PR cannot merge
before the suite passes. A new push to a labeled PR cancels the
superseded run, waiting or mid-suite, and starts a fresh one.

An operator approves the run from their own machine and brings up a
one-shot self-hosted runner that takes exactly that job. The job
checks out the PR and drives the hardware target over SSH through
cijoe, using the same rsync/build/run_tests flow as manual development
(see "Remote testing with CIJOE" in the README). The full log lands on
the workflow run. The operator tooling lives with the operator, not in
this repository.

The `bench` label works the same way and implies `test-full`: one
approval runs the full suite first, and only a passing suite starts
the standard sweep (`scripts/bench/sweep.toml`, opends suite) on the
target. The sweep's report and history land on the `artefacts` branch
(`scripts/bench/artefacts.py --push`). The reference datasets must
already be populated on the target; the operator preflight checks for
them but does not create them.

## Repository setup

One-time GitHub configuration on the repository the workflow runs in:

1. Create the `test-full` and `bench` labels.
2. Create an environment named `manual-operator` with the operator as
   required reviewer.
3. In Settings, Actions, set "Require approval for all outside
   collaborators". This is the security boundary for outside PRs; the
   environment gate is not, since a PR can edit the workflow file.
4. Add `test-full (contact operator to run)` and `bench` to the
   required status checks. A skipped check satisfies the
   requirement, so unlabeled PRs are unaffected. The requirement
   pins the job display names; renaming a job blocks merges as
   "Expected".

The operator-machine setup is documented with the operator tooling.
