# Hardware CI

The `test-full` check runs the full test suite, aisio included, on a
hardware target. It is opt-in per PR and human-triggered per run.

A maintainer puts the `test-full` label on the PR. A small workflow
then marks the PR head with a pending `test-full` commit status
("waiting for an operator"). A new push to a labeled PR gets a fresh
pending status, since statuses are per commit.

An operator explicitly runs an approval script on their own machine.
It checks out the PR head and drives the hardware target over SSH
through cijoe, using the same rsync/build/run_tests flow as manual
development (see "Remote testing with CIJOE" in the README). It then
posts the result as a success or failure status on the head commit.
The operator tooling lives with the operator, not in this repository.
