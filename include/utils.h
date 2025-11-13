#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>
#include <mpi.h>

// Timing structure to hold different timing components
struct TimingData {
    double total_time;
    double local_sort_time;
    double comm_time;
    double merge_time;
    double other_time;
    
    TimingData() : total_time(0), local_sort_time(0), comm_time(0), 
                   merge_time(0), other_time(0) {}
};

// Configuration structure
struct BenchmarkConfig {
    std::string algorithm;
    size_t total_size;
    int iterations;
    bool verify;
    std::string output_file;
    unsigned int seed;
    
    BenchmarkConfig() : algorithm("psrs"), total_size(1000000), 
                       iterations(5), verify(false), 
                       output_file(""), seed(42) {}
};

// Data generation
void generate_random_data(std::vector<int>& data, unsigned int seed, int rank);
void generate_uniform_data(std::vector<int>& data, int rank);

// Verification
bool verify_sorted(const std::vector<int>& data, int rank, int size, MPI_Comm comm);
bool is_locally_sorted(const std::vector<int>& data);

// Output
void write_results_csv(const std::string& filename, const BenchmarkConfig& config,
                       const TimingData& timing, int rank, int size, int iteration);

// Utilities
void print_statistics(const TimingData& timing, int rank, int size, MPI_Comm comm);
std::string get_timestamp();

// Timer class for easy timing
class Timer {
private:
    double start_time;
    bool running;
    
public:
    Timer() : start_time(0), running(false) {}
    
    void start() {
        start_time = MPI_Wtime();
        running = true;
    }
    
    double stop() {
        if (!running) return 0.0;
        double elapsed = MPI_Wtime() - start_time;
        running = false;
        return elapsed;
    }
    
    double elapsed() const {
        if (running) {
            return MPI_Wtime() - start_time;
        }
        return 0.0;
    }
};

#endif // UTILS_H
