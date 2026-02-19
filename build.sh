#!/bin/bash
set -euo pipefail

usage() {
	cat <<'EOF'
Usage: ./build.sh [core|tests|bench|all] [--debug|--release]

Modes:
  core   Build server only (no tests, no benchmarks)
  tests  Build with tests enabled
  bench  Build with benchmarks enabled
  all    Build with tests and benchmarks enabled

Examples:
  ./build.sh
  ./build.sh bench --release
  ./build.sh tests
EOF
}

MODE="core"
BUILD_TYPE="Debug"

while [[ $# -gt 0 ]]; do
	case "$1" in
	core | tests | bench | all)
		MODE="$1"
		shift
		;;
	--release)
		BUILD_TYPE="Release"
		shift
		;;
	--debug)
		BUILD_TYPE="Debug"
		shift
		;;
	--help | -h)
		usage
		exit 0
		;;
	*)
		echo "Unknown option: $1"
		usage
		exit 1
		;;
	esac
done

case "$MODE" in
core)
	ENABLE_TESTS=OFF
	ENABLE_BENCHMARKS=OFF
	;;
tests)
	ENABLE_TESTS=ON
	ENABLE_BENCHMARKS=OFF
	;;
bench)
	ENABLE_TESTS=OFF
	ENABLE_BENCHMARKS=ON
	;;
all)
	ENABLE_TESTS=ON
	ENABLE_BENCHMARKS=ON
	;;
*)
	echo "Unknown mode: $MODE"
	usage
	exit 1
	;;
esac

if command -v nproc >/dev/null 2>&1; then
	JOBS="${JOBS:-$(nproc)}"
else
	JOBS="${JOBS:-8}"
fi

cmake -S . -B build \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	-DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
	-DENABLE_BENCHMARKS="$ENABLE_BENCHMARKS" \
	-DENABLE_TESTS="$ENABLE_TESTS"

cmake --build build -j"$JOBS"
