# HPC Parallel Sorting Benchmarking

High-performance parallel sorting algorithms using MPI in C++. This project implements two distributed sorting algorithms: **PSRS** (Parallel Sorting by Regular Sampling) and **Bitonic Sort**.

## Overview

This project demonstrates parallel computing skills through:
- Implementation of two parallel sorting algorithms with different communication patterns
- Detailed timing breakdowns (compute vs communication)
- CSV output for performance analysis
- Command-line interface for easy benchmarking

## Algorithms Implemented

### 1. PSRS (Parallel Sorting by Regular Sampling)
- Each rank sorts local chunk and samples regularly
- Global pivot selection via sampling
- All-to-all exchange (MPI_Alltoallv) for repartitioning
- Excellent scalability for large problem sizes

### 2. Bitonic Sort (Network-based)
- Classic sorting network implemented across ranks
- Stage-wise compare-exchange operations
- Works best with power-of-2 ranks
- Regular communication pattern with O(log²p) stages

## Requirements

- MPI implementation (OpenMPI, MPICH, Intel MPI)
- C++17 compatible compiler (GCC 7+, Clang 5+)
- CMake 3.12+

## Installation

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install build-essential cmake libopenmpi-dev openmpi-bin

# Build the project
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

## Usage

### Command Line Arguments

```bash
./benchmark <algorithm> <problem_size> <output_csv>

Arguments:
  algorithm      : psrs or bitonic
  problem_size   : number of integers to sort
  output_csv     : output CSV file name

Example:
  mpirun -np 16 ./build/benchmark psrs 100000000 results_psrs_16.csv
```

### Running Benchmarks

```bash
# Single run
mpirun -np 4 ./build/benchmark psrs 10000000 results.csv

# Run comprehensive benchmarks
bash scripts/run_bench.sh
```

## Output Format

The benchmark outputs CSV files with the following columns:
- `num_ranks`: Number of MPI ranks used
- `problem_size`: Total number of elements sorted
- `total_time`: Total execution time (seconds)
- `local_sort_time`: Average time for local sorting across ranks
- `communication_time`: Average communication time across ranks
- `merge_time`: Average merge/partition time across ranks

## Project Structure

```
.
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── include/                # Header files
│   ├── psrs_sort.h
│   ├── bitonic_sort.h
│   └── utils.h
├── src/                    # Source files
│   ├── main.cpp
│   ├── psrs_sort.cpp
│   ├── bitonic_sort.cpp
│   └── utils.cpp
├── scripts/
│   └── run_bench.sh        # Automated benchmarking script
├── build/                  # Build directory (created by cmake)
└── results/                # Benchmark results (created at runtime)
```

## Performance Results

Example results on a workstation (oversubscribed):

### PSRS Performance
- 16 ranks, 50M elements: **38.22 M elements/s**
- Scales well with increasing ranks
- Communication overhead increases with more ranks

### Bitonic Sort Performance
- 16 ranks, 50M elements: **12.25 M elements/s**
- Best with power-of-2 ranks
- Higher communication overhead due to O(log²p) stages

## Implementation Details

### PSRS Algorithm
1. **Local Sort**: Each rank sorts its local data using std::sort
2. **Regular Sampling**: Select p-1 evenly spaced samples from sorted data
3. **Global Pivot Selection**: Gather all samples, sort, and select p-1 pivots
4. **Partitioning**: Partition local data based on pivots
5. **All-to-All Exchange**: MPI_Alltoallv redistributes data
6. **Final Merge**: K-way merge of received partitions using priority queue

### Bitonic Sort Algorithm
1. **Local Sort**: Each rank sorts its local data
2. **Bitonic Stages**: O(log²p) compare-exchange stages
3. **Compare-Exchange**: Ranks exchange and merge data pairwise
4. **Network Pattern**: Follows bitonic sorting network structure

## License

This project is open source and available for educational purposes.
