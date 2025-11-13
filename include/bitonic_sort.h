#ifndef BITONIC_SORT_H
#define BITONIC_SORT_H

#include <vector>
#include <mpi.h>
#include "utils.h"

/**
 * Bitonic Sort (Network-based algorithm)
 * 
 * Algorithm:
 * 1. Each rank sorts its local data
 * 2. Perform log²(p) compare-exchange stages
 * 3. In each stage, pairs of ranks exchange data and keep appropriate half
 * 4. Build bitonic sequences and repeatedly merge them
 * 
 * Communication: Pairwise exchanges (MPI_Sendrecv)
 * Works best when p is power of 2
 * Regular communication pattern, good for low-latency networks
 * Time: O((n/p)log(n/p)) local + O(log²p) network stages
 */
void bitonic_sort(std::vector<int>& local_data,
                  int rank,
                  int size,
                  MPI_Comm comm,
                  TimingData& timing);

// Helper functions
void compare_exchange(std::vector<int>& local_data,
                     int partner_rank,
                     bool keep_small,
                     int rank,
                     MPI_Comm comm,
                     double& comm_time);

void merge_high(std::vector<int>& data, const std::vector<int>& received);
void merge_low(std::vector<int>& data, const std::vector<int>& received);

bool is_power_of_two(int n);

#endif // BITONIC_SORT_H
