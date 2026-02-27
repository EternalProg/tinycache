# TinyCache Benchmarks

Two layers of benchmarking:
- Google Benchmark: microbenchmarks for in-memory LruCache
- redis-benchmark: end-to-end TCP/RESP performance

## Build

Recommended (via helper script):
```bash
./build.sh bench
```

Other options:
```bash
./build.sh core
./build.sh tests
./build.sh all
./build.sh bench --release
```

## Run microbenchmarks (in-memory)

```bash
./build/benchmarks/tinycache_bench_read_heavy
./build/benchmarks/tinycache_bench_balanced
./build/benchmarks/tinycache_bench_write_heavy
```

Useful flags:
```bash
./build/benchmarks/tinycache_bench_read_heavy --benchmark_min_time=0.1
./build/benchmarks/tinycache_bench_read_heavy --benchmark_filter=Zipf
```

## Run end-to-end benchmarks (network)

Start server:
```bash
./build/tinycache
```

Run workload scripts (bash):
```bash
chmod +x benchmarks/redis_bench_*.sh
PORT=8080 CLIENTS=32 REQUESTS=20000 ./benchmarks/redis_bench.sh --mode read_heavy --out bench/read_heavy

PORT=8080 CLIENTS=32 REQUESTS=20000 ./benchmarks/redis_bench.sh --mode write_heavy --out bench/write_heavy

PORT=8080 CLIENTS=32 REQUESTS=20000 ./benchmarks/redis_bench.sh --mode balanced --out bench/balanced

PORT=8080 CLIENTS=32 REQUESTS=20000 ./benchmarks/redis_bench.sh --mode balanced --out bench/balanced --perf

PORT=8080 CLIENTS=32 REQUESTS=20000 ./benchmarks/redis_bench.sh --mode balanced --out bench/balanced --perf --perf-format text

PORT=8080 CLIENTS=32 REQUESTS=20000 ./benchmarks/redis_bench.sh --mode balanced --out bench/balanced --perf --perf-format csv --perf-delim ';'

PORT=8080 CLIENTS=32 REQUESTS=20000 ./benchmarks/redis_bench.sh --mode balanced --out bench/balanced --perf \
  --perf-events "cycles,instructions,task-clock,cache-misses" --perf-args "-r 5"
```

Unified Python CLI (run + compare):
```bash
./benchmarks/bench_cli.py run --mode balanced --out bench/balanced_01 --suite all --perf --perf-format text
./benchmarks/bench_cli.py run --mode balanced --out bench/balanced_02 --suite all --perf --perf-format text

./benchmarks/bench_cli.py compare --left bench/balanced_01 --right bench/balanced_02
```

Notes:
- CLI defaults to port 8080 (TinyCache default). Override with `--port` or `PORT=...` when needed.
- It's better to compare runs with the same mode and similar parameters (clients, requests) to isolate performance changes.

Unified runner (saves redis-benchmark CSV and Google Benchmark JSON):
```bash
./benchmarks/bench_run.sh --mode read_heavy --out bench/read_heavy --suite all
./benchmarks/bench_run.sh --mode read_heavy --out bench/read_heavy --suite all --perf
./benchmarks/bench_run.sh --mode read_heavy --out bench/read_heavy --suite all --perf --perf-format text
./benchmarks/bench_run.sh --mode read_heavy --out bench/read_heavy --suite all --perf --perf-format csv --perf-delim ';'
./benchmarks/bench_run.sh --mode read_heavy --out bench/read_heavy --suite all --perf \
  --perf-events "cycles,instructions,task-clock,cache-misses" --perf-args "-r 5"
```

Run only one suite:
```bash
./benchmarks/bench_run.sh --mode read_heavy --out bench/read_heavy --suite gbench
./benchmarks/bench_run.sh --mode read_heavy --out bench/read_heavy --suite redis
```

Notes:
- `redis-benchmark -t get,set,del` runs separate tests, not a mixed workload.
- Scripts run separate GET/SET/DEL passes to match ratios.
- Tune: `HOST`, `PORT`, `CLIENTS`, `REQUESTS`, `KEYSPACE`, `VALUE_SIZE`, `WARMUP`.

## Interpreting output (redis-benchmark)

Key fields:
- `throughput summary`: requests/sec
- `p50/p95/p99`: median and tail latency
- `avg`: average latency (less important than p95/p99)

If you see `WARNING: Could not fetch server CONFIG`, TinyCache just does not
implement CONFIG; the benchmark still runs.
