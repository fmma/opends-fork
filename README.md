# OpenDS benchmark artefacts

Generated bench data published by `scripts/bench/artefacts.py`. Orphan branch: no shared history with the code.

- `latest/` mirrors the newest snapshot's report, overwritten each publish.
- `snapshots/<date>-<sha>/` is one immutable snapshot per publish: `report.md`, `report.png`, `sweep.csv`, and `history.jsonl` (all legs concatenated).
- `custom-experiments/<name>/` holds one-off, manually run experiments not driven by the publish pipeline.

## Snapshots

- [`20260827-dcdd300`](snapshots/20260827-dcdd300/report.md)
- [`20260707-e19b884`](snapshots/20260707-e19b884/report.md)
- [`20260703-35d2765`](snapshots/20260703-35d2765/report.md)
- [`20260703-1cc09ec`](snapshots/20260703-1cc09ec/report.md)

## Custom experiments

- [`cufile-buf-register`](custom-experiments/cufile-buf-register/report.md) — cuFile buffer registration vs no-registration across datasets.
