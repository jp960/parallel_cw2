cmake_minimum_required(VERSION 3.13)
project(parallel_cw2 C)
find_package(Threads)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_FLAGS -pthread)

add_executable(parallel_cw2 main.c)

target_link_libraries(${PROJECT_NAME} pthread)