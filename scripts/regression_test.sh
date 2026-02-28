#!/bin/bash
#
# Golden RNG - Regression Test Suite
# Runs baseline tests and compares against previous results
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Paths
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SRC_DIR"
BASELINE_FILE="$SCRIPT_DIR/baseline_results.txt"
REPORT_FILE="$SCRIPT_DIR/regression_report.txt"

# Test results
declare -A TEST_RESULTS
PASS_COUNT=0
FAIL_COUNT=0

echo "=============================================="
echo "  Golden RNG Regression Test Suite"
echo "=============================================="
echo ""

# Build if needed
if [ ! -f "$BUILD_DIR/goldenrng" ]; then
    echo -e "${YELLOW}Building goldenrng...${NC}"
    cd "$SRC_DIR"
    make clean
    make BUILD_TYPE=release
    cd "$SCRIPT_DIR"
fi

# Function to run a test
run_test() {
    local test_name="$1"
    local test_cmd="$2"
    
    echo -n "Running $test_name... "
    
    if eval "$test_cmd" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        TEST_RESULTS["$test_name"]="PASS"
        ((PASS_COUNT++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        TEST_RESULTS["$test_name"]="FAIL"
        ((FAIL_COUNT++))
        return 1
    fi
}

# Run all tests
echo "=== Core Functionality Tests ==="
run_test "Unit Tests" "$BUILD_DIR/goldenrng -m unittests"
run_test "KAT Tests" "$BUILD_DIR/goldenrng -m kat"
run_test "Determinism Test" "$BUILD_DIR/goldenrng -m determinism"

echo ""
echo "=== Statistical Tests ==="
run_test "Advanced Tests" "$BUILD_DIR/goldenrng -m advanced"

echo ""
echo "=== Performance Baseline ==="

# Run benchmark and capture results
BENCH_OUTPUT=$($BUILD_DIR/goldenrng -m benchmark -s 10M -t 4 2>&1 || true)
SEQ_THROUGHPUT=$(echo "$BENCH_OUTPUT" | grep -oP 'Sequential.*?:\s*\K[0-9.]+' | head -1)
PAR_THROUGHPUT=$(echo "$BENCH_OUTPUT" | grep -oP 'Parallel.*?:\s*\K[0-9.]+' | head -1)

echo "  Sequential: ${SEQ_THROUGHPUT:-N/A} MB/s"
echo "  Parallel: ${PAR_THROUGHPUT:-N/A} MB/s"

# Compare against baseline if exists
if [ -f "$BASELINE_FILE" ]; then
    echo ""
    echo "=== Regression Analysis ==="
    
    BASELINE_SEQ=$(grep "sequential_mbps" "$BASELINE_FILE" | cut -d= -f2)
    BASELINE_PAR=$(grep "parallel_mbps" "$BASELINE_FILE" | cut -d= -f2)
    
    if [ -n "$BASELINE_SEQ" ] && [ -n "$SEQ_THROUGHPUT" ]; then
        SEQ_DIFF=$(echo "scale=2; ($SEQ_THROUGHPUT - $BASELINE_SEQ) / $BASELINE_SEQ * 100" | bc)
        echo "Sequential: ${SEQ_DIFF}% vs baseline"
        
        if (( $(echo "$SEQ_DIFF < -10" | bc -l) )); then
            echo -e "${RED}WARNING: Sequential performance regressed by more than 10%${NC}"
            TEST_RESULTS["perf_sequential"]="REGRESSION"
            ((FAIL_COUNT++))
        else
            TEST_RESULTS["perf_sequential"]="OK"
            ((PASS_COUNT++))
        fi
    fi
    
    if [ -n "$BASELINE_PAR" ] && [ -n "$PAR_THROUGHPUT" ]; then
        PAR_DIFF=$(echo "scale=2; ($PAR_THROUGHPUT - $BASELINE_PAR) / $BASELINE_PAR * 100" | bc)
        echo "Parallel: ${PAR_DIFF}% vs baseline"
        
        if (( $(echo "$PAR_DIFF < -10" | bc -l) )); then
            echo -e "${RED}WARNING: Parallel performance regressed by more than 10%${NC}"
            TEST_RESULTS["perf_parallel"]="REGRESSION"
            ((FAIL_COUNT++))
        else
            TEST_RESULTS["perf_parallel"]="OK"
            ((PASS_COUNT++))
        fi
    fi
else
    echo -e "${YELLOW}No baseline file found, creating baseline...${NC}"
    echo "sequential_mbps=$SEQ_THROUGHPUT" > "$BASELINE_FILE"
    echo "parallel_mbps=$PAR_THROUGHPUT" >> "$BASELINE_FILE"
    TEST_RESULTS["baseline_created"]="CREATED"
    ((PASS_COUNT++))
fi

# Generate report
echo ""
echo "=== Generating Report ==="

{
    echo "Golden RNG Regression Test Report"
    echo "================================"
    echo ""
    echo "Date: $(date)"
    echo ""
    echo "Results:"
    for test in "${!TEST_RESULTS[@]}"; do
        echo "  $test: ${TEST_RESULTS[$test]}"
    done
    echo ""
    echo "Summary:"
    echo "  Passed: $PASS_COUNT"
    echo "  Failed: $FAIL_COUNT"
    echo ""
    echo "Performance:"
    echo "  Sequential: ${SEQ_THROUGHPUT:-N/A} MB/s"
    echo "  Parallel: ${PAR_THROUGHPUT:-N/A} MB/s"
} > "$REPORT_FILE"

echo ""
echo "=============================================="
echo "  Test Summary"
echo "=============================================="
echo -e "Passed: ${GREEN}$PASS_COUNT${NC}"
echo -e "Failed: ${RED}$FAIL_COUNT${NC}"
echo ""
echo "Report saved to: $REPORT_FILE"

if [ $FAIL_COUNT -gt 0 ]; then
    exit 1
fi

exit 0

