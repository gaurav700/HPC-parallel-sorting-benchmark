#include "bitonic_sort.h"
#include <algorithm>
#include <iostream>
#include <cmath>

bool is_power_of_two(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

void merge_low(std::vector<int>& data, const std::vector<int>& received) {
    // Keep the smaller half after merging
    std::vector<int> merged;
    merged.reserve(data.size() + received.size());
    
    size_t i = 0, j = 0;
    while (i < data.size() && j < received.size()) {
        if (data[i] <= received[j]) {
            merged.push_back(data[i++]);
        } else {
            merged.push_back(received[j++]);
        }
    }
    while (i < data.size()) {
        merged.push_back(data[i++]);
    }
    while (j < received.size()) {
        merged.push_back(received[j++]);
    }
    
    // Keep only the first half
    data.assign(merged.begin(), merged.begin() + data.size());
}

void merge_high(std::vector<int>& data, const std::vector<int>& received) {
    // Keep the larger half after merging
    std::vector<int> merged;
    merged.reserve(data.size() + received.size());
    
    size_t i = 0, j = 0;
    while (i < data.size() && j < received.size()) {
        if (data[i] <= received[j]) {
            merged.push_back(data[i++]);
        } else {
            merged.push_back(received[j++]);
        }
    }
    while (i < data.size()) {
        merged.push_back(data[i++]);
    }
    while (j < received.size()) {
        merged.push_back(received[j++]);
    }
    
    // Keep only the second half
    size_t start = merged.size() - data.size();
    data.assign(merged.begin() + start, merged.end());
}

void compare_exchange(std::vector<int>& local_data,
                     int partner_rank,
                     bool keep_small,
                     int rank,
                     MPI_Comm comm,
                     double& comm_time) {
    Timer comm_timer;
    comm_timer.start();
    
    int local_size = local_data.size();
    int partner_size;
    
    // Exchange sizes
    MPI_Sendrecv(&local_size, 1, MPI_INT, partner_rank, 0,
                 &partner_size, 1, MPI_INT, partner_rank, 0,
                 comm, MPI_STATUS_IGNORE);
    
    // Exchange data
    std::vector<int> partner_data(partner_size);
    MPI_Sendrecv(local_data.data(), local_size, MPI_INT, partner_rank, 1,
                 partner_data.data(), partner_size, MPI_INT, partner_rank, 1,
                 comm, MPI_STATUS_IGNORE);
    
    comm_time += comm_timer.stop();
    
    // Merge and keep appropriate half
    if (keep_small) {
        merge_low(local_data, partner_data);
    } else {
        merge_high(local_data, partner_data);
    }
}

void bitonic_sort(std::vector<int>& local_data,
                  int rank,
                  int size,
                  MPI_Comm comm,
                  TimingData& timing) {
    Timer total_timer, local_timer;
    total_timer.start();
    
    // Check if size is power of 2
    if (!is_power_of_two(size)) {
        if (rank == 0) {
            std::cerr << "Warning: Bitonic sort works best with power-of-2 ranks. "
                      << "Current size: " << size << std::endl;
        }
    }
    
    // Step 1: Local sort
    local_timer.start();
    std::sort(local_data.begin(), local_data.end());
    timing.local_sort_time = local_timer.stop();
    
    // Step 2: Bitonic merge network
    // The algorithm has log²(p) stages
    int num_stages = 0;
    for (int temp = size; temp > 1; temp /= 2) {
        num_stages++;
    }
    
    for (int stage = 0; stage < num_stages; ++stage) {
        for (int step = stage; step >= 0; --step) {
            int partner_distance = 1 << step;  // 2^step
            int partner_rank = rank ^ partner_distance;
            
            if (partner_rank >= size) continue;
            
            // Determine direction of sorting for this stage
            int stage_size = 1 << (stage + 1);  // 2^(stage+1)
            bool ascending = ((rank / stage_size) % 2) == 0;
            
            // Determine if we keep small or large elements
            bool keep_small;
            if (rank < partner_rank) {
                keep_small = ascending;
            } else {
                keep_small = !ascending;
            }
            
            // Perform compare-exchange
            compare_exchange(local_data, partner_rank, keep_small, rank, comm, timing.comm_time);
        }
    }
    
    timing.total_time = total_timer.stop();
}
