#!/bin/bash
set -euo pipefail

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-6379}"
CLIENTS="${CLIENTS:-32}"
KEYSPACE="${KEYSPACE:-10000}"
VALUE_SIZE="${VALUE_SIZE:-256}"

MODE=""
CSV_MODE="false"
CSV_FILE=""
OUT_DIR=""
PERF_MODE="false"
PERF_FILE=""
PERF_FORMAT="csv"
PERF_DELIM=","
PERF_ARGS=()
PERF_EVENTS_OVERRIDE=""

PERF_EVENTS_DEFAULT="cycles,instructions,task-clock,context-switches,cpu-migrations,page-faults,branch-instructions,branch-misses,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,L1-dcache-store-misses,L1-icache-loads,L1-icache-load-misses,dTLB-loads,dTLB-load-misses,iTLB-loads,iTLB-load-misses,LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,bus-cycles,mem-loads,mem-stores"

usage() {
	cat <<'EOF'
Usage: redis_bench.sh --mode <read_heavy|balanced|write_heavy> [options]

Options:
  --mode <name>      Benchmark mode (read_heavy, balanced, write_heavy)
  --out <dir>        Write redis-benchmark CSV to <dir>/redis_bench.csv
  --csv <file>       Write redis-benchmark CSV output to file
  --perf             Run redis-benchmark under perf stat
  --perf-out <file>  Write perf stat CSV output to file
  --perf-format <f>  Perf output format: csv or text (default: csv)
  --perf-delim <c>   CSV delimiter for perf output (default: ,)
  --perf-events <list> Comma-separated perf event list
  --perf-args <args> Extra args passed to perf stat
  -h, --help         Show this help

Environment:
  HOST, PORT, CLIENTS, REQUESTS, KEYSPACE, VALUE_SIZE, WARMUP
  PERF_EVENTS         Override perf stat event list
  PERF_STAT_ARGS      Extra args passed to perf stat
EOF
}

ARGS_NO_PERF=()

while [[ $# -gt 0 ]]; do
	case "$1" in
	--mode)
		MODE="${2:-}"
		if [[ -z "$MODE" ]]; then
			echo "ERROR: --mode requires a value" >&2
			usage
			exit 1
		fi
		ARGS_NO_PERF+=(--mode "$MODE")
		shift 2
		;;
	--out)
		OUT_DIR="${2:-}"
		if [[ -z "$OUT_DIR" ]]; then
			echo "ERROR: --out requires a directory" >&2
			usage
			exit 1
		fi
		ARGS_NO_PERF+=(--out "$OUT_DIR")
		shift 2
		;;
	--csv)
		CSV_MODE="true"
		CSV_FILE="${2:-}"
		if [[ -z "$CSV_FILE" ]]; then
			echo "ERROR: --csv requires an output file path" >&2
			usage
			exit 1
		fi
		ARGS_NO_PERF+=(--csv "$CSV_FILE")
		shift 2
		;;
	--perf)
		PERF_MODE="true"
		shift
		;;
	--perf-out)
		PERF_FILE="${2:-}"
		if [[ -z "$PERF_FILE" ]]; then
			echo "ERROR: --perf-out requires an output file path" >&2
			usage
			exit 1
		fi
		shift 2
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

if [[ "$PERF_MODE" == "true" && -z "${REDIS_BENCH_PERF_WRAPPED:-}" ]]; then
	PERF_EVENTS="${PERF_EVENTS_OVERRIDE:-${PERF_EVENTS:-$PERF_EVENTS_DEFAULT}}"
	if [[ -z "$PERF_FILE" ]]; then
		if [[ -n "$OUT_DIR" ]]; then
			if [[ "$PERF_FORMAT" == "text" ]]; then
				PERF_FILE="${OUT_DIR}/perf_redis_bench.txt"
			else
				PERF_FILE="${OUT_DIR}/perf_redis_bench.csv"
			fi
		else
			if [[ "$PERF_FORMAT" == "text" ]]; then
				PERF_FILE="perf_redis_bench.txt"
			else
				PERF_FILE="perf_redis_bench.csv"
			fi
		fi
	fi
	mkdir -p "$(dirname "$PERF_FILE")"
	export REDIS_BENCH_PERF_WRAPPED=1
	if [[ "$PERF_FORMAT" == "text" ]]; then
		perf stat -o "$PERF_FILE" -e "$PERF_EVENTS" "${PERF_ARGS[@]}" -- "$0" "${ARGS_NO_PERF[@]}"
	else
		LC_ALL=C perf stat -x"$PERF_DELIM" -o "$PERF_FILE" -e "$PERF_EVENTS" "${PERF_ARGS[@]}" -- "$0" "${ARGS_NO_PERF[@]}"
	fi
	exit 0
fi

case "$MODE" in
read_heavy)
	GET_PCT=80
	SET_PCT=15
	DEL_PCT=5
	DEFAULT_REQUESTS=10000
	DEFAULT_WARMUP=5000
	MODE_LABEL="Read-heavy"
	;;
balanced)
	GET_PCT=40
	SET_PCT=40
	DEL_PCT=20
	DEFAULT_REQUESTS=100000
	DEFAULT_WARMUP=50000
	MODE_LABEL="Balanced"
	;;
write_heavy)
	SET_PCT=70
	GET_PCT=20
	DEL_PCT=10
	DEFAULT_REQUESTS=10000
	DEFAULT_WARMUP=0
	MODE_LABEL="Write-heavy"
	;;
*)
	echo "ERROR: Unknown mode: $MODE" >&2
	usage
	exit 1
	;;
esac

REQUESTS="${REQUESTS:-$DEFAULT_REQUESTS}"
WARMUP="${WARMUP:-$DEFAULT_WARMUP}"

if [[ -n "$OUT_DIR" && -z "$CSV_FILE" ]]; then
	CSV_FILE="${OUT_DIR}/redis_bench.csv"
	CSV_MODE="true"
fi

if [[ -n "$CSV_FILE" ]]; then
	CSV_MODE="true"
fi

GET_REQ=$((REQUESTS * GET_PCT / 100))
SET_REQ=$((REQUESTS * SET_PCT / 100))
DEL_REQ=$((REQUESTS * DEL_PCT / 100))

log() {
	if [[ "$CSV_MODE" == "true" ]]; then
		printf '%s\n' "$*" >&2
	else
		printf '%s\n' "$*"
	fi
}

run_bench() {
	if [[ "$CSV_MODE" == "true" ]]; then
		local tmp_file
		tmp_file="$(mktemp)"
		redis-benchmark "$@" --csv >"$tmp_file"
		if [[ -s "$CSV_FILE" ]]; then
			tail -n +2 "$tmp_file" >>"$CSV_FILE"
		else
			cat "$tmp_file" >>"$CSV_FILE"
		fi
		rm -f "$tmp_file"
	else
		redis-benchmark "$@"
	fi
}

if [[ "$CSV_MODE" == "true" ]]; then
	mkdir -p "$(dirname "$CSV_FILE")"
	: >"$CSV_FILE"
fi

if ((WARMUP > 0)); then
	log "Warmup: SET ${WARMUP}"
	run_bench -h "$HOST" -p "$PORT" -t set -n "$WARMUP" -c "$CLIENTS" -d "$VALUE_SIZE" -r "$KEYSPACE"
fi

log "${MODE_LABEL} mix: GET ${GET_REQ}, SET ${SET_REQ}, DEL ${DEL_REQ}"
run_bench -h "$HOST" -p "$PORT" -t get -n "$GET_REQ" -c "$CLIENTS" -d "$VALUE_SIZE" -r "$KEYSPACE"
run_bench -h "$HOST" -p "$PORT" -t set -n "$SET_REQ" -c "$CLIENTS" -d "$VALUE_SIZE" -r "$KEYSPACE"
run_bench -h "$HOST" -p "$PORT" -t del -n "$DEL_REQ" -c "$CLIENTS" -r "$KEYSPACE"
