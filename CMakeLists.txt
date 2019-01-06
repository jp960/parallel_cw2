cmake_minimum_required(VERSION 3.13)
project(parallel_cw2 C)

find_package(Threads)
find_package(MPI REQUIRED)

set(CMAKE_C_STANDARD 99)
SET(CMAKE_C_COMPILER mpicc)
SET(CMAKE_CXX_COMPILER mpicxx)

add_executable(parallel_cw2 main.c)

#include_directories(/usr/include/mpi/)
include_directories(${MPI_INCLUDE_PATH})

target_link_libraries(${PROJECT_NAME} pthread)



