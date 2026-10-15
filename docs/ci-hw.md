# Hardware CI

The `test-full` check runs the full test suite, aisio included, on a
hardware machine. It is opt-in per PR.

A maintainer puts the `test-full` label on the PR. The label starts a
`ci / test-full` check. The check waits for the hosted `lint` and
`test-ref` jobs. When they pass, the job runs on the hardware
machine's persistent runner. The job first takes the machine's
labgrid place, as `gh-run-<run id>`, and fails naming the holder if
someone else has it. It then checks that the stack is down: no HOMI
process running, nothing mounted over ublk, and the NVMe controller
on the kernel driver. Otherwise the job fails before it installs
anything, names what is in the way, and leaves the machine alone. The
place is given back when the job ends, cancelled or not. Re-run the
job once the machine is free. The check is required, so a labeled PR
cannot merge before the suite passes. A new push or a label change
cancels the superseded run, queued or mid-suite, and starts a fresh
one.

The job runs the PR's own workflow and scripts as root on the machine.
The label is the only gate, and it stays on the PR, so later pushes by
the author run on the hardware without another look. The machine is
therefore sacrificial: it holds no credentials and nothing trusts it.
GitHub's setting for workflows from fork pull requests, which can
require approval for outside collaborators, covers these jobs like the
hosted ones.

The job checks out the PR on the machine. It drives the hardware
through cijoe over a local SSH loop (`root@localhost`). This is the
same rsync/build/run_tests flow as manual development (see "Remote
testing with CIJOE" in the README). The full log lands on the
workflow run.

The bench labels work the same way. `bench` runs one leg at the
default knobs, `bench-sweep` runs the standard sweep
(`scripts/bench/sweep.toml`), and `bench-full-sweep` runs the whole
knob cross product. Each uploads its cijoe output, `report.md`
included, as a workflow artifact on the run. The reference datasets
must already be present on the machine.

## Repository setup

One-time GitHub configuration on the repository the workflow runs in:

1. Create the `test-full`, `bench`, `bench-sweep`, and
   `bench-full-sweep` labels.
2. Add the four checks to the required status checks. A skipped
   check satisfies the requirement, so unlabeled PRs are unaffected.

## Runner machine

One-time setup on the hardware machine, as an unprivileged user:

1. Bring the machine to the target requirements ("Remote testing with
   CIJOE" in the README). Populate the reference datasets for bench.
2. Install the GitHub Actions runner under `~/actions-runner`.
   Register it with an extra `nvme-cuda` label. Install it as a
   service (`sudo ./svc.sh install <user> && sudo ./svc.sh start`) and
   leave it running: GitHub deletes the registration of a runner that
   has not connected for 14 days. The stack check, not a stopped
   service, is what keeps a labeled run off a machine in use.
3. Create the local SSH loop: Make a passphrase-less key pair for the
   runner user and authorize it for `root@localhost`.
4. Copy `configs/{test,transport}.toml.example` to
   `~/.config/opends-ci/{test,transport}.toml` and fill them in. Set
   `localhost` as the hostname. Use a `repo_path` and a `build_dir`
   dedicated to CI. The workflow copies the files into the checkout.
5. Install cijoe and the python dependencies of `scripts/` for the
   runner user. `config.sh` records the PATH of the shell that runs it
   in the runner's `.path`, so configure from a shell that finds them.
6. Check that rsync on the machine is 3.2.3 or newer: `scripts/rsync.py`
   passes `--mkpath`.
7. Install [labgrid](https://labgrid.readthedocs.io/), run
   `labgrid-coordinator` as a service bound to localhost, and create a
   place for the machine. Put `LG_PLACE=<place>` in the runner's
   `.env`. Everyone who uses the machine acquires that place first;
   the job holds it with `labgrid-client acquire` and `release-from`.
