#include "psrs_sort.h"
#include <algorithm>
#include <iostream>
#include <queue>

void select_regular_samples(const std::vector<int>& data,
                           std::vector<int>& samples,
                           int num_samples) {
    samples.clear();
    if (data.empty() || num_samples <= 0) return;
    
    size_t step = data.size() / num_samples;
    if (step == 0) step = 1;
    
    for (int i = 0; i < num_samples && i * step < data.size(); ++i) {
        samples.push_back(data[i * step]);
    }
}

void partition_by_pivots(const std::vector<int>& data,
                        const std::vector<int>& pivots,
                        std::vector<std::vector<int>>& partitions) {
    int num_partitions = pivots.size() + 1;
    partitions.clear();
    partitions.resize(num_partitions);
    
    for (int value : data) {
        // Binary search to find which partition this value belongs to
        auto it = std::lower_bound(pivots.begin(), pivots.end(), value);
        int partition_idx = it - pivots.begin();
        partitions[partition_idx].push_back(value);
    }
}

void merge_partitions(const std::vector<std::vector<int>>& partitions,
                     std::vector<int>& result) {
    result.clear();
    
    // Calculate total size
    size_t total_size = 0;
    for (const auto& partition : partitions) {
        total_size += partition.size();
    }
    result.reserve(total_size);
    
    // Use k-way merge with priority queue
    using PQElement = std::pair<int, std::pair<int, int>>; // <value, <partition_idx, element_idx>>
    auto cmp = [](const PQElement& a, const PQElement& b) { return a.first > b.first; };
    std::priority_queue<PQElement, std::vector<PQElement>, decltype(cmp)> pq(cmp);
    
    // Initialize with first element from each partition
    for (size_t i = 0; i < partitions.size(); ++i) {
        if (!partitions[i].empty()) {
            pq.push({partitions[i][0], {i, 0}});
        }
    }
    
    // Extract min and add next element from same partition
    while (!pq.empty()) {
        auto [value, indices] = pq.top();
        pq.pop();
        
        result.push_back(value);
        
        int part_idx = indices.first;
        int elem_idx = indices.second + 1;
        
        if (elem_idx < static_cast<int>(partitions[part_idx].size())) {
            pq.push({partitions[part_idx][elem_idx], {part_idx, elem_idx}});
        }
    }
}

void psrs_sort(std::vector<int>& local_data,
               int rank,
               int size,
               MPI_Comm comm,
               TimingData& timing) {
    Timer total_timer, local_timer, comm_timer, merge_timer;
    total_timer.start();
    
    // Step 1: Local sort
    local_timer.start();
    std::sort(local_data.begin(), local_data.end());
    timing.local_sort_time = local_timer.stop();
    
    // Step 2: Regular sampling
    int samples_per_rank = size;  // w = p
    std::vector<int> local_samples;
    select_regular_samples(local_data, local_samples, samples_per_rank);
    
    // Step 3: Gather all samples at root
    comm_timer.start();
    std::vector<int> all_samples;
    if (rank == 0) {
        all_samples.resize(size * samples_per_rank);
    }
    
    // Pad local_samples if needed
    while (local_samples.size() < static_cast<size_t>(samples_per_rank)) {
        local_samples.push_back(local_samples.empty() ? 0 : local_samples.back());
    }
    
    MPI_Gather(local_samples.data(), samples_per_rank, MPI_INT,
               all_samples.data(), samples_per_rank, MPI_INT,
               0, comm);
    timing.comm_time += comm_timer.stop();
    
    // Step 4: Select pivots at root
    std::vector<int> pivots(size - 1);
    if (rank == 0) {
        std::sort(all_samples.begin(), all_samples.end());
        
        // Select p-1 pivots regularly from sorted samples
        int total_samples = all_samples.size();
        for (int i = 0; i < size - 1; ++i) {
            int idx = (i + 1) * total_samples / size;
            if (idx >= total_samples) idx = total_samples - 1;
            pivots[i] = all_samples[idx];
        }
    }
    
    // Step 5: Broadcast pivots
    comm_timer.start();
    MPI_Bcast(pivots.data(), size - 1, MPI_INT, 0, comm);
    timing.comm_time += comm_timer.stop();
    
    // Step 6: Partition local data based on pivots
    merge_timer.start();
    std::vector<std::vector<int>> partitions;
    partition_by_pivots(local_data, pivots, partitions);
    timing.merge_time += merge_timer.stop();
    
    // Step 7: Prepare for all-to-all exchange
    std::vector<int> send_counts(size);
    std::vector<int> send_displs(size);
    std::vector<int> recv_counts(size);
    std::vector<int> recv_displs(size);
    
    int send_offset = 0;
    for (int i = 0; i < size; ++i) {
        send_counts[i] = partitions[i].size();
        send_displs[i] = send_offset;
        send_offset += send_counts[i];
    }
    
    // Flatten partitions for sending
    std::vector<int> send_buffer;
    send_buffer.reserve(local_data.size());
    for (const auto& partition : partitions) {
        send_buffer.insert(send_buffer.end(), partition.begin(), partition.end());
    }
    
    // Exchange counts
    comm_timer.start();
    MPI_Alltoall(send_counts.data(), 1, MPI_INT,
                 recv_counts.data(), 1, MPI_INT, comm);
    
    // Calculate receive displacements and total receive size
    int recv_total = 0;
    for (int i = 0; i < size; ++i) {
        recv_displs[i] = recv_total;
        recv_total += recv_counts[i];
    }
    
    // Step 8: All-to-all exchange
    std::vector<int> recv_buffer(recv_total);
    MPI_Alltoallv(send_buffer.data(), send_counts.data(), send_displs.data(), MPI_INT,
                  recv_buffer.data(), recv_counts.data(), recv_displs.data(), MPI_INT,
                  comm);
    timing.comm_time += comm_timer.stop();
    
    // Step 9: Merge received partitions
    merge_timer.start();
    std::vector<std::vector<int>> received_partitions(size);
    for (int i = 0; i < size; ++i) {
        received_partitions[i].assign(
            recv_buffer.begin() + recv_displs[i],
            recv_buffer.begin() + recv_displs[i] + recv_counts[i]
        );
    }
    
    merge_partitions(received_partitions, local_data);
    timing.merge_time += merge_timer.stop();
    
    timing.total_time = total_timer.stop();
}
