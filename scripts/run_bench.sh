#!/bin/bash
# Benchmarking script for parallel sorting algorithms (PSRS and Bitonic)

set -e

# Configuration
BENCHMARK_BIN="./build/benchmark"
RESULTS_DIR="./results"

# Create results directory
mkdir -p "$RESULTS_DIR"

echo "========================================="
echo "  Parallel Sorting Benchmark Suite"
echo "========================================="
echo ""

# Check if benchmark binary exists
if [ ! -f "$BENCHMARK_BIN" ]; then
    echo "Error: Benchmark binary not found at $BENCHMARK_BIN"
    echo "Please build the project first:"
    echo "  mkdir build && cd build"
    echo "  cmake -DCMAKE_BUILD_TYPE=Release .."
    echo "  make -j"
    exit 1
fi

# Benchmark parameters
ALGORITHMS=("psrs" "bitonic")
RANKS=(2 4 8 16)
PROBLEM_SIZE=50000000  # 50M elements

echo "Starting benchmarks..."
echo "Problem size: $PROBLEM_SIZE elements"
echo ""

for algo in "${ALGORITHMS[@]}"; do
    echo "=== Algorithm: $algo ==="
    OUTPUT_FILE="$RESULTS_DIR/results_${algo}.csv"
    
    for ranks in "${RANKS[@]}"; do
        echo -n "  Running with $ranks ranks... "
        
        mpirun -np $ranks "$BENCHMARK_BIN" \
            "$algo" \
            "$PROBLEM_SIZE" \
            "$OUTPUT_FILE"
        
        if [ $? -eq 0 ]; then
            echo "✓ Done"
        else
            echo "✗ Failed"
            exit 1
        fi
    done
    echo ""
done

echo "========================================="
echo "Benchmarks complete!"
echo "Results saved in: $RESULTS_DIR/"
echo "========================================="
