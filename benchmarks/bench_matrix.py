#!/usr/bin/env python3
import argparse
import json
import signal
import socket
import subprocess
import sys
import time
from datetime import UTC, datetime
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parents[1]
DEFAULT_MODES = ["read_heavy", "balanced", "write_heavy"]
ALLOWED_MODES = set(DEFAULT_MODES)


def parse_csv_ints(raw: str, label: str) -> list[int]:
    values: list[int] = []
    for token in raw.split(","):
        token = token.strip()
        if not token:
            continue
        try:
            number = int(token)
        except ValueError:
            raise argparse.ArgumentTypeError(f"Invalid {label} value: {token}")
        if number <= 0:
            raise argparse.ArgumentTypeError(
                f"{label} values must be > 0, got: {number}"
            )
        values.append(number)
    if not values:
        raise argparse.ArgumentTypeError(f"No {label} values provided")
    return values


def parse_csv_modes(raw: str) -> list[str]:
    modes = [token.strip() for token in raw.split(",") if token.strip()]
    if not modes:
        raise argparse.ArgumentTypeError("No modes provided")
    unknown = [mode for mode in modes if mode not in ALLOWED_MODES]
    if unknown:
        raise argparse.ArgumentTypeError(
            f"Unknown mode(s): {', '.join(unknown)}. Allowed: {', '.join(sorted(ALLOWED_MODES))}"
        )
    return modes


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run TinyCache benchmark matrix across config combinations"
    )
    parser.add_argument(
        "--out",
        required=True,
        help="Output directory for all matrix results",
    )
    parser.add_argument(
        "--shards",
        default="1,2,4,8",
        help="Comma-separated shard counts (default: 1,2,4,8)",
    )
    parser.add_argument(
        "--max-memory-mb",
        default="4,16,64,128",
        help="Comma-separated total memory budgets in MB (default: 4,16,64,128)",
    )
    parser.add_argument(
        "--modes",
        default=",".join(DEFAULT_MODES),
        help="Comma-separated workload modes (default: read_heavy,balanced,write_heavy)",
    )
    parser.add_argument("--host", default="127.0.0.1", help="Benchmark host")
    parser.add_argument("--port", type=int, default=8080, help="Benchmark port")
    parser.add_argument(
        "--clients", type=int, default=32, help="Redis benchmark clients"
    )
    parser.add_argument(
        "--requests",
        type=int,
        default=50000,
        help="Total requests per mode (default: 50000)",
    )
    parser.add_argument(
        "--keyspace",
        type=int,
        default=10000,
        help="redis-benchmark keyspace size",
    )
    parser.add_argument(
        "--value-size",
        type=int,
        default=256,
        help="redis-benchmark value payload size",
    )
    parser.add_argument(
        "--warmup",
        type=int,
        default=None,
        help="Warmup requests override for all modes",
    )
    parser.add_argument(
        "--server-bin",
        default=str(ROOT_DIR / "build" / "tinycache"),
        help="Path to tinycache server binary",
    )
    parser.add_argument(
        "--config",
        default=str(ROOT_DIR / "config.toml"),
        help="Path to config.toml",
    )
    parser.add_argument(
        "--startup-timeout",
        type=float,
        default=10.0,
        help="Seconds to wait for server startup",
    )
    parser.add_argument(
        "--startup-settle",
        type=float,
        default=0.5,
        help="Extra seconds to wait after server becomes reachable (default: 0.5)",
    )
    parser.add_argument(
        "--mode-retries",
        type=int,
        default=2,
        help="Extra retries per mode on transient benchmark failure (default: 2)",
    )
    parser.add_argument(
        "--retry-delay",
        type=float,
        default=1.0,
        help="Seconds to wait between retries (default: 1.0)",
    )
    parser.add_argument(
        "--suite",
        choices=["redis", "gbench", "all"],
        default="all",
        help="Benchmark suite to run per mode/config (default: all)",
    )
    parser.add_argument(
        "--perf",
        action="store_true",
        help="Run selected suite under perf stat",
    )
    parser.add_argument(
        "--perf-target",
        choices=["client", "server"],
        default="client",
        help="Perf target process (default: client)",
    )
    parser.add_argument(
        "--perf-format",
        choices=["csv", "text"],
        default="text",
        help="Perf output format (default: text)",
    )
    parser.add_argument(
        "--perf-delim",
        default=",",
        help="Delimiter for perf CSV output (default: ,)",
    )
    parser.add_argument(
        "--perf-events",
        default=None,
        help="Comma-separated perf events override",
    )
    parser.add_argument(
        "--perf-args",
        default=None,
        help="Extra perf stat args (quoted string)",
    )
    parser.add_argument(
        "--gbench-format",
        choices=["json", "csv"],
        default="json",
        help="Google Benchmark output format (default: json)",
    )
    parser.add_argument(
        "--gbench-args",
        default=None,
        help="Extra Google Benchmark args (quoted string)",
    )
    args = parser.parse_args()

    args.shards = parse_csv_ints(args.shards, "shards")
    args.max_memory_mb = parse_csv_ints(args.max_memory_mb, "max-memory-mb")
    args.modes = parse_csv_modes(args.modes)

    if args.port <= 0 or args.port > 65535:
        parser.error("--port must be in range 1..65535")
    if args.clients <= 0:
        parser.error("--clients must be > 0")
    if args.requests <= 0:
        parser.error("--requests must be > 0")
    if args.keyspace <= 0:
        parser.error("--keyspace must be > 0")
    if args.value_size <= 0:
        parser.error("--value-size must be > 0")
    if args.warmup is not None and args.warmup < 0:
        parser.error("--warmup must be >= 0")
    if args.startup_timeout <= 0:
        parser.error("--startup-timeout must be > 0")
    if args.startup_settle < 0:
        parser.error("--startup-settle must be >= 0")
    if args.mode_retries < 0:
        parser.error("--mode-retries must be >= 0")
    if args.retry_delay < 0:
        parser.error("--retry-delay must be >= 0")

    return args


def can_connect(host: str, port: int, timeout: float = 0.5) -> bool:
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def replace_toml_value(config_text: str, key: str, value: int) -> str:
    lines = config_text.splitlines()
    replaced = False
    for idx, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith(f"{key}") and "=" in stripped:
            prefix = line.split("=", 1)[0]
            lines[idx] = f"{prefix}= {value}"
            replaced = True
            break
    if not replaced:
        raise RuntimeError(f"Could not find key '{key}' in config.toml")
    return "\n".join(lines) + "\n"


def patch_config(
    config_path: Path,
    original_text: str,
    shard_count: int,
    max_memory_mb: int,
    port: int,
) -> None:
    bytes_total = max_memory_mb * 1024 * 1024
    max_memory_bytes_per_shard = max(1, bytes_total // shard_count)

    updated = replace_toml_value(original_text, "shard_count", shard_count)
    updated = replace_toml_value(
        updated, "max_memory_bytes_per_shard", max_memory_bytes_per_shard
    )
    updated = replace_toml_value(updated, "port", port)
    config_path.write_text(updated, encoding="utf-8")


def wait_for_server(host: str, port: int, timeout: float) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        if can_connect(host, port, timeout=0.2):
            return True
        time.sleep(0.1)
    return False


def stop_server(proc: subprocess.Popen) -> None:
    if proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)


def run_one_mode(
    args: argparse.Namespace, mode: str, out_dir: Path, server_pid: int | None = None
) -> None:
    cmd = [
        sys.executable,
        str(ROOT_DIR / "benchmarks" / "bench_cli.py"),
        "run",
        "--mode",
        mode,
        "--suite",
        args.suite,
        "--out",
        str(out_dir),
        "--host",
        args.host,
        "--port",
        str(args.port),
        "--clients",
        str(args.clients),
        "--requests",
        str(args.requests),
        "--keyspace",
        str(args.keyspace),
        "--value-size",
        str(args.value_size),
        "--gbench-format",
        args.gbench_format,
    ]

    if args.warmup is not None:
        cmd.extend(["--warmup", str(args.warmup)])
    if args.perf:
        cmd.extend(["--perf", "--perf-format", args.perf_format])
        cmd.extend(["--perf-target", args.perf_target])
        if args.perf_target == "server":
            if server_pid is None:
                raise RuntimeError("server pid is required for --perf-target=server")
            cmd.extend(["--perf-server-pid", str(server_pid)])
        if args.perf_delim:
            cmd.extend(["--perf-delim", args.perf_delim])
        if args.perf_events:
            cmd.extend(["--perf-events", args.perf_events])
        if args.perf_args:
            cmd.extend(["--perf-args", args.perf_args])
    if args.gbench_args:
        cmd.extend(["--gbench-args", args.gbench_args])

    subprocess.run(cmd, check=True, cwd=ROOT_DIR)


def main() -> None:
    args = parse_args()
    server_bin = Path(args.server_bin)
    config_path = Path(args.config)
    out_root = Path(args.out)
    out_root.mkdir(parents=True, exist_ok=True)

    if not server_bin.exists():
        print(
            f"ERROR: Server binary not found: {server_bin}. Build it with './build.sh core --release'.",
            file=sys.stderr,
        )
        sys.exit(1)
    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}", file=sys.stderr)
        sys.exit(1)

    if can_connect(args.host, args.port):
        print(
            f"ERROR: {args.host}:{args.port} is already in use. Stop the existing server or use --port.",
            file=sys.stderr,
        )
        sys.exit(1)

    original_config = config_path.read_text(encoding="utf-8")
    manifest = {
        "timestamp": datetime.now(UTC).isoformat(),
        "host": args.host,
        "port": args.port,
        "clients": args.clients,
        "requests": args.requests,
        "keyspace": args.keyspace,
        "value_size": args.value_size,
        "warmup": args.warmup,
        "suite": args.suite,
        "perf": args.perf,
        "perf_target": args.perf_target,
        "perf_format": args.perf_format,
        "perf_delim": args.perf_delim,
        "perf_events": args.perf_events,
        "perf_args": args.perf_args,
        "gbench_format": args.gbench_format,
        "gbench_args": args.gbench_args,
        "mode_retries": args.mode_retries,
        "retry_delay": args.retry_delay,
        "startup_settle": args.startup_settle,
        "shards": args.shards,
        "max_memory_mb_total": args.max_memory_mb,
        "modes": args.modes,
        "runs": [],
    }

    interrupted = {"value": False}

    def _handle_signal(signum, _frame):
        interrupted["value"] = True
        print(f"\nReceived signal {signum}, stopping after current step...", flush=True)

    signal.signal(signal.SIGINT, _handle_signal)
    signal.signal(signal.SIGTERM, _handle_signal)

    try:
        for shard_count in args.shards:
            for max_memory_mb in args.max_memory_mb:
                if interrupted["value"]:
                    raise KeyboardInterrupt

                max_memory_bytes_per_shard = max(
                    1, (max_memory_mb * 1024 * 1024) // shard_count
                )

                run_id = f"shards_{shard_count}__memmb_{max_memory_mb}"
                run_out = out_root / run_id
                run_out.mkdir(parents=True, exist_ok=True)
                server_log = run_out / "server.log"

                print(
                    f"\n=== Config: shard_count={shard_count}, max_memory_mb_total={max_memory_mb}, max_memory_bytes_per_shard={max_memory_bytes_per_shard} ===",
                    flush=True,
                )

                patch_config(
                    config_path,
                    original_config,
                    shard_count,
                    max_memory_mb,
                    args.port,
                )

                with server_log.open("w", encoding="utf-8") as log_handle:
                    server_proc = subprocess.Popen(
                        [str(server_bin)],
                        cwd=ROOT_DIR,
                        stdout=log_handle,
                        stderr=subprocess.STDOUT,
                    )
                    try:
                        if not wait_for_server(
                            args.host, args.port, args.startup_timeout
                        ):
                            raise RuntimeError(
                                f"Server did not become ready on {args.host}:{args.port} within {args.startup_timeout}s"
                            )
                        if args.startup_settle > 0:
                            time.sleep(args.startup_settle)

                        for mode in args.modes:
                            if interrupted["value"]:
                                raise KeyboardInterrupt
                            print(f"Running mode={mode} -> {run_out}", flush=True)
                            attempt = 0
                            while True:
                                try:
                                    run_one_mode(
                                        args,
                                        mode,
                                        run_out,
                                        server_proc.pid
                                        if args.perf and args.perf_target == "server"
                                        else None,
                                    )
                                    break
                                except subprocess.CalledProcessError:
                                    if attempt >= args.mode_retries:
                                        raise
                                    attempt += 1
                                    print(
                                        f"Mode {mode} failed (attempt {attempt}/{args.mode_retries + 1}). Retrying in {args.retry_delay:.1f}s...",
                                        flush=True,
                                    )
                                    if args.retry_delay > 0:
                                        time.sleep(args.retry_delay)
                            manifest["runs"].append(
                                {
                                    "run_id": run_id,
                                    "mode": mode,
                                    "shard_count": shard_count,
                                    "max_memory_mb_total": max_memory_mb,
                                    "max_memory_bytes_per_shard": max_memory_bytes_per_shard,
                                    "out_dir": str(run_out),
                                }
                            )
                    finally:
                        stop_server(server_proc)

        (out_root / "matrix_manifest.json").write_text(
            json.dumps(manifest, indent=2), encoding="utf-8"
        )
        print(f"\nMatrix run complete. Results saved to: {out_root}", flush=True)
    except KeyboardInterrupt:
        (out_root / "matrix_manifest.partial.json").write_text(
            json.dumps(manifest, indent=2), encoding="utf-8"
        )
        print(
            "\nBenchmark matrix interrupted. Partial manifest saved.", file=sys.stderr
        )
        sys.exit(130)
    except Exception as exc:
        (out_root / "matrix_manifest.partial.json").write_text(
            json.dumps(manifest, indent=2), encoding="utf-8"
        )
        print(
            f"\nBenchmark matrix failed: {exc}. Partial manifest saved.",
            file=sys.stderr,
        )
        raise
    finally:
        config_path.write_text(original_config, encoding="utf-8")


if __name__ == "__main__":
    main()
