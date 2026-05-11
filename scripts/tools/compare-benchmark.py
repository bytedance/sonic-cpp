#!/usr/bin/env python3
import argparse
import json
import re
import sys

TIME_UNIT_SCALE = {
    "": 1.0,
    "ns": 1e-9,
    "us": 1e-6,
    "ms": 1e-3,
    "s": 1.0,
}


def load_rows(path, include_regex):
    include_pattern = re.compile(include_regex) if include_regex else None
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)

    grouped = {}
    for row in data.get("benchmarks", []):
        name = row.get("run_name") or row.get("name")
        if not name:
            continue
        if include_pattern and not include_pattern.search(name):
            continue
        if name.endswith("_stddev") or name.endswith("_cv"):
            continue
        grouped.setdefault(name, []).append(row)

    selected = {}
    for name, rows in grouped.items():
        choice = next((r for r in rows if r.get("aggregate_name") == "median"),
                      None)
        if choice is None:
            choice = next((r for r in rows if r.get("aggregate_name") == "mean"),
                          None)
        if choice is None:
            choice = rows[0]
        selected[name] = choice
    return selected


def real_time(row):
    try:
        return float(row.get("real_time", 0.0))
    except (TypeError, ValueError):
        return 0.0


def normalized_real_time(row):
    unit = time_unit(row)
    if unit not in TIME_UNIT_SCALE:
        raise ValueError(f"unsupported Google Benchmark time_unit: {unit!r}")
    return real_time(row) * TIME_UNIT_SCALE[unit]


def time_unit(row):
    return row.get("time_unit", "")


def format_time(value, unit):
    if unit:
        return f"{value:.2f} {unit}"
    return f"{value:.2f}"


def row_severity(delta, warn_threshold, critical_threshold):
    if delta > critical_threshold:
        return "critical"
    if delta > warn_threshold:
        return "warning"
    return "info"


def main():
    parser = argparse.ArgumentParser(
        description="Compare two Google Benchmark JSON outputs.")
    parser.add_argument("base")
    parser.add_argument("head")
    parser.add_argument("--include-regex", default="Sonic")
    parser.add_argument("--warn-threshold", type=float, default=3.0)
    parser.add_argument("--fail-threshold", type=float, default=10.0)
    parser.add_argument(
        "--fail-on-critical",
        action="store_true",
        help="Exit non-zero when overall severity is critical.")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    base_rows = load_rows(args.base, args.include_regex)
    head_rows = load_rows(args.head, args.include_regex)
    names = sorted(set(base_rows) & set(head_rows))
    mode = "gating" if args.fail_on_critical else "informational"

    lines = [
        "## Benchmark",
        "",
        f"Mode: `{mode}`",
        f"Warning threshold: `>{args.warn_threshold:g}%` regression",
        f"Critical threshold: `>{args.fail_threshold:g}%` regression",
        f"Included benchmarks: `{args.include_regex or 'all'}`",
        "",
        "| Severity | Benchmark | base real_time | head real_time | delta |",
        "| --- | --- | ---: | ---: | ---: |",
    ]

    severity = "ok"
    emitted = False
    for name in names:
        base_normalized_time = normalized_real_time(base_rows[name])
        head_normalized_time = normalized_real_time(head_rows[name])
        if base_normalized_time <= 0.0:
            continue

        delta = ((head_normalized_time - base_normalized_time) * 100.0 /
                 base_normalized_time)
        if abs(delta) >= args.warn_threshold:
            emitted = True
            base_time = real_time(base_rows[name])
            head_time = real_time(head_rows[name])
            base_unit = time_unit(base_rows[name])
            head_unit = time_unit(head_rows[name])
            item_severity = row_severity(delta, args.warn_threshold,
                                         args.fail_threshold)
            lines.append(
                f"| {item_severity} | `{name}` | "
                f"{format_time(base_time, base_unit)} | "
                f"{format_time(head_time, head_unit)} | {delta:+.2f}% |")
        if delta > args.fail_threshold:
            severity = "critical"
        elif delta > args.warn_threshold and severity != "critical":
            severity = "warning"

    if not names:
        severity = "critical"
        lines.append("| critical | No comparable benchmark rows found. |  |  |  |")
    elif not emitted:
        lines.append("| ok | No benchmark changed by the warning threshold. |  |  |  |")

    lines.extend([
        "",
        f"Overall severity: `{severity}`",
        "",
        ("Conclusion: critical benchmark regressions fail this run."
         if args.fail_on_critical else
         "Conclusion: informational only. Benchmark regressions are reported "
         "in this summary/artifact and do not block the workflow."),
    ])

    with open(args.output, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
        f.write("\n")

    print("\n".join(lines))
    if args.fail_on_critical and severity == "critical":
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
