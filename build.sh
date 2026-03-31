#!/bin/bash
set -euo pipefail

usage() {
	cat <<'EOF'
Usage: ./build.sh [core|tests|bench|all] [--debug|--release] [options]

Modes:
  core   Build server only (no tests, no benchmarks)
  tests  Build with tests enabled
  bench  Build with benchmarks enabled
  all    Build with tests and benchmarks enabled

Options:
  --lto                 Enable LTO/IPO (ENABLE_LTO=ON)
  --pgo-generate        Build with PGO instrumentation (PGO_MODE=GENERATE)
  --pgo-use             Build using collected PGO profiles (PGO_MODE=USE)
  --pgo-data-dir <dir>  Profile directory (PGO_DATA_DIR)

Examples:
  ./build.sh
  ./build.sh bench --release
  ./build.sh tests
  ./build.sh core --release --pgo-generate --pgo-data-dir build/pgo-data
  ./build.sh core --release --pgo-use --lto --pgo-data-dir build/pgo-data
EOF
}

MODE="core"
BUILD_TYPE="Debug"
ENABLE_LTO="OFF"
PGO_MODE="OFF"
PGO_DATA_DIR=""

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
	--lto)
		ENABLE_LTO="ON"
		shift
		;;
	--pgo-generate)
		PGO_MODE="GENERATE"
		shift
		;;
	--pgo-use)
		PGO_MODE="USE"
		shift
		;;
	--pgo-data-dir)
		PGO_DATA_DIR="${2:-}"
		if [[ -z "$PGO_DATA_DIR" ]]; then
			echo "ERROR: --pgo-data-dir requires a value"
			usage
			exit 1
		fi
		shift 2
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

cmake_args=(
	-S .
	-B build
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	-DCMAKE_BUILD_TYPE="$BUILD_TYPE"
	-DENABLE_BENCHMARKS="$ENABLE_BENCHMARKS"
	-DENABLE_TESTS="$ENABLE_TESTS"
	-DENABLE_LTO="$ENABLE_LTO"
	-DPGO_MODE="$PGO_MODE"
)

if [[ -n "$PGO_DATA_DIR" ]]; then
	cmake_args+=(-DPGO_DATA_DIR="$PGO_DATA_DIR")
fi

cmake "${cmake_args[@]}"

cmake --build build -j"$JOBS"
