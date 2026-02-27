#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR_DEFAULT="${ROOT_DIR}/benchmarks/out"

LABEL=""
MODE=""
OUT_DIR="$OUT_DIR_DEFAULT"
OUT_DIR_SET="false"
PERF_MODE="false"
SUITE="all"
PERF_FORMAT="csv"
PERF_DELIM=","
PERF_ARGS=()
PERF_EVENTS_OVERRIDE=""

PERF_EVENTS_DEFAULT="cycles,instructions,task-clock,context-switches,cpu-migrations,page-faults,branch-instructions,branch-misses,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,L1-dcache-store-misses,L1-icache-loads,L1-icache-load-misses,dTLB-loads,dTLB-load-misses,iTLB-loads,iTLB-load-misses,LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,bus-cycles,mem-loads,mem-stores"

usage() {
	cat <<'EOF'
Usage: bench_run.sh --mode <read_heavy|balanced|write_heavy|resp> [options]

Options:
  --mode <name>    Benchmark mode (read_heavy, balanced, write_heavy)
  --suite <name>   Benchmark suite (redis, gbench, all)
  --out <dir>      Output directory (if --label set, used as base)
  --label <name>   Run label (appended to --out base)
  --perf           Run selected benchmarks under perf stat
  --perf-format <f> Perf output format: csv or text (default: csv)
  --perf-delim <c> CSV delimiter for perf output (default: ,)
  --perf-events <list> Comma-separated perf event list
  --perf-args <args> Extra args passed to perf stat
  -h, --help       Show this help

Environment:
  HOST, PORT, CLIENTS, REQUESTS, KEYSPACE, VALUE_SIZE, WARMUP
  PERF_EVENTS      Override perf stat event list
  PERF_STAT_ARGS   Extra args passed to perf stat
  GBENCH_ARGS      Extra args passed to Google Benchmark binaries
EOF
}

while [[ $# -gt 0 ]]; do
	case "$1" in
	--label)
		LABEL="${2:-}"
		if [[ -z "$LABEL" ]]; then
			echo "ERROR: --label requires a value" >&2
			usage
			exit 1
		fi
		shift 2
		;;
	--mode)
		MODE="${2:-}"
		if [[ -z "$MODE" ]]; then
			echo "ERROR: --mode requires a value" >&2
			usage
			exit 1
		fi
		shift 2
		;;
	--suite)
		SUITE="${2:-}"
		if [[ -z "$SUITE" ]]; then
			echo "ERROR: --suite requires a value" >&2
			usage
			exit 1
		fi
		shift 2
		;;
	--out)
		OUT_DIR="${2:-}"
		if [[ -z "$OUT_DIR" ]]; then
			echo "ERROR: --out requires a value" >&2
			usage
			exit 1
		fi
		OUT_DIR_SET="true"
		shift 2
		;;
	--perf)
		PERF_MODE="true"
		shift
		;;
	--perf-format)
		PERF_FORMAT="${2:-}"
		if [[ -z "$PERF_FORMAT" ]]; then
			echo "ERROR: --perf-format requires a value" >&2
			usage
			exit 1
		fi
		shift 2
		;;
	--perf-delim)
		PERF_DELIM="${2:-}"
		if [[ -z "$PERF_DELIM" ]]; then
			echo "ERROR: --perf-delim requires a value" >&2
			usage
			exit 1
		fi
		shift 2
		;;
	--perf-events)
		PERF_EVENTS_OVERRIDE="${2:-}"
		if [[ -z "$PERF_EVENTS_OVERRIDE" ]]; then
			echo "ERROR: --perf-events requires a value" >&2
			usage
			exit 1
		fi
		shift 2
		;;
	--perf-args)
		if [[ -z "${2:-}" ]]; then
			echo "ERROR: --perf-args requires a value" >&2
			usage
			exit 1
		fi
		# shellcheck disable=SC2206
		PERF_ARGS+=($2)
		shift 2
		;;
	-h | --help)
		usage
		exit 0
		;;
	*)
		echo "ERROR: Unknown argument: $1" >&2
		usage
		exit 1
		;;
	esac
done

if [[ -z "$MODE" ]]; then
	echo "ERROR: --mode is required" >&2
	usage
	exit 1
fi

case "$MODE" in
read_heavy | balanced | write_heavy | resp) ;;
*)
	echo "ERROR: Unknown mode: $MODE" >&2
	usage
	exit 1
	;;
esac

if [[ "$MODE" == "resp" && ("$SUITE" == "redis" || "$SUITE" == "all") ]]; then
	echo "ERROR: RESP mode only supports gbench suite" >&2
	usage
	exit 1
fi

case "$SUITE" in
redis | gbench | all) ;;
*)
	echo "ERROR: Unknown suite: $SUITE" >&2
	usage
	exit 1
	;;
esac

case "$PERF_FORMAT" in
csv | text) ;;
*)
	echo "ERROR: Unknown perf format: $PERF_FORMAT" >&2
	usage
	exit 1
	;;
esac

if [[ -n "${PERF_STAT_ARGS:-}" ]]; then
	# shellcheck disable=SC2206
	PERF_ARGS+=($PERF_STAT_ARGS)
fi

if [[ -n "$LABEL" ]]; then
	RUN_DIR="${OUT_DIR}/${LABEL}"
	NAME_SUFFIX="_${MODE}"
elif [[ "$OUT_DIR_SET" == "true" ]]; then
	RUN_DIR="$OUT_DIR"
	NAME_SUFFIX=""
else
	echo "ERROR: --out or --label is required" >&2
	usage
	exit 1
fi
mkdir -p "$RUN_DIR"

REDIS_CSV="${RUN_DIR}/redis_bench${NAME_SUFFIX}.csv"
GBENCH_JSON="${RUN_DIR}/gbench${NAME_SUFFIX}.json"
if [[ "$PERF_FORMAT" == "text" ]]; then
	PERF_REDIS_OUT="${RUN_DIR}/perf_redis_bench${NAME_SUFFIX}.txt"
	PERF_GBENCH_OUT="${RUN_DIR}/perf_gbench${NAME_SUFFIX}.txt"
else
	PERF_REDIS_OUT="${RUN_DIR}/perf_redis_bench${NAME_SUFFIX}.csv"
	PERF_GBENCH_OUT="${RUN_DIR}/perf_gbench${NAME_SUFFIX}.csv"
fi

run_redis() {
	local args=(--mode "$MODE" --csv "$REDIS_CSV")
	if [[ "$PERF_MODE" == "true" ]]; then
		args+=(--perf --perf-out "$PERF_REDIS_OUT" --perf-format "$PERF_FORMAT")
		args+=(--perf-delim "$PERF_DELIM")
		if [[ -n "$PERF_EVENTS_OVERRIDE" ]]; then
			args+=(--perf-events "$PERF_EVENTS_OVERRIDE")
		fi
		if [[ ${#PERF_ARGS[@]} -gt 0 ]]; then
			args+=(--perf-args "${PERF_ARGS[*]}")
		fi
	fi
	"${ROOT_DIR}/benchmarks/redis_bench.sh" "${args[@]}"
}

run_gbench() {
	local gbench_bin="${ROOT_DIR}/build/benchmarks/tinycache_bench_${MODE}"
	if [[ ! -x "$gbench_bin" ]]; then
		echo "ERROR: Google Benchmark binary not found: $gbench_bin" >&2
		echo "Build with: ./build.sh bench" >&2
		exit 1
	fi

	local gbench_args=(--benchmark_out_format=json "--benchmark_out=${GBENCH_JSON}")
	if [[ -n "${GBENCH_ARGS:-}" ]]; then
		# shellcheck disable=SC2206
		gbench_args+=($GBENCH_ARGS)
	fi

	if [[ "$PERF_MODE" == "true" ]]; then
		PERF_EVENTS="${PERF_EVENTS_OVERRIDE:-${PERF_EVENTS:-$PERF_EVENTS_DEFAULT}}"
		if [[ "$PERF_FORMAT" == "text" ]]; then
			perf stat -o "$PERF_GBENCH_OUT" -e "$PERF_EVENTS" "${PERF_ARGS[@]}" -- "$gbench_bin" "${gbench_args[@]}"
		else
			LC_ALL=C perf stat -x"$PERF_DELIM" -o "$PERF_GBENCH_OUT" -e "$PERF_EVENTS" "${PERF_ARGS[@]}" -- "$gbench_bin" "${gbench_args[@]}"
		fi
	else
		"$gbench_bin" "${gbench_args[@]}"
	fi
}

if [[ "$SUITE" == "redis" || "$SUITE" == "all" ]]; then
	run_redis
fi

if [[ "$SUITE" == "gbench" || "$SUITE" == "all" ]]; then
	run_gbench
fi

if [[ "$SUITE" == "redis" || "$SUITE" == "all" ]]; then
	echo "Saved redis-benchmark CSV: $REDIS_CSV"
	if [[ "$PERF_MODE" == "true" ]]; then
		echo "Saved perf stat output (redis-benchmark): $PERF_REDIS_OUT"
	fi
fi

if [[ "$SUITE" == "gbench" || "$SUITE" == "all" ]]; then
	echo "Saved Google Benchmark JSON: $GBENCH_JSON"
	if [[ "$PERF_MODE" == "true" ]]; then
		echo "Saved perf stat output (gbench): $PERF_GBENCH_OUT"
	fi
fi
