# TinyCache Benchmarks

Two layers of benchmarking:
- Google Benchmark: microbenchmarks for in-memory LruShard
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
./build/benchmarks/tinycache_bench_resp
./build/benchmarks/tinycache_bench_lru_mt
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

./benchmarks/bench_cli.py run --mode resp --out bench/resp_01 --suite gbench --perf --perf-format text

./benchmarks/bench_cli.py run --mode lru_mt --out bench/lru_mt_01 --suite gbench
./benchmarks/bench_cli.py run --mode lru_mt --out bench/lru_mt_02 --suite gbench

./benchmarks/bench_cli.py compare --left bench/balanced_01 --right bench/balanced_02
./benchmarks/bench_cli.py compare --left bench/lru_mt_01 --right bench/lru_mt_02
```

Server-side perf (measure the server process, not the benchmark client):
```bash
# Start server in another shell and capture its PID
./build/tinycache &
SERVER_PID=$!

./benchmarks/bench_cli.py run \
  --mode read_heavy \
  --suite redis \
  --out bench/read_heavy_server_perf \
  --perf --perf-format text \
  --perf-target server \
  --perf-server-pid "$SERVER_PID"
```

Matrix runner (automates shard/item/workload combinations):
```bash
./build.sh core --release
./build.sh bench --release
python3 ./benchmarks/bench_matrix.py --out bench/matrix_default
```

Defaults used by matrix runner:
- shards: `1,2,4,8`
- max_memory_mb (total budget): `4,16,64,128`
- modes: `read_heavy,balanced,write_heavy`
- suite: `all` (redis + gbench)
- clients: `32`
- requests: `50000`

Custom matrix example:
```bash
python3 ./benchmarks/bench_matrix.py \
  --out bench/matrix_custom \
  --shards 1,2,4,8 \
  --max-memory-mb 4,16,64,128 \
  --modes read_heavy,balanced,write_heavy \
  --suite all \
  --perf --perf-format text --perf-target server \
  --clients 32 \
  --requests 50000
```

Notes:
- The runner updates `config.toml` for each config, starts `./build/tinycache`, runs benchmarks, then restores the original config at the end.
- Each config gets its own output folder: `shards_<N>__memmb_<M>/`.
- Run metadata is saved in `matrix_manifest.json`.
- If you run with `--perf`, every config/mode writes `perf_redis_bench_<mode>.*` and `perf_gbench_<mode>.*` (when `--suite` includes those suites).
- `--perf-target client` (default) measures benchmark client processes; `--perf-target server` measures the managed tinycache server PID.

Notes:
- `bench_cli.py run` writes mode-suffixed outputs so you can store multiple modes in the same `--out` directory (e.g. `gbench_read_heavy.json`, `gbench_lru_mt.json`, `perf_gbench_read_heavy.csv`, ...).

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

./benchmarks/bench_run.sh --mode lru_mt --out bench/lru_mt --suite gbench
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
