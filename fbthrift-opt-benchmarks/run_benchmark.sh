#!/bin/bash
# =============================================================================
# fbthrift benchmark perf + kperf_arm topdown collection script
#
# Usage:
#   sudo ./run_benchmark.sh [compact|binary] [all|perf|topdown]
#
#   Protocol (default: compact):
#     compact   – use Compact protocol
#     binary    – use Binary protocol
#
#   Mode (default: all):
#     all       – run all three steps: benchmark baseline + perf record + topdown
#     perf      – run benchmark baseline + perf record only
#     topdown   – run benchmark baseline + kperf_arm topdown only
#
# Per group, the following steps are executed according to mode:
#   Step 1 (always) – Standard Google Benchmark run  -> JSON result
#   Step 2 (all|perf)    – perf record               -> perf.data
#   Step 3 (all|topdown) – kperf_arm topdown/cache/tlb/imix
#
# Benchmarks are grouped by data type + operation (8 groups total).
# All sizes of the same type are merged into a single --benchmark_filter.
# =============================================================================

# NOTE: intentionally NO "set -e" – each step captures its own exit code so
# a single failure does not abort the entire collection run.

# ---------------------------------------------------------------------------
# Configuration – adjust these paths before running
# ---------------------------------------------------------------------------
RUN_SCRIPT="./run.sh"                          # existing run script
KPERF_BIN="/home/Acc_util/kperf_arm"           # kperf_arm tool path
CORE=105                                        # must match the core in run.sh
RESULTS_DIR="benchmark_results"                 # output directory

# kperf_arm parameters
KPERF_DURATION=3
KPERF_INTERVAL=1
KPERF_ITER=3

# Minimum run time for the benchmark baseline pass
BM_MIN_TIME="3s"

# Number of repetitions for the perf record pass and kperf_arm background loop
# The outer while-true loop restarts automatically, so kperf_arm always sees
# a live workload regardless of how fast each case finishes.
BM_REPETITIONS=100
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Parse arguments
# ---------------------------------------------------------------------------
PROTOCOL="compact"
MODE="all"

for arg in "$@"; do
    case "$arg" in
        compact|binary)
            PROTOCOL="$arg"
            ;;
        all|perf|topdown)
            MODE="$arg"
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: $0 [compact|binary] [all|perf|topdown]"
            exit 1
            ;;
    esac
done

mkdir -p "${RESULTS_DIR}"

echo "Protocol : ${PROTOCOL}"
echo "Mode     : ${MODE}"
echo "Core     : ${CORE}"
echo "Results  : ${RESULTS_DIR}"
echo ""

# =============================================================================
# Two parallel arrays: LABELS[i] <-> FILTERS[i]
# =============================================================================
LABELS=(
    "serialize_varint32"
    "deserialize_varint32"
    "serialize_varint64"
    "deserialize_varint64"
    "serialize_double"
    "deserialize_double"
    "serialize_string"
    "deserialize_string"
)

FILTERS=(
    "BM_varint32_"
    "BM_Deserialize_varint32_"
    "BM_varint64_"
    "BM_Deserialize_varint64_"
    "BM_double_"
    "BM_Deserialize_double_"
    "BM_string_"
    "BM_Deserialize_string_"
)

# =============================================================================
# Helper: run one group and collect metrics according to MODE
# Arguments: $1 = label, $2 = benchmark filter regex
# =============================================================================
run_and_collect() {
    local label="$1"
    local filter="$2"
    local prefix="${RESULTS_DIR}/${label}"

    echo "====================================================="
    echo "Group  : ${label}"
    echo "Filter : ${filter}"

    # ------------------------------------------------------------------
    # Step 1 (always): Standard Google Benchmark run -> JSON result
    # ------------------------------------------------------------------
    echo "  [1] Standard benchmark run..."
    taskset -c ${CORE} \
    bash ${RUN_SCRIPT} ${PROTOCOL} \
        --benchmark_filter="${filter}" \
        --benchmark_min_time=${BM_MIN_TIME} \
        --benchmark_out="${prefix}_result.json" \
        --benchmark_out_format=json \
        > "${prefix}_stdout.txt" 2>&1
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "  [WARN] Step 1 exited with code ${rc}, see ${prefix}_stdout.txt"
    else
        echo "  [OK]   Step 1 done -> ${prefix}_result.json"
    fi

    # ------------------------------------------------------------------
    # Step 2 (all|perf): perf record -> perf.data
    # ------------------------------------------------------------------
    if [[ "${MODE}" == "all" || "${MODE}" == "perf" ]]; then
        echo "  [2] perf record..."
        perf record \
            --call-graph dwarf,65528 \
            -F 399 \
            -o "${prefix}_perf.data" \
            -- taskset -c ${CORE} \
               bash ${RUN_SCRIPT} ${PROTOCOL} \
                --benchmark_filter="${filter}" \
                --benchmark_repetitions=${BM_REPETITIONS} \
                > /dev/null 2>&1
        rc=$?
        if [ $rc -ne 0 ]; then
            echo "  [WARN] Step 2 (perf record) exited with code ${rc}"
        else
            echo "  [OK]   Step 2 done -> ${prefix}_perf.data"
        fi
    fi

    # ------------------------------------------------------------------
    # Step 3 (all|topdown): kperf_arm topdown / cache / tlb / imix
    #
    # Launch the benchmark in a background loop so kperf_arm always
    # finds a live workload on core ${CORE}.
    # ------------------------------------------------------------------
    if [[ "${MODE}" == "all" || "${MODE}" == "topdown" ]]; then
        echo "  [3] kperf_arm topdown collection..."

        (
            while true; do
                taskset -c ${CORE} \
                bash ${RUN_SCRIPT} ${PROTOCOL} \
                    --benchmark_filter="${filter}" \
                    --benchmark_repetitions=${BM_REPETITIONS} \
                    > /dev/null 2>&1
            done
        ) &
        local BM_BG_PID=$!

        sleep 1   # warm-up

        ${KPERF_BIN} \
            --topdown \
            --cache \
            --tlb \
            --imix \
            --duration ${KPERF_DURATION} \
            --interval ${KPERF_INTERVAL} \
            --iter-num ${KPERF_ITER} \
            --core ${CORE} \
            > "${prefix}_kperf_topdown.txt" 2>&1
        rc=$?
        if [ $rc -ne 0 ]; then
            echo "  [WARN] Step 3 (kperf_arm) exited with code ${rc}, see ${prefix}_kperf_topdown.txt"
        else
            echo "  [OK]   Step 3 done -> ${prefix}_kperf_topdown.txt"
        fi

        kill ${BM_BG_PID} 2>/dev/null
        wait ${BM_BG_PID} 2>/dev/null
    fi

    echo ""
}

# =============================================================================
# Main: iterate over all 8 groups
# =============================================================================
for i in "${!LABELS[@]}"; do
    run_and_collect "${LABELS[$i]}" "${FILTERS[$i]}"
done

echo "====================================================="
echo "All ${#LABELS[@]} groups completed."
echo "Results are in: ${RESULTS_DIR}/"
