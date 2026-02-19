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
PORT=8080 CLIENTS=32 REQUESTS=20000 ./benchmarks/redis_bench_read_heavy.sh
PORT=8080 CLIENTS=32 REQUESTS=20000 ./benchmarks/redis_bench_balanced.sh
PORT=8080 CLIENTS=32 REQUESTS=20000 ./benchmarks/redis_bench_write_heavy.sh
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
