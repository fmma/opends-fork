#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Render the sync split ablation: split vs no-split sweep comparison.

This branch carries a commit that short-circuits the sync read split
gate in submit_sync_op(), so every synchronous read executes unsplit on
a single IO worker. Comparing a sweep from this branch against a
split-enabled baseline isolates what splitting contributes per
(dataset, io_threads, queue_depth).

Reproduce:
  1. Baseline (split enabled, the parent of the gate short-circuit
     commit): scripts/rsync.py && scripts/build.py, then
     scripts/bench/run.py --suite opends --mode sync --out <split-dir>.
     The 20260703-35d2765 snapshot on the artefacts branch is such a
     sweep.
  2. No-split (this branch): same commands, --out <nosplit-dir>.
  3. scripts/bench/split_compare.py --split <split-dir>
     --nosplit <nosplit-dir>

Inputs are scanned recursively for history.jsonl, so a raw sweep dir
and an artefacts-branch snapshot dir both work; opends sync records are
kept. Writes report.md (no-split, split, and ratio pivots per dataset)
and report.png (throughput overlay and ratio panels; ratio > 1 means
no-split is faster) to the output dir.
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from _helpers import ROOT, iter_history, ok
from report import (BASELINE, GRIDLINE, INK_MUTED, INK_PRIMARY,
                    INK_SECONDARY, RAMP, SPEC_MIBS, SURFACE)


def _load(in_dir):
    vals = {}
    for _, hist in iter_history([in_dir]):
        for line in hist.read_text().splitlines():
            line = line.strip()
            if not line:
                continue
            rec = json.loads(line)
            cfg, res = rec.get("config", {}), rec.get("result", {})
            if cfg.get("backend") == "opends" and cfg.get("mode") == "sync":
                vals[(cfg.get("dataset"), cfg.get("io_threads"),
                      cfg.get("queue_depth"))] = res.get("mib_s")
    return vals


def _pivot_md(title, values, threads, depths):
    lines = [f"### {title}", "",
             "| io_threads \\ queue_depth | "
             + " | ".join(str(q) for q in depths) + " |",
             "|" + "---|" * (len(depths) + 1)]
    for t in threads:
        cells = []
        for q in depths:
            v = values.get((t, q))
            cells.append("-" if v is None
                         else f"{v:.2f}" if v < 10 else f"{v:.0f}")
        lines.append(f"| {t} | " + " | ".join(cells) + " |")
    lines.append("")
    return lines


def _style_axis(ax, depths):
    ax.set_facecolor(SURFACE)
    ax.set_xscale("log", base=2)
    ax.set_xlim(0.85, 620)
    ax.yaxis.grid(True, color=GRIDLINE, linewidth=0.6)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(BASELINE)
        ax.spines[side].set_linewidth(0.8)
    ax.tick_params(colors=INK_MUTED, labelsize=8, length=3,
                   width=0.8, labelcolor=INK_MUTED)
    ax.set_xticks([1, 8, 64, 512])
    ax.set_xticks(depths, minor=True)
    ax.xaxis.set_major_formatter(lambda v, _: f"{int(v)}")
    ax.xaxis.set_minor_formatter(lambda v, _: "")


def _plot(split, nosplit, datasets, threads, depths, dst):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from matplotlib.lines import Line2D
    except ImportError:
        print("matplotlib not installed; skipping plot", file=sys.stderr)
        return False

    fig, axes = plt.subplots(
        2, len(datasets),
        figsize=(13, 5.8), dpi=160,
        sharex=True, squeeze=False,
        facecolor=SURFACE,
    )
    fig.subplots_adjust(left=0.075, right=0.985, top=0.80, bottom=0.11,
                        hspace=0.22, wspace=0.16)

    for c, ds in enumerate(datasets):
        ax = axes[0][c]
        _style_axis(ax, depths)
        ax.set_ylim(0, 7600)
        ax.set_yticks([0, 2000, 4000, 6000])
        if c:
            ax.set_yticklabels([])
        ax.axhline(SPEC_MIBS, color=BASELINE, linewidth=0.8, zorder=1)
        for t in threads:
            xs = [q for q in depths if split.get((ds, t, q)) is not None]
            ax.plot(xs, [split[(ds, t, q)] for q in xs],
                    color=RAMP[t], linewidth=1.3, linestyle=(0, (4, 2.5)),
                    zorder=2)
            xs = [q for q in depths if nosplit.get((ds, t, q)) is not None]
            ax.plot(xs, [nosplit[(ds, t, q)] for q in xs],
                    color=RAMP[t], linewidth=1.6,
                    marker="o", markersize=3.6,
                    markeredgecolor=SURFACE, markeredgewidth=0.6,
                    zorder=3)
        ax.set_title(ds, fontsize=10.5, color=INK_PRIMARY, pad=8)

        ax = axes[1][c]
        _style_axis(ax, depths)
        ax.set_ylim(0.72, 1.62)
        ax.set_yticks([0.8, 1.0, 1.2, 1.4])
        if c:
            ax.set_yticklabels([])
        ax.axhline(1.0, color=BASELINE, linewidth=0.8, zorder=1)
        for t in threads:
            xs = [q for q in depths
                  if split.get((ds, t, q)) and nosplit.get((ds, t, q))]
            ax.plot(xs,
                    [nosplit[(ds, t, q)] / split[(ds, t, q)] for q in xs],
                    color=RAMP[t], linewidth=1.6,
                    marker="o", markersize=3.6,
                    markeredgecolor=SURFACE, markeredgewidth=0.6,
                    zorder=3)
        ax.set_xlabel("queue depth", fontsize=8.5, color=INK_MUTED)

    axes[0][0].set_ylabel("sync\nMiB/s", fontsize=9, color=INK_SECONDARY,
                          labelpad=6)
    axes[1][0].set_ylabel("ratio\nno-split / split", fontsize=9,
                          color=INK_SECONDARY, labelpad=6)
    axes[0][0].annotate(
        "990 PRO spec seq. read (7450 MB/s)",
        xy=(1, SPEC_MIBS), xytext=(0, 4), textcoords="offset points",
        fontsize=7.5, color=INK_MUTED, va="bottom",
    )
    axes[1][0].annotate(
        "no-split faster above 1.0",
        xy=(1, 1.0), xytext=(0, 4), textcoords="offset points",
        fontsize=7.5, color=INK_MUTED, va="bottom",
    )

    fig.suptitle("Sync read: split vs no-split", x=0.075, y=0.975,
                 ha="left", fontsize=14, color=INK_PRIMARY)
    fig.text(0.075, 0.885,
             "Top: MiB/s by queue depth, no-split (solid) vs split "
             "(dashed). Bottom: no-split / split ratio.",
             fontsize=9.5, color=INK_SECONDARY)

    handles = [
        Line2D([], [], color=RAMP[t], linewidth=1.6, marker="o",
               markersize=3.6, markeredgecolor=SURFACE, markeredgewidth=0.6,
               label=str(t))
        for t in threads
    ] + [
        Line2D([], [], color=INK_SECONDARY, linewidth=1.6, marker="o",
               markersize=3.6, markeredgecolor=SURFACE, markeredgewidth=0.6,
               label="no-split"),
        Line2D([], [], color=INK_SECONDARY, linewidth=1.3,
               linestyle=(0, (4, 2.5)), label="split"),
    ]
    leg = fig.legend(
        handles=handles, title="io_threads",
        loc="upper right", bbox_to_anchor=(0.985, 0.99),
        ncol=6, frameon=False,
        fontsize=8.5, title_fontsize=8.5,
        labelcolor=INK_SECONDARY, alignment="right",
    )
    leg.get_title().set_color(INK_SECONDARY)

    fig.savefig(dst, facecolor=SURFACE)
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--split", default="cijoe-output-sweep", metavar="DIR",
                    help="Split-enabled baseline sweep dir. Default "
                         "cijoe-output-sweep.")
    ap.add_argument("--nosplit", default="cijoe-output-nosplit",
                    metavar="DIR",
                    help="No-split sweep dir (this branch). Default "
                         "cijoe-output-nosplit.")
    ap.add_argument("--out", metavar="DIR",
                    help="Output dir for report.md/report.png. Default: "
                         "the --nosplit dir.")
    ap.add_argument("--no-plot", action="store_true")
    a = ap.parse_args()

    def resolve(d):
        d = Path(d)
        return d if d.is_absolute() else ROOT / d

    split_dir, nosplit_dir = resolve(a.split), resolve(a.nosplit)
    out_dir = resolve(a.out) if a.out else nosplit_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    split, nosplit = _load(split_dir), _load(nosplit_dir)
    if not nosplit:
        sys.exit(f"no opends sync records found under {nosplit_dir}")
    if not split:
        sys.exit(f"no opends sync records found under {split_dir}")

    datasets = sorted({k[0] for k in nosplit})
    threads = sorted({k[1] for k in nosplit})
    depths = sorted({k[2] for k in nosplit})

    md = [
        "# Sync read: split vs no-split",
        "",
        "Effect of splitting large synchronous reads across IO worker",
        "threads. The no-split sweep ran with the split gate in",
        "`submit_sync_op()` short-circuited, so every sync read executes",
        "unsplit on a single worker; the baseline is a split-enabled",
        "sweep of the same grid. Ratio > 1 means no-split is faster.",
        "",
        f"Grid: io_threads {', '.join(map(str, threads))} x queue_depth",
        f"{', '.join(map(str, depths))}, sync mode.",
        "",
    ]
    for ds in datasets:
        md.append(f"## {ds}")
        md.append("")
        ns = {(t, q): nosplit.get((ds, t, q))
              for t in threads for q in depths}
        sp = {(t, q): split.get((ds, t, q))
              for t in threads for q in depths}
        ratio = {k: ns[k] / sp[k] for k in ns if ns[k] and sp[k]}
        md += _pivot_md("no-split (MiB/s)", ns, threads, depths)
        md += _pivot_md("split (MiB/s)", sp, threads, depths)
        md += _pivot_md("ratio no-split / split", ratio, threads, depths)

    report = out_dir / "report.md"
    report.write_text("\n".join(md) + "\n")

    outs = [report]
    if not a.no_plot:
        png = out_dir / "report.png"
        if _plot(split, nosplit, datasets, threads, depths, png):
            outs.append(png)

    print("wrote " + ", ".join(str(p) for p in outs))
    ok("split_compare")


if __name__ == "__main__":
    main()
