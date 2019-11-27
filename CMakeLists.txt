cmake_minimum_required(VERSION 2.8.12)
set(CMAKE_CXX_COMPILER "/opt/rh/devtoolset-8/root/usr/bin/g++")
set(CMAKE_C_COMPILER "/opt/rh/devtoolset-8/root/usr/bin/gcc")

project(ib_benchmark)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++17 -Wall -Wextra -O3")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

# global dependencies
find_package(MPI REQUIRED)
include_directories(${MPI_INCLUDE_PATH})
# find_package(Boost 1.69.0 REQUIRED)
include_directories("/home/uri/Downloads/boost_1_71_0")

# include directories
include_directories(${PROJECT_SOURCE_DIR}/src)

add_subdirectory(src)
