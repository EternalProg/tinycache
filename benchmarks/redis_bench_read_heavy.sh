#!/bin/bash
set -euo pipefail

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-6379}"
CLIENTS="${CLIENTS:-32}"
REQUESTS="${REQUESTS:-10000}"
KEYSPACE="${KEYSPACE:-10000}"
VALUE_SIZE="${VALUE_SIZE:-256}"
WARMUP="${WARMUP:-5000}"

GET_PCT=80
SET_PCT=15
DEL_PCT=5

GET_REQ=$((REQUESTS * GET_PCT / 100))
SET_REQ=$((REQUESTS * SET_PCT / 100))
DEL_REQ=$((REQUESTS * DEL_PCT / 100))

echo "Warmup: SET ${WARMUP}"
redis-benchmark -h "$HOST" -p "$PORT" -t set -n "$WARMUP" -c "$CLIENTS" -d "$VALUE_SIZE" -r "$KEYSPACE"

echo "Read-heavy mix: GET ${GET_REQ}, SET ${SET_REQ}, DEL ${DEL_REQ}"
redis-benchmark -h "$HOST" -p "$PORT" -t get -n "$GET_REQ" -c "$CLIENTS" -d "$VALUE_SIZE" -r "$KEYSPACE"
redis-benchmark -h "$HOST" -p "$PORT" -t set -n "$SET_REQ" -c "$CLIENTS" -d "$VALUE_SIZE" -r "$KEYSPACE"
redis-benchmark -h "$HOST" -p "$PORT" -t del -n "$DEL_REQ" -c "$CLIENTS" -r "$KEYSPACE"
