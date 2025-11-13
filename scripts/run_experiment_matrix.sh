#!/bin/bash
# Minimal Experiment Matrix - Concrete Benchmarking
# 
# Machine: GAURAV (single-node workstation)
# CPU: Intel Core i5-11400H (12 cores @ 2.70GHz)
# Memory: 3.7 GB
# MPI: Open MPI 4.1.6
# Compiler: GCC 13.3.0
# Network: Ethernet (eth0)
# Note: Oversubscribed execution (ranks > physical cores)

set -e

BENCHMARK_BIN="./build/benchmark"
RESULTS_DIR="./results/experiment_matrix"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Create results directory
mkdir -p "$RESULTS_DIR"

# Experiment Parameters
ALGORITHMS=("psrs" "bitonic")
RANKS=(2 4 8 16)  # Limited due to memory constraints (3.7GB)
PROBLEM_SIZES=(10000000 100000000)  # 10M, 100M (500M exceeds available memory)
RUNS_PER_POINT=5

echo "=============================================="
echo "  Minimal Experiment Matrix - Concrete"
echo "=============================================="
echo ""
echo "Machine Configuration:"
echo "  Hostname:     GAURAV"
echo "  CPU:          Intel Core i5-11400H @ 2.70GHz"
echo "  Cores:        12"
echo "  Memory:       3.7 GB"
echo "  MPI:          Open MPI 4.1.6"
echo "  Compiler:     GCC 13.3.0"
echo "  Network:      Ethernet (eth0)"
echo ""
echo "Experiment Parameters:"
echo "  Algorithms:   PSRS, Bitonic"
echo "  Problem sizes: 10M, 100M keys"
echo "  Ranks:        2, 4, 8, 16"
echo "  Runs/point:   $RUNS_PER_POINT"
echo ""
echo "=============================================="
echo ""

# Check if benchmark binary exists
if [ ! -f "$BENCHMARK_BIN" ]; then
    echo "Error: Benchmark binary not found at $BENCHMARK_BIN"
    exit 1
fi

# Total number of experiments
TOTAL_EXPERIMENTS=$((${#ALGORITHMS[@]} * ${#RANKS[@]} * ${#PROBLEM_SIZES[@]} * RUNS_PER_POINT))
CURRENT_EXPERIMENT=0

echo "Total experiments to run: $TOTAL_EXPERIMENTS"
echo ""

# Run experiments
for algo in "${ALGORITHMS[@]}"; do
    echo "=========================================="
    echo "Algorithm: $algo"
    echo "=========================================="
    
    for size in "${PROBLEM_SIZES[@]}"; do
        SIZE_MB=$((size * 4 / 1024 / 1024))
        echo ""
        echo "--- Problem Size: $size elements (~${SIZE_MB} MB) ---"
        
        for ranks in "${RANKS[@]}"; do
            OUTPUT_FILE="$RESULTS_DIR/${algo}_${size}_${ranks}ranks_${TIMESTAMP}.csv"
            
            echo -n "  Ranks $ranks: "
            
            # Run multiple iterations
            for run in $(seq 1 $RUNS_PER_POINT); do
                CURRENT_EXPERIMENT=$((CURRENT_EXPERIMENT + 1))
                echo -n "[$run]"
                
                mpirun --oversubscribe -np $ranks "$BENCHMARK_BIN" \
                    "$algo" \
                    "$size" \
                    "$OUTPUT_FILE" \
                    > /dev/null 2>&1
                
                if [ $? -ne 0 ]; then
                    echo " FAILED"
                    exit 1
                fi
            done
            
            # Calculate statistics from CSV
            if [ -f "$OUTPUT_FILE" ]; then
                echo -n " → "
                # Extract last 5 runs and calculate mean
                tail -n $RUNS_PER_POINT "$OUTPUT_FILE" | awk -F',' '
                    NR==1 { next }  # Skip if header
                    { 
                        sum_total += $3
                        sum_local += $4
                        sum_comm += $5
                        sum_merge += $6
                        count++
                    }
                    END {
                        if (count > 0) {
                            printf "Total: %.4fs", sum_total/count
                        }
                    }
                '
                echo ""
            fi
        done
    done
    echo ""
done

echo ""
echo "=============================================="
echo "Experiments Complete!"
echo "=============================================="
echo ""
echo "Results saved in: $RESULTS_DIR/"
echo ""
echo "Generating summary statistics..."

# Generate summary report
SUMMARY_FILE="$RESULTS_DIR/EXPERIMENT_SUMMARY_${TIMESTAMP}.txt"

cat > "$SUMMARY_FILE" << EOF
Minimal Experiment Matrix - Results Summary
Generated: $(date)

Machine Configuration:
  Hostname:     GAURAV
  CPU:          Intel Core i5-11400H @ 2.70GHz (12 cores)
  Memory:       3.7 GB
  MPI:          Open MPI 4.1.6
  Compiler:     GCC 13.3.0
  Network:      Ethernet (eth0)
  Note:         Oversubscribed execution (ranks > cores)

Experiment Parameters:
  Algorithms:   PSRS, Bitonic
  Problem sizes: 10M, 100M elements
  Ranks:        2, 4, 8, 16 (powers of two)
  Runs/point:   $RUNS_PER_POINT

Results (Mean ± Std):
=====================

EOF

# Process results for each configuration
for algo in "${ALGORITHMS[@]}"; do
    echo "Algorithm: $algo" >> "$SUMMARY_FILE"
    echo "-----------------------------------" >> "$SUMMARY_FILE"
    
    for size in "${PROBLEM_SIZES[@]}"; do
        SIZE_MB=$((size * 4 / 1024 / 1024))
        echo "" >> "$SUMMARY_FILE"
        echo "Problem Size: $size elements (~${SIZE_MB} MB)" >> "$SUMMARY_FILE"
        
        for ranks in "${RANKS[@]}"; do
            OUTPUT_FILE="$RESULTS_DIR/${algo}_${size}_${ranks}ranks_${TIMESTAMP}.csv"
            
            if [ -f "$OUTPUT_FILE" ]; then
                # Calculate mean and std
                tail -n $RUNS_PER_POINT "$OUTPUT_FILE" | awk -F',' -v ranks="$ranks" -v size="$size" '
                    BEGIN {
                        count = 0
                        sum_total = 0
                        sum_local = 0
                        sum_comm = 0
                        sum_merge = 0
                    }
                    NF == 6 && $1 ~ /^[0-9]+$/ {
                        total[count] = $3
                        local_sort[count] = $4
                        comm[count] = $5
                        merge[count] = $6
                        
                        sum_total += $3
                        sum_local += $4
                        sum_comm += $5
                        sum_merge += $6
                        count++
                    }
                    END {
                        if (count > 0) {
                            mean_total = sum_total / count
                            mean_local = sum_local / count
                            mean_comm = sum_comm / count
                            mean_merge = sum_merge / count
                            
                            # Calculate standard deviation
                            var_total = 0
                            var_local = 0
                            var_comm = 0
                            var_merge = 0
                            
                            for (i = 0; i < count; i++) {
                                var_total += (total[i] - mean_total)^2
                                var_local += (local_sort[i] - mean_local)^2
                                var_comm += (comm[i] - mean_comm)^2
                                var_merge += (merge[i] - mean_merge)^2
                            }
                            
                            std_total = sqrt(var_total / count)
                            std_local = sqrt(var_local / count)
                            std_comm = sqrt(var_comm / count)
                            std_merge = sqrt(var_merge / count)
                            
                            throughput = size / mean_total / 1e6
                            
                            printf "  Ranks %2d: Total: %.4f ± %.4f s  ", ranks, mean_total, std_total
                            printf "Throughput: %.2f M elem/s\n", throughput
                            printf "             Local: %.4f ± %.4f s  ", mean_local, std_local
                            printf "Comm: %.4f ± %.4f s  ", mean_comm, std_comm
                            printf "Merge: %.4f ± %.4f s\n", mean_merge, std_merge
                        }
                    }
                ' >> "$SUMMARY_FILE"
            fi
        done
    done
    echo "" >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"
done

echo ""
cat "$SUMMARY_FILE"
echo ""
echo "Summary saved to: $SUMMARY_FILE"
