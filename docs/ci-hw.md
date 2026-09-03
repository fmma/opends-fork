# Hardware CI

The `test-full` check runs the full test suite, aisio included, on a
hardware target. It is opt-in per PR and human-triggered per run.

A maintainer puts the `test-full` label on the PR. The `test-full`
workflow then starts a run that waits for approval in the `test-full`
environment; nothing executes yet. A new push to a labeled PR starts
a fresh waiting run.

An operator approves the run from their own machine and brings up a
one-shot self-hosted runner that takes exactly that job. The job
checks out the PR and drives the hardware target over SSH through
cijoe, using the same rsync/build/run_tests flow as manual development
(see "Remote testing with CIJOE" in the README). The full log lands on
the workflow run. The operator tooling lives with the operator, not in
this repository.
