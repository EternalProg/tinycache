#!/bin/bash
set -euo pipefail

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-6379}"
CLIENTS="${CLIENTS:-32}"
REQUESTS="${REQUESTS:-100000}"
KEYSPACE="${KEYSPACE:-10000}"
VALUE_SIZE="${VALUE_SIZE:-256}"

SET_PCT=70
GET_PCT=20
DEL_PCT=10

SET_REQ=$((REQUESTS * SET_PCT / 100))
GET_REQ=$((REQUESTS * GET_PCT / 100))
DEL_REQ=$((REQUESTS * DEL_PCT / 100))

echo "Write-heavy mix: SET ${SET_REQ}, GET ${GET_REQ}, DEL ${DEL_REQ}"
redis-benchmark -h "$HOST" -p "$PORT" -t set -n "$SET_REQ" -c "$CLIENTS" -d "$VALUE_SIZE" -r "$KEYSPACE" -q
redis-benchmark -h "$HOST" -p "$PORT" -t get -n "$GET_REQ" -c "$CLIENTS" -d "$VALUE_SIZE" -r "$KEYSPACE" -q
redis-benchmark -h "$HOST" -p "$PORT" -t del -n "$DEL_REQ" -c "$CLIENTS" -r "$KEYSPACE" -q
