cmake_minimum_required(VERSION 3.12)
project(ParallelSortingBenchmarks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find MPI package
find_package(MPI REQUIRED)

# Compiler flags for optimization
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Include directories
include_directories(${MPI_CXX_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/psrs_sort.cpp
    src/bitonic_sort.cpp
    src/utils.cpp
)

# Executable
add_executable(benchmark ${SOURCES})

# Link MPI
target_link_libraries(benchmark ${MPI_CXX_LIBRARIES})

# MPI compile flags
if(MPI_CXX_COMPILE_FLAGS)
    set_target_properties(benchmark PROPERTIES
        COMPILE_FLAGS "${MPI_CXX_COMPILE_FLAGS}")
endif()

# MPI link flags
if(MPI_CXX_LINK_FLAGS)
    set_target_properties(benchmark PROPERTIES
        LINK_FLAGS "${MPI_CXX_LINK_FLAGS}")
endif()

# Print build info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
message(STATUS "MPI CXX compiler: ${MPI_CXX_COMPILER}")
