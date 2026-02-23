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

usage() {
  cat <<'EOF'
Usage: redis_bench.sh --mode <read_heavy|balanced|write_heavy> [--csv <file>]

Options:
  --mode <name>  Benchmark mode (read_heavy, balanced, write_heavy)
  --csv <file>   Write redis-benchmark CSV output to file
  -h, --help     Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
  --mode)
    MODE="${2:-}"
    if [[ -z "$MODE" ]]; then
      echo "ERROR: --mode requires a value" >&2
      usage
      exit 1
    fi
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
    redis-benchmark "$@" --csv >>"$CSV_FILE"
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
