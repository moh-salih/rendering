#!/usr/bin/env python3
"""
visualize_fps.py
----------------
Plot FPS logs produced by the TexGAN texture-loading tests.

Log filename pattern
    fps_<approach>_<texCount>tex_<timestamp>.csv
Examples
    fps_single_5tex_20250627T130512.csv
    fps_shared_200tex_20250627T131404.csv

Each CSV has two columns:  Time(s),FPS
"""

import re
import argparse
from pathlib import Path
from typing import Dict, List

import pandas as pd
import matplotlib.pyplot as plt


# ---------- Helpers ----------------------------------------------------------


LOG_PATTERN = re.compile(
    r"^fps_(?P<approach>single|shared)_"        # approach
    r"(?P<count>\d+)tex_"                       # texture batch size
    r"(?P<stamp>\d{8}T\d{6})\.csv$",            # timestamp
    re.IGNORECASE
)


def discover_logs(directory: Path) -> Dict[str, Dict[int, List[Path]]]:
    """
    Walk `directory` and group log paths as
        logs[approach][texCount] -> [Path, Path, ...]
    """
    logs: Dict[str, Dict[int, List[Path]]] = {"single": {}, "shared": {}}

    for file in directory.glob("fps_*.csv"):
        m = LOG_PATTERN.match(file.name)
        if not m:
            continue

        approach = m.group("approach").lower()
        count    = int(m.group("count"))

        logs.setdefault(approach, {}).setdefault(count, []).append(file)

    return logs


def load_runs(paths: List[Path]) -> List[pd.DataFrame]:
    """Read each CSV to a DataFrame and return the list."""
    return [pd.read_csv(p) for p in paths]


# ---------- Plotting ---------------------------------------------------------


def plot_time_series(ax, df: pd.DataFrame, label: str, colour):
    """Time on X‐axis, FPS on Y‐axis."""
    ax.plot(df["Time(s)"], df["FPS"], label=label, linewidth=1.2, color=colour)


def plot_bar(ax, pos, value, label, colour):
    ax.bar(pos, value, label=label, color=colour, width=0.4)


# ---------- Main -------------------------------------------------------------


def main():
    ap = argparse.ArgumentParser(
        description="Visualise FPS logs for single vs shared texture loaders"
    )
    ap.add_argument(
        "-d", "--dir", default="../assets/log",
        help="Directory that contains the fps_*.csv files (default: assets/log)"
    )
    ap.add_argument(
        "--show", action="store_true",
        help="Show the plot window instead of saving to PNG"
    )
    ap.add_argument(
        "-o", "--out", default="fps_summary.png",
        help="Output PNG filename when --show is not given (default: fps_summary.png)"
    )
    args = ap.parse_args()

    log_dir = Path(args.dir).expanduser()
    if not log_dir.is_dir():
        raise SystemExit(f"✗ Directory not found: {log_dir}")

    logs = discover_logs(log_dir)
    if not any(logs[appr] for appr in logs):
        raise SystemExit("✗ No matching fps_*.csv files were found.")

    # --- Matplotlib layout ----------------------------------------------------

    fig = plt.figure(constrained_layout=True, figsize=(12, 6))
    gs = fig.add_gridspec(1, 2, width_ratios=[2, 1])
    ax_time = fig.add_subplot(gs[0, 0])
    ax_bar  = fig.add_subplot(gs[0, 1])

    colours = {"single": "#1f77b4",  # blue
               "shared": "#d62728"}  # red

    # --- Plot every run -------------------------------------------------------

    bar_positions = []      # x locations for bars
    bar_labels    = []      # “N textures”
    bar_single    = []      # mean FPS for single
    bar_shared    = []      # mean FPS for shared

    for approach, colour in colours.items():
        for count in sorted(logs[approach]):
            runs = load_runs(logs[approach][count])

            for idx, df in enumerate(runs):
                label = f"{approach}-{count}tex run{idx+1}"
                plot_time_series(ax_time, df, label, colour)

            # Average FPS for this run group
            all_fps = pd.concat(runs)["FPS"]
            mean_fps = all_fps.mean()

            if approach == "single":
                bar_single.append(mean_fps)
            else:
                bar_shared.append(mean_fps)

        # keep bar positions & labels only once
        if not bar_positions:
            counts_sorted = sorted(set().union(*[logs[a].keys() for a in logs]))
            bar_positions = list(range(len(counts_sorted)))
            bar_labels    = [f"{c} tex" for c in counts_sorted]

    # --- Bar chart ------------------------------------------------------------

    width = 0.35
    ax_bar.bar(
        [p - width / 2 for p in bar_positions],
        bar_single, width, label="single", color=colours["single"]
    )
    ax_bar.bar(
        [p + width / 2 for p in bar_positions],
        bar_shared, width, label="shared", color=colours["shared"]
    )

    ax_bar.set_xticks(bar_positions, bar_labels, rotation=45, ha="right")
    ax_bar.set_ylabel("Mean FPS")
    ax_bar.legend(title="Approach", frameon=False)

    # --- Decorate time-series subplot ----------------------------------------

    ax_time.set_xlabel("Elapsed time (s)")
    ax_time.set_ylabel("FPS")
    ax_time.set_title("FPS-vs-time for every run")
    ax_time.legend(fontsize="small", frameon=False)

    fig.suptitle("TexGAN Texture Streaming Performance", fontsize=14)

    # --- Show or save --------------------------------------------------------

    if args.show:
        plt.show()
    else:
        plt.savefig(args.out, dpi=150)
        print(f"✓ Saved figure to {args.out}")


if __name__ == "__main__":
    main()
