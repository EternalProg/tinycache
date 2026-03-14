#!/usr/bin/env python3
import argparse
import csv
import datetime as dt
import json
import os
import re
import shlex
import socket
import subprocess
import sys
from pathlib import Path
from shutil import which

ROOT_DIR = Path(__file__).resolve().parents[1]
DEFAULT_PORT = 8080

PERF_EVENTS_DEFAULT = (
    "cycles,instructions,task-clock,context-switches,cpu-migrations,page-faults,"
    "branch-instructions,branch-misses,cache-references,cache-misses,"
    "L1-dcache-loads,L1-dcache-load-misses,"
    "L1-dcache-stores,L1-dcache-store-misses,"
    "L1-icache-loads,L1-icache-load-misses,"
    "dTLB-loads,dTLB-load-misses,"
    "iTLB-loads,iTLB-load-misses,"
    "LLC-loads,LLC-load-misses,"
    "LLC-stores,LLC-store-misses,"
    "bus-cycles,mem-loads,mem-stores"
)

MODE_CONFIG = {
    "read_heavy": {
        "get_pct": 80,
        "set_pct": 15,
        "del_pct": 5,
        "default_requests": 10000,
        "default_warmup": 5000,
        "label": "Read-heavy",
    },
    "resp": {
        "get_pct": 0,
        "set_pct": 0,
        "del_pct": 0,
        "default_requests": 0,
        "default_warmup": 0,
        "label": "RESP",
    },
    "balanced": {
        "get_pct": 40,
        "set_pct": 40,
        "del_pct": 20,
        "default_requests": 100000,
        "default_warmup": 50000,
        "label": "Balanced",
    },
    "write_heavy": {
        "get_pct": 20,
        "set_pct": 70,
        "del_pct": 10,
        "default_requests": 10000,
        "default_warmup": 0,
        "label": "Write-heavy",
    },
    "lru_mt": {
        "get_pct": 0,
        "set_pct": 0,
        "del_pct": 0,
        "default_requests": 0,
        "default_warmup": 0,
        "label": "LruShard MT",
    },
}

PERF_TEXT_UNITS = {
    "msec",
    "sec",
    "usec",
    "nsec",
    "K/sec",
    "M/sec",
    "G/sec",
    "GHz",
}


def run_command(args, env=None, cwd=None):
    result = subprocess.run(args, env=env, cwd=cwd, check=False)
    if result.returncode != 0:
        sys.exit(result.returncode)


def which_or_exit(binary, hint=None):
    if not which(binary):
        msg = f"ERROR: '{binary}' not found in PATH"
        if hint:
            msg = f"{msg}. {hint}"
        print(msg, file=sys.stderr)
        sys.exit(1)


def git_info():
    def _run(cmd):
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            return None
        return result.stdout.strip()

    sha = _run(["git", "rev-parse", "HEAD"])
    is_dirty = bool(_run(["git", "status", "--porcelain"]))
    return {"sha": sha, "dirty": is_dirty} if sha else None


def parse_args():
    parser = argparse.ArgumentParser(description="TinyCache benchmark CLI")
    sub = parser.add_subparsers(dest="command", required=True)

    run = sub.add_parser("run", help="Run benchmarks")
    run.add_argument("--mode", required=True, choices=MODE_CONFIG.keys())
    run.add_argument("--out", required=True, help="Output directory")
    run.add_argument("--suite", default="all", choices=["redis", "gbench", "all"])
    run.add_argument("--perf", action="store_true", help="Run with perf stat")
    run.add_argument("--perf-format", default="csv", choices=["csv", "text"])
    run.add_argument("--perf-delim", default=",", help="CSV delimiter for perf")
    run.add_argument("--perf-events", default=None, help="Comma-separated perf events")
    run.add_argument("--perf-args", default=None, help="Extra perf stat args")
    run.add_argument("--gbench-format", default="json", choices=["json", "csv"])
    run.add_argument("--gbench-args", default=None, help="Extra gbench args")

    run.add_argument("--host", default=None)
    run.add_argument("--port", type=int, default=None)
    run.add_argument("--clients", type=int, default=None)
    run.add_argument("--requests", type=int, default=None)
    run.add_argument("--keyspace", type=int, default=None)
    run.add_argument("--value-size", type=int, default=None)
    run.add_argument("--warmup", type=int, default=None)

    compare = sub.add_parser("compare", help="Compare benchmark outputs")
    compare.add_argument("--left", required=True, help="Left run directory")
    compare.add_argument("--right", required=True, help="Right run directory")
    compare.add_argument("--format", default="table", choices=["table", "json"])
    compare.add_argument("--out", default=None, help="Write comparison output to file")

    return parser.parse_args()


def resolve_env_int(name, default):
    if name in os.environ and os.environ[name].strip() != "":
        try:
            return int(os.environ[name])
        except ValueError:
            pass
    return default


def resolve_env_str(name, default):
    value = os.environ.get(name)
    return value if value else default


def build_perf_command(perf_out, perf_format, perf_events, perf_args, perf_delim):
    cmd = ["perf", "stat", "-o", perf_out, "-e", perf_events]
    if perf_format == "csv":
        cmd.insert(2, f"-x{perf_delim}")
    if perf_args:
        cmd.extend(perf_args)
    return cmd


def run_with_perf(command, perf_out, perf_format, perf_events, perf_args, perf_delim):
    perf_cmd = build_perf_command(
        perf_out=perf_out,
        perf_format=perf_format,
        perf_events=perf_events,
        perf_args=perf_args,
        perf_delim=perf_delim,
    )
    env = os.environ.copy()
    if perf_format == "csv":
        env["LC_ALL"] = "C"
    run_command(perf_cmd + ["--"] + command, env=env)


def check_server(host, port, timeout=1.0):
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True, None
    except OSError as exc:
        return False, exc


def run_redis_benchmark(
    out_csv,
    mode,
    host,
    port,
    clients,
    requests,
    keyspace,
    value_size,
    warmup,
):
    which_or_exit("redis-benchmark")
    ok, err = check_server(host, port)
    if not ok:
        print(
            f"ERROR: Cannot connect to {host}:{port} ({err}). "
            "Set --port or PORT=... and ensure ./build/tinycache is running.",
            file=sys.stderr,
        )
        sys.exit(1)
    config = MODE_CONFIG[mode]
    if requests is None:
        requests = config["default_requests"]
    if warmup is None:
        warmup = config["default_warmup"]

    get_req = requests * config["get_pct"] // 100
    set_req = requests * config["set_pct"] // 100
    del_req = requests * config["del_pct"] // 100

    out_path = Path(out_csv)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("")

    def run_one(test, count):
        cmd = [
            "redis-benchmark",
            "-h",
            host,
            "-p",
            str(port),
            "-t",
            test,
            "-n",
            str(count),
            "-c",
            str(clients),
            "-r",
            str(keyspace),
        ]
        if test != "del":
            cmd.extend(["-d", str(value_size)])
        cmd.append("--csv")

        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(result.stderr, file=sys.stderr)
            sys.exit(result.returncode)
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")

        lines = result.stdout.strip().splitlines()
        if not lines:
            return
        with out_path.open("a", encoding="utf-8") as fh:
            if out_path.stat().st_size == 0:
                fh.write("\n".join(lines) + "\n")
            else:
                fh.write("\n".join(lines[1:]) + "\n")

    print(
        f"Target: {host}:{port} (clients={clients}, requests={requests})",
        flush=True,
    )

    if warmup and warmup > 0:
        print(f"Warmup: SET {warmup}", flush=True)
        run_one("set", warmup)

    print(
        f"{config['label']} mix: GET {get_req}, SET {set_req}, DEL {del_req}",
        flush=True,
    )
    run_one("get", get_req)
    run_one("set", set_req)
    run_one("del", del_req)


def run_gbench(mode, out_path, gbench_format, gbench_args):
    gbench_bin = ROOT_DIR / "build/benchmarks" / f"tinycache_bench_{mode}"
    if not gbench_bin.exists():
        print(
            f"ERROR: Google Benchmark binary not found: {gbench_bin}", file=sys.stderr
        )
        print("Build with: ./build.sh bench", file=sys.stderr)
        sys.exit(1)

    cmd = [
        str(gbench_bin),
        f"--benchmark_out_format={gbench_format}",
        f"--benchmark_out={out_path}",
    ]
    if gbench_args:
        cmd.extend(gbench_args)
    run_command(cmd)


def normalize_number_text(value):
    return value.replace("\u202f", "").replace("\u00a0", "").replace(" ", "")


def parse_number(value, decimal_comma=None):
    if value is None:
        return None
    if isinstance(value, (int, float)):
        return float(value)
    value = value.strip()
    if value in ("<not supported>", "<not counted>", "not counted"):
        return None
    value = normalize_number_text(value)
    has_comma = "," in value
    has_dot = "." in value
    if decimal_comma is None:
        if has_comma and has_dot:
            decimal_comma = value.rfind(",") > value.rfind(".")
        elif has_comma:
            decimal_comma = True
        else:
            decimal_comma = False
    if decimal_comma:
        value = value.replace(".", "")
        value = value.replace(",", ".")
    else:
        value = value.replace(",", "")
    try:
        return float(value)
    except ValueError:
        return None


def detect_perf_delim(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if line.count(";") > line.count(","):
                return ";"
            return ","
    return ","


def parse_perf_csv(path):
    metrics = {}
    delim = detect_perf_delim(path)
    units = {"msec", "sec", "usec", "nsec"}
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = [p.strip() for p in line.split(delim)]
            if len(parts) < 3:
                continue
            value = parts[0]
            unit = parts[1]
            event = parts[2]

            if len(parts) > 3 and parts[1].isdigit() and parts[2] in units:
                value = f"{parts[0]}.{parts[1]}"
                unit = parts[2]
                event = parts[3]

            if not event:
                continue
            parsed = parse_number(value)
            if parsed is None:
                continue
            metrics[event] = {"value": parsed, "unit": unit}
    return metrics


def parse_perf_text(path):
    metrics = {}
    line_re = re.compile(
        r"^\s*(?P<value>[0-9][0-9\s\u202f\u00a0\.,]*|<not supported>|<not counted>)\s+(?P<rest>.+)$"
    )
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if "Performance counter stats" in line or "time elapsed" in line:
                continue
            match = line_re.match(line)
            if not match:
                continue
            value_token = match.group("value")
            rest = match.group("rest").strip()
            if rest.startswith("seconds"):
                continue
            if value_token.startswith("<"):
                continue
            value = parse_number(value_token, decimal_comma=None)
            if value is None:
                continue
            tokens = rest.split()
            if not tokens:
                continue
            if tokens[0] in PERF_TEXT_UNITS:
                if len(tokens) < 2:
                    continue
                unit = tokens[0]
                event = tokens[1]
            else:
                unit = ""
                event = tokens[0]
            if event:
                metrics[event] = {"value": value, "unit": unit}
    return metrics


def parse_perf(path):
    if path.suffix == ".txt":
        return parse_perf_text(path)
    return parse_perf_csv(path)


def parse_redis_csv(path):
    results = {}
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            test = row.get("test")
            if not test or test.lower() == "test":
                continue
            metrics = {}
            for key, value in row.items():
                if key == "test":
                    continue
                metrics[key] = parse_number(value)
            results[test] = metrics
    return results


def find_one_by_glob(dir_path, patterns, preferred=None):
    if preferred:
        p = dir_path / preferred
        if p.exists():
            return p
    matches = []
    for pat in patterns:
        matches.extend(list(dir_path.glob(pat)))
    matches = sorted({m for m in matches if m.is_file()})
    if len(matches) == 1:
        return matches[0]
    return None


def find_gbench_file(dir_path):
    return find_one_by_glob(
        dir_path,
        patterns=["gbench*.json", "gbench*.csv"],
        preferred="gbench.json",
    )


def gbench_time_to_ns(value, unit):
    unit = (unit or "ns").strip()
    scale = {
        "ns": 1.0,
        "us": 1_000.0,
        "usec": 1_000.0,
        "ms": 1_000_000.0,
        "sec": 1_000_000_000.0,
        "s": 1_000_000_000.0,
    }.get(unit)
    if scale is None:
        return None
    try:
        return float(value) * scale
    except (TypeError, ValueError):
        return None


def parse_gbench_json(path):
    payload = json.loads(Path(path).read_text(encoding="utf-8"))
    results = {}
    for entry in payload.get("benchmarks", []):
        name = entry.get("name")
        if not name:
            continue
        if entry.get("run_type") and entry.get("run_type") != "iteration":
            continue
        time_unit = entry.get("time_unit", "ns")
        metrics = {}
        if "real_time" in entry:
            ns = gbench_time_to_ns(entry.get("real_time"), time_unit)
            if ns is not None:
                metrics["real_time_ns"] = ns
        if "cpu_time" in entry:
            ns = gbench_time_to_ns(entry.get("cpu_time"), time_unit)
            if ns is not None:
                metrics["cpu_time_ns"] = ns
        if "items_per_second" in entry:
            metrics["items_per_second"] = parse_number(
                str(entry.get("items_per_second"))
            )
        counters = entry.get("counters")
        if isinstance(counters, dict):
            for k, v in counters.items():
                if isinstance(v, (int, float)):
                    metrics[k] = float(v)
        if metrics:
            results[name] = metrics
    return results


def parse_gbench_csv(path):
    results = {}
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            name = row.get("name")
            if not name or name.lower() == "name":
                continue
            unit = row.get("time_unit", "ns")
            metrics = {}
            if row.get("real_time"):
                ns = gbench_time_to_ns(parse_number(row.get("real_time")), unit)
                if ns is not None:
                    metrics["real_time_ns"] = ns
            if row.get("cpu_time"):
                ns = gbench_time_to_ns(parse_number(row.get("cpu_time")), unit)
                if ns is not None:
                    metrics["cpu_time_ns"] = ns
            if row.get("items_per_second"):
                metrics["items_per_second"] = parse_number(row.get("items_per_second"))
            for k, v in row.items():
                if k in (
                    "name",
                    "iterations",
                    "real_time",
                    "cpu_time",
                    "time_unit",
                    "threads",
                    "items_per_second",
                    "bytes_per_second",
                ):
                    continue
                parsed = parse_number(v)
                if parsed is not None:
                    metrics[k] = parsed
            if metrics:
                results[name] = metrics
    return results


def parse_gbench(path):
    if path.suffix == ".csv":
        return parse_gbench_csv(path)
    return parse_gbench_json(path)


def format_number(value):
    if value is None:
        return "-"
    if abs(value) >= 1000:
        return f"{value:,.2f}"
    return f"{value:.4f}".rstrip("0").rstrip(".")


def calc_delta_pct(left, right):
    if left is None or right is None or left == 0:
        return None
    return (right - left) / left * 100.0


def format_delta_pct(delta_pct):
    if delta_pct is None:
        return "-"
    return f"{delta_pct:+.2f}%"


def compare_metrics(left, right):
    rows = []
    keys = sorted(set(left.keys()) | set(right.keys()))
    for key in keys:
        left_val = left.get(key)
        right_val = right.get(key)
        unit = ""
        if isinstance(left_val, dict):
            unit = left_val.get("unit", "")
            left_val = left_val.get("value")
        if isinstance(right_val, dict):
            unit = unit or right_val.get("unit", "")
            right_val = right_val.get("value")
        delta = None
        if left_val is not None and right_val is not None:
            delta = right_val - left_val
        rows.append(
            {
                "metric": key,
                "unit": unit,
                "left": left_val,
                "right": right_val,
                "delta": delta,
                "delta_pct": calc_delta_pct(left_val, right_val),
            }
        )
    return rows


def table_from_rows(rows, headers):
    data = [headers]
    for row in rows:
        data.append([str(row.get(h, "")) for h in headers])
    widths = [max(len(str(item)) for item in col) for col in zip(*data)]
    lines = []
    for idx, row in enumerate(data):
        line = "  ".join(item.ljust(widths[i]) for i, item in enumerate(row))
        lines.append(line)
        if idx == 0:
            lines.append("  ".join("-" * w for w in widths))
    return "\n".join(lines)


def emit_output(text, out_path):
    if out_path:
        Path(out_path).write_text(text)
    else:
        print(text)


def compare_runs(left_dir, right_dir, fmt):
    left = Path(left_dir)
    right = Path(right_dir)

    redis_section = None
    redis_left_files = list_files_by_suffix(left, "redis_bench", [".csv"])
    redis_right_files = list_files_by_suffix(right, "redis_bench", [".csv"])
    redis_rows = []
    redis_missing = []
    for suffix in sorted(set(redis_left_files.keys()) | set(redis_right_files.keys())):
        lpath = redis_left_files.get(suffix)
        rpath = redis_right_files.get(suffix)
        mode = suffix_to_mode(suffix)
        if not lpath or not rpath:
            redis_missing.append(
                {
                    "mode": mode,
                    "left": str(lpath) if lpath else None,
                    "right": str(rpath) if rpath else None,
                }
            )
            continue
        left_data = parse_redis_csv(lpath)
        right_data = parse_redis_csv(rpath)
        tests = sorted(set(left_data.keys()) | set(right_data.keys()))
        for test in tests:
            left_metrics = left_data.get(test, {})
            right_metrics = right_data.get(test, {})
            for metric in sorted(set(left_metrics.keys()) | set(right_metrics.keys())):
                lval = left_metrics.get(metric)
                rval = right_metrics.get(metric)
                delta = None
                if lval is not None and rval is not None:
                    delta = rval - lval
                redis_rows.append(
                    {
                        "mode": mode,
                        "test": test,
                        "metric": metric,
                        "left": lval,
                        "right": rval,
                        "delta": delta,
                        "delta_pct": calc_delta_pct(lval, rval),
                    }
                )
    if redis_rows:
        redis_section = {"rows": redis_rows, "missing": redis_missing}
    else:
        redis_section = {
            "missing": redis_missing
            if redis_missing
            else [
                {
                    "mode": "*",
                    "left": f"{left}/redis_bench*.csv",
                    "right": f"{right}/redis_bench*.csv",
                }
            ]
        }

    perf_sections = {}
    perf_pairs = [
        ("perf_redis_bench", "Perf (redis)"),
        ("perf_gbench", "Perf (gbench)"),
    ]
    for stem, label in perf_pairs:
        left_perf_files = list_files_by_suffix(left, stem, [".csv", ".txt"])
        right_perf_files = list_files_by_suffix(right, stem, [".csv", ".txt"])
        rows = []
        missing = []
        for suffix in sorted(
            set(left_perf_files.keys()) | set(right_perf_files.keys())
        ):
            lpath = left_perf_files.get(suffix)
            rpath = right_perf_files.get(suffix)
            mode = suffix_to_mode(suffix)
            if not lpath or not rpath:
                missing.append(
                    {
                        "mode": mode,
                        "left": str(lpath) if lpath else None,
                        "right": str(rpath) if rpath else None,
                    }
                )
                continue
            left_metrics = parse_perf(lpath)
            right_metrics = parse_perf(rpath)
            for row in compare_metrics(left_metrics, right_metrics):
                row = dict(row)
                row["mode"] = mode
                rows.append(row)
        if rows:
            perf_sections[stem] = {"label": label, "rows": rows, "missing": missing}
        else:
            perf_sections[stem] = {
                "label": label,
                "missing": missing
                if missing
                else [
                    {
                        "mode": "*",
                        "left": f"{left}/{stem}*.[csv|txt]",
                        "right": f"{right}/{stem}*.[csv|txt]",
                    }
                ],
            }

    gbench_section = None
    left_gbench_files = list_files_by_suffix(left, "gbench", [".json", ".csv"])
    right_gbench_files = list_files_by_suffix(right, "gbench", [".json", ".csv"])
    gbench_rows = []
    gbench_missing = []
    skip_metrics = {"iterations", "repetitions", "threads"}
    for suffix in sorted(
        set(left_gbench_files.keys()) | set(right_gbench_files.keys())
    ):
        lpath = left_gbench_files.get(suffix)
        rpath = right_gbench_files.get(suffix)
        mode = suffix_to_mode(suffix)
        if not lpath or not rpath:
            gbench_missing.append(
                {
                    "mode": mode,
                    "left": str(lpath) if lpath else None,
                    "right": str(rpath) if rpath else None,
                }
            )
            continue
        left_data = parse_gbench(lpath)
        right_data = parse_gbench(rpath)
        bench_names = sorted(set(left_data.keys()) | set(right_data.keys()))
        for bname in bench_names:
            lmetrics = left_data.get(bname, {})
            rmetrics = right_data.get(bname, {})
            metric_names = sorted(set(lmetrics.keys()) | set(rmetrics.keys()))
            for metric in metric_names:
                if metric in skip_metrics:
                    continue
                lval = lmetrics.get(metric)
                rval = rmetrics.get(metric)
                unit = ""
                if metric.endswith("_ns"):
                    unit = "ns"
                elif metric == "items_per_second":
                    unit = "items/s"
                delta = None
                if lval is not None and rval is not None:
                    delta = rval - lval
                gbench_rows.append(
                    {
                        "mode": mode,
                        "benchmark": bname,
                        "metric": metric,
                        "unit": unit,
                        "left": lval,
                        "right": rval,
                        "delta": delta,
                        "delta_pct": calc_delta_pct(lval, rval),
                    }
                )
    if gbench_rows:
        gbench_section = {"rows": gbench_rows, "missing": gbench_missing}
    else:
        gbench_section = {
            "missing": gbench_missing
            if gbench_missing
            else [
                {
                    "mode": "*",
                    "left": f"{left}/gbench*.[json|csv]",
                    "right": f"{right}/gbench*.[json|csv]",
                }
            ]
        }

    if fmt == "json":
        payload = {
            "left": str(left),
            "right": str(right),
            "redis": redis_section,
            "perf": perf_sections,
            "gbench": gbench_section,
        }
        return json.dumps(payload, indent=2)

    output = []
    output.append("Redis Benchmark")
    if "rows" in redis_section:
        rows = []
        for row in redis_section["rows"]:
            rows.append(
                {
                    "mode": row.get("mode", ""),
                    "test": row["test"],
                    "metric": row["metric"],
                    "left": format_number(row["left"]),
                    "right": format_number(row["right"]),
                    "delta": format_number(row["delta"]),
                    "delta_pct": format_delta_pct(row["delta_pct"]),
                }
            )
        output.append(
            table_from_rows(
                rows,
                ["mode", "test", "metric", "left", "right", "delta", "delta_pct"],
            )
        )
    else:
        missing = redis_section.get("missing", [])
        output.append("Missing redis outputs: " + ", ".join(str(m) for m in missing))

    for stem, section in perf_sections.items():
        output.append(section["label"])
        if "rows" in section:
            rows = []
            for row in section["rows"]:
                rows.append(
                    {
                        "mode": row.get("mode", ""),
                        "metric": row["metric"],
                        "unit": row["unit"],
                        "left": format_number(row["left"]),
                        "right": format_number(row["right"]),
                        "delta": format_number(row["delta"]),
                        "delta_pct": format_delta_pct(row["delta_pct"]),
                    }
                )
            output.append(
                table_from_rows(
                    rows,
                    ["mode", "metric", "unit", "left", "right", "delta", "delta_pct"],
                )
            )
        else:
            output.append(
                "Missing perf outputs: "
                + ", ".join(str(m) for m in section.get("missing", []))
            )

    output.append("Google Benchmark")
    if "rows" in gbench_section:
        rows = []
        for row in gbench_section["rows"]:
            rows.append(
                {
                    "mode": row.get("mode", ""),
                    "benchmark": row["benchmark"],
                    "metric": row["metric"],
                    "unit": row["unit"],
                    "left": format_number(row["left"]),
                    "right": format_number(row["right"]),
                    "delta": format_number(row["delta"]),
                    "delta_pct": format_delta_pct(row["delta_pct"]),
                }
            )
        output.append(
            table_from_rows(
                rows,
                [
                    "mode",
                    "benchmark",
                    "metric",
                    "unit",
                    "left",
                    "right",
                    "delta",
                    "delta_pct",
                ],
            )
        )
    else:
        output.append(
            "Missing gbench outputs: "
            + ", ".join(str(m) for m in gbench_section.get("missing", []))
        )

    return "\n\n".join(output)


def find_perf_file(dir_path, stem):
    csv_path = dir_path / f"{stem}.csv"
    txt_path = dir_path / f"{stem}.txt"
    if csv_path.exists():
        return csv_path
    if txt_path.exists():
        return txt_path
    matches = list(dir_path.glob(f"{stem}*.csv")) + list(dir_path.glob(f"{stem}*.txt"))
    matches = sorted({m for m in matches if m.is_file()})
    if len(matches) == 1:
        return matches[0]
    return None


def list_files_by_suffix(dir_path, prefix, exts):
    files = {}
    for ext in exts:
        for p in sorted(dir_path.glob(f"{prefix}*{ext}")):
            if not p.is_file():
                continue
            stem = p.stem
            if not stem.startswith(prefix):
                continue
            suffix = stem[len(prefix) :]
            # Prefer earlier extensions in `exts` (callers should order by priority).
            if suffix not in files:
                files[suffix] = p
    return files


def suffix_to_mode(suffix):
    if not suffix:
        return "default"
    if suffix.startswith("_"):
        return suffix[1:]
    return suffix


def write_metadata(out_dir, args, redis_out, gbench_out, perf_outs, context):
    meta = {
        "timestamp": dt.datetime.now(dt.UTC).isoformat(),
        "mode": args.mode,
        "suite": args.suite,
        "perf": args.perf,
        "perf_format": args.perf_format,
        "perf_events": context["perf_events"],
        "perf_args": args.perf_args,
        "redis_csv": str(redis_out) if redis_out else None,
        "gbench_out": str(gbench_out) if gbench_out else None,
        "perf_outputs": perf_outs,
        "gbench_format": args.gbench_format,
        "gbench_args": args.gbench_args,
        "redis": {
            "host": context["host"],
            "port": context["port"],
            "clients": context["clients"],
            "requests": context["requests"],
            "keyspace": context["keyspace"],
            "value_size": context["value_size"],
            "warmup": context["warmup"],
        },
    }
    meta.update({"git": git_info()})
    meta_path = Path(out_dir) / f"bench_run_{args.mode}.json"
    meta_path.write_text(json.dumps(meta, indent=2))


def main():
    args = parse_args()

    if args.command == "compare":
        text = compare_runs(args.left, args.right, args.format)
        emit_output(text, args.out)
        return

    if args.mode in ("resp", "lru_mt") and args.suite in ("redis", "all"):
        print(
            f"NOTE: Mode '{args.mode}' does not support redis; forcing --suite gbench",
            file=sys.stderr,
        )
        args.suite = "gbench"

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    # Avoid overwriting when storing multiple modes in the same directory.
    name_suffix = f"_{args.mode}"

    host = args.host or resolve_env_str("HOST", "127.0.0.1")
    port = args.port or resolve_env_int("PORT", DEFAULT_PORT)
    clients = args.clients or resolve_env_int("CLIENTS", 32)
    keyspace = args.keyspace or resolve_env_int("KEYSPACE", 10000)
    value_size = args.value_size or resolve_env_int("VALUE_SIZE", 256)

    requests = args.requests
    if requests is None:
        requests = resolve_env_int("REQUESTS", None)
    warmup = args.warmup
    if warmup is None:
        warmup = resolve_env_int("WARMUP", None)

    perf_events = (
        args.perf_events or os.environ.get("PERF_EVENTS") or PERF_EVENTS_DEFAULT
    )
    perf_args = shlex.split(args.perf_args) if args.perf_args else []

    gbench_args = []
    if os.environ.get("GBENCH_ARGS"):
        gbench_args.extend(shlex.split(os.environ["GBENCH_ARGS"]))
    if args.gbench_args:
        gbench_args.extend(shlex.split(args.gbench_args))

    redis_csv = out_dir / f"redis_bench{name_suffix}.csv"
    gbench_out = out_dir / f"gbench{name_suffix}.{args.gbench_format}"
    perf_redis_out = (
        out_dir
        / f"perf_redis_bench{name_suffix}.{'txt' if args.perf_format == 'text' else 'csv'}"
    )
    perf_gbench_out = (
        out_dir
        / f"perf_gbench{name_suffix}.{'txt' if args.perf_format == 'text' else 'csv'}"
    )

    perf_outputs = {}

    if args.perf:
        which_or_exit("perf")
        script_path = str(Path(__file__).resolve())

        if args.suite in ("redis", "all"):
            cmd = [
                sys.executable,
                script_path,
                "run",
                "--mode",
                args.mode,
                "--out",
                str(out_dir),
                "--suite",
                "redis",
            ]
            cmd.extend(["--host", host])
            cmd.extend(["--port", str(port)])
            cmd.extend(["--clients", str(clients)])
            if requests is not None:
                cmd.extend(["--requests", str(requests)])
            cmd.extend(["--keyspace", str(keyspace)])
            cmd.extend(["--value-size", str(value_size)])
            if warmup is not None:
                cmd.extend(["--warmup", str(warmup)])
            if args.perf_format:
                cmd.extend(["--perf-format", args.perf_format])
            if args.perf_delim:
                cmd.extend(["--perf-delim", args.perf_delim])
            if args.perf_events:
                cmd.extend(["--perf-events", args.perf_events])
            if args.perf_args:
                cmd.extend(["--perf-args", args.perf_args])
            if args.gbench_format:
                cmd.extend(["--gbench-format", args.gbench_format])
            if args.gbench_args:
                cmd.extend(["--gbench-args", args.gbench_args])

            run_with_perf(
                command=cmd,
                perf_out=str(perf_redis_out),
                perf_format=args.perf_format,
                perf_events=perf_events,
                perf_args=perf_args,
                perf_delim=args.perf_delim,
            )
            perf_outputs["redis"] = str(perf_redis_out)

        if args.suite in ("gbench", "all"):
            gbench_bin = ROOT_DIR / "build/benchmarks" / f"tinycache_bench_{args.mode}"
            if not gbench_bin.exists():
                print(
                    f"ERROR: Google Benchmark binary not found: {gbench_bin}",
                    file=sys.stderr,
                )
                print("Build with: ./build.sh bench", file=sys.stderr)
                sys.exit(1)
            gbench_cmd = [
                str(gbench_bin),
                f"--benchmark_out_format={args.gbench_format}",
                f"--benchmark_out={gbench_out}",
            ]
            gbench_cmd.extend(gbench_args)
            run_with_perf(
                command=gbench_cmd,
                perf_out=str(perf_gbench_out),
                perf_format=args.perf_format,
                perf_events=perf_events,
                perf_args=perf_args,
                perf_delim=args.perf_delim,
            )
            perf_outputs["gbench"] = str(perf_gbench_out)

    else:
        if args.suite in ("redis", "all"):
            run_redis_benchmark(
                out_csv=redis_csv,
                mode=args.mode,
                host=host,
                port=port,
                clients=clients,
                requests=requests,
                keyspace=keyspace,
                value_size=value_size,
                warmup=warmup,
            )

        if args.suite in ("gbench", "all"):
            run_gbench(
                mode=args.mode,
                out_path=gbench_out,
                gbench_format=args.gbench_format,
                gbench_args=gbench_args,
            )

    context = {
        "host": host,
        "port": port,
        "clients": clients,
        "requests": requests,
        "keyspace": keyspace,
        "value_size": value_size,
        "warmup": warmup,
        "perf_events": perf_events,
    }
    write_metadata(out_dir, args, redis_csv, gbench_out, perf_outputs, context)


if __name__ == "__main__":
    main()
