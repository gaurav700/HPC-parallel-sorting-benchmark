#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <cstdlib>

#include "psrs_sort.h"
#include "bitonic_sort.h"
#include "utils.h"

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " <algorithm> <problem_size> <output_csv>\n\n"
              << "Arguments:\n"
              << "  algorithm      : psrs or bitonic\n"
              << "  problem_size   : number of integers to sort\n"
              << "  output_csv     : output CSV file name\n\n"
              << "Example:\n"
              << "  mpirun -np 16 " << prog_name << " psrs 100000000 results_psrs_16.csv\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // Parse command line arguments
    if (argc != 4) {
        if (rank == 0) {
            std::cerr << "Error: Invalid number of arguments\n\n";
            print_usage(argv[0]);
        }
        MPI_Finalize();
        return 1;
    }
    
    std::string algorithm = argv[1];
    size_t problem_size = std::stoull(argv[2]);
    std::string output_file = argv[3];
    
    // Validate algorithm
    if (algorithm != "psrs" && algorithm != "bitonic") {
        if (rank == 0) {
            std::cerr << "Error: Algorithm must be 'psrs' or 'bitonic'\n";
        }
        MPI_Finalize();
        return 1;
    }
    
    // Print configuration (rank 0 only)
    if (rank == 0) {
        std::cout << "Parallel Sorting Benchmark\n";
        std::cout << "==========================\n";
        std::cout << "Algorithm:     " << algorithm << "\n";
        std::cout << "Problem size:  " << problem_size << "\n";
        std::cout << "MPI ranks:     " << size << "\n";
        std::cout << "Output file:   " << output_file << "\n";
        std::cout << "==========================\n" << std::endl;
    }
    
    // Calculate local data size for each rank
    size_t base_local_size = problem_size / size;
    size_t remainder = problem_size % size;
    size_t local_size = base_local_size + (rank < static_cast<int>(remainder) ? 1 : 0);
    
    // Generate random data
    std::vector<int> local_data(local_size);
    
    if (rank == 0) {
        std::cout << "Generating random data..." << std::flush;
    }
    
    generate_random_data(local_data, 42 + rank, rank);
    
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        std::cout << " Done\n" << std::endl;
    }
    
    // Timing variables
    TimingData timing;
    double start_total = MPI_Wtime();
    
    // Run the selected algorithm
    if (algorithm == "psrs") {
        psrs_sort(local_data, rank, size, MPI_COMM_WORLD, timing);
    } else if (algorithm == "bitonic") {
        bitonic_sort(local_data, rank, size, MPI_COMM_WORLD, timing);
    }
    
    double end_total = MPI_Wtime();
    timing.total_time = end_total - start_total;
    
    // Verify correctness
    bool is_correct = verify_sorted(local_data, rank, size, MPI_COMM_WORLD);
    
    // Gather timing statistics
    double global_total_time, global_local_sort, global_comm, global_merge;
    double max_total_time, max_local_sort, max_comm, max_merge;
    
    MPI_Reduce(&timing.total_time, &global_total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&timing.local_sort_time, &global_local_sort, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&timing.comm_time, &global_comm, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&timing.merge_time, &global_merge, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    
    MPI_Reduce(&timing.total_time, &max_total_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&timing.local_sort_time, &max_local_sort, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&timing.comm_time, &max_comm, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&timing.merge_time, &max_merge, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    
    // Rank 0 outputs results
    if (rank == 0) {
        double avg_total = global_total_time / size;
        double avg_local_sort = global_local_sort / size;
        double avg_comm = global_comm / size;
        double avg_merge = global_merge / size;
        
        std::cout << "Results:\n";
        std::cout << "--------\n";
        std::cout << "Verification:        " << (is_correct ? "PASSED" : "FAILED") << "\n";
        std::cout << "Total time (max):    " << max_total_time << " s\n";
        std::cout << "Local sort (avg):    " << avg_local_sort << " s\n";
        std::cout << "Communication (avg): " << avg_comm << " s\n";
        std::cout << "Merge time (avg):    " << avg_merge << " s\n";
        std::cout << "Throughput:          " << (problem_size / max_total_time / 1e6) << " M elements/s\n";
        std::cout << std::endl;
        
        // Write CSV output
        std::ofstream csvfile;
        bool file_exists = std::ifstream(output_file).good();
        
        csvfile.open(output_file, std::ios::app);
        
        // Write header if file doesn't exist
        if (!file_exists) {
            csvfile << "num_ranks,problem_size,total_time,local_sort_time,communication_time,merge_time\n";
        }
        
        // Write data
        csvfile << size << "," 
                << problem_size << "," 
                << max_total_time << "," 
                << avg_local_sort << "," 
                << avg_comm << "," 
                << avg_merge << "\n";
        
        csvfile.close();
        
        std::cout << "Results written to: " << output_file << std::endl;
    }
    
    MPI_Finalize();
    return is_correct ? 0 : 1;
}
