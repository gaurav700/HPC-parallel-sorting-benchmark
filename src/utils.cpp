#include "utils.h"
#include <algorithm>
#include <random>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <numeric>
#include <cmath>
#include <climits>

void generate_random_data(std::vector<int>& data, unsigned int seed, int rank) {
    // Use rank-specific seed for reproducibility while ensuring different data per rank
    std::mt19937 gen(seed + rank * 12345);
    std::uniform_int_distribution<int> dis(0, 1000000000);
    
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = dis(gen);
    }
}

void generate_uniform_data(std::vector<int>& data, int rank) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<int>(rank * data.size() + i);
    }
}

bool is_locally_sorted(const std::vector<int>& data) {
    for (size_t i = 1; i < data.size(); ++i) {
        if (data[i] < data[i-1]) {
            return false;
        }
    }
    return true;
}

bool verify_sorted(const std::vector<int>& data, int rank, int size, MPI_Comm comm) {
    // Check local sorting
    if (!is_locally_sorted(data)) {
        if (rank == 0) {
            std::cerr << "Rank " << rank << ": Local data not sorted!" << std::endl;
        }
        return false;
    }
    
    // Check boundary conditions between ranks
    int last_elem = data.empty() ? INT_MIN : data.back();
    int next_first_elem;
    
    if (rank < size - 1) {
        MPI_Send(&last_elem, 1, MPI_INT, rank + 1, 0, comm);
    }
    
    if (rank > 0) {
        int prev_last_elem;
        MPI_Recv(&prev_last_elem, 1, MPI_INT, rank - 1, 0, comm, MPI_STATUS_IGNORE);
        
        if (!data.empty() && data[0] < prev_last_elem) {
            std::cerr << "Rank " << rank << ": Boundary condition violated! "
                      << "Previous rank last: " << prev_last_elem 
                      << ", Current rank first: " << data[0] << std::endl;
            return false;
        }
    }
    
    // Gather results
    int local_ok = 1;
    int global_ok;
    MPI_Allreduce(&local_ok, &global_ok, 1, MPI_INT, MPI_LAND, comm);
    
    return global_ok == 1;
}

void write_results_csv(const std::string& filename, const BenchmarkConfig& config,
                       const TimingData& timing, int rank, int size, int iteration) {
    if (rank != 0) return;  // Only root writes
    
    std::ofstream file;
    bool file_exists = std::ifstream(filename).good();
    
    file.open(filename, std::ios::app);
    
    // Write header if file is new
    if (!file_exists) {
        file << "timestamp,algorithm,ranks,total_size,per_rank_size,iteration,"
             << "total_time,local_sort_time,comm_time,merge_time,other_time,verified\n";
    }
    
    size_t per_rank_size = config.total_size / size;
    
    file << get_timestamp() << ","
         << config.algorithm << ","
         << size << ","
         << config.total_size << ","
         << per_rank_size << ","
         << iteration << ","
         << std::fixed << std::setprecision(6)
         << timing.total_time << ","
         << timing.local_sort_time << ","
         << timing.comm_time << ","
         << timing.merge_time << ","
         << timing.other_time << ","
         << (config.verify ? "true" : "false") << "\n";
    
    file.close();
}

void print_statistics(const TimingData& timing, int rank, int size, MPI_Comm comm) {
    // Gather timing data from all ranks
    std::vector<double> all_total(size);
    std::vector<double> all_local_sort(size);
    std::vector<double> all_comm(size);
    std::vector<double> all_merge(size);
    
    MPI_Gather(&timing.total_time, 1, MPI_DOUBLE, all_total.data(), 1, MPI_DOUBLE, 0, comm);
    MPI_Gather(&timing.local_sort_time, 1, MPI_DOUBLE, all_local_sort.data(), 1, MPI_DOUBLE, 0, comm);
    MPI_Gather(&timing.comm_time, 1, MPI_DOUBLE, all_comm.data(), 1, MPI_DOUBLE, 0, comm);
    MPI_Gather(&timing.merge_time, 1, MPI_DOUBLE, all_merge.data(), 1, MPI_DOUBLE, 0, comm);
    
    if (rank == 0) {
        auto calc_stats = [](const std::vector<double>& vec) {
            double mean = std::accumulate(vec.begin(), vec.end(), 0.0) / vec.size();
            double sq_sum = 0.0;
            for (double v : vec) {
                sq_sum += (v - mean) * (v - mean);
            }
            double stddev = std::sqrt(sq_sum / vec.size());
            double max_val = *std::max_element(vec.begin(), vec.end());
            return std::make_tuple(mean, stddev, max_val);
        };
        
        auto [mean_total, std_total, max_total] = calc_stats(all_total);
        auto [mean_local, std_local, max_local] = calc_stats(all_local_sort);
        auto [mean_comm, std_comm, max_comm] = calc_stats(all_comm);
        auto [mean_merge, std_merge, max_merge] = calc_stats(all_merge);
        
        std::cout << "\n=== Timing Statistics (across all ranks) ===" << std::endl;
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "Total Time:      " << mean_total << " ± " << std_total 
                  << " s (max: " << max_total << " s)" << std::endl;
        std::cout << "Local Sort:      " << mean_local << " ± " << std_local 
                  << " s (max: " << max_local << " s)" << std::endl;
        std::cout << "Communication:   " << mean_comm << " ± " << std_comm 
                  << " s (max: " << max_comm << " s)" << std::endl;
        std::cout << "Merge/Partition: " << mean_merge << " ± " << std_merge 
                  << " s (max: " << max_merge << " s)" << std::endl;
        
        double comm_percentage = (mean_comm / mean_total) * 100.0;
        std::cout << "\nCommunication overhead: " << comm_percentage << "%" << std::endl;
        double load_imbalance = (std_total / mean_total) * 100.0;
        std::cout << "Load imbalance (CoV): " << load_imbalance << "%" << std::endl;
    }
}

std::string get_timestamp() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
