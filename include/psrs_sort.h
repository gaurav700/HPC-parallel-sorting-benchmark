#ifndef PSRS_SORT_H
#define PSRS_SORT_H

#include <vector>
#include <mpi.h>
#include "utils.h"

/**
 * PSRS - Parallel Sorting by Regular Sampling
 * 
 * Algorithm:
 * 1. Each rank sorts its local data
 * 2. Select w regular samples from each rank (w = p)
 * 3. Gather all samples at root, sort, and select p-1 pivots
 * 4. Broadcast pivots to all ranks
 * 5. Each rank partitions its data based on pivots into p buckets
 * 6. All-to-all exchange (MPI_Alltoallv) to redistribute data
 * 7. Each rank merges received partitions
 * 
 * Communication: MPI_Gather, MPI_Bcast, MPI_Alltoallv
 * Often has best practical scaling for large p
 */
void psrs_sort(std::vector<int>& local_data,
               int rank,
               int size,
               MPI_Comm comm,
               TimingData& timing);

// Helper functions
void select_regular_samples(const std::vector<int>& data,
                           std::vector<int>& samples,
                           int num_samples);

void partition_by_pivots(const std::vector<int>& data,
                        const std::vector<int>& pivots,
                        std::vector<std::vector<int>>& partitions);

void merge_partitions(const std::vector<std::vector<int>>& partitions,
                     std::vector<int>& result);

#endif // PSRS_SORT_H
