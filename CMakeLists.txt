cmake_minimum_required(VERSION 3.12)

project(ib_benchmark)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++17 -g -O3 -flto")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

# global dependencies
set(MPI_ROOT /usr/local/)
set(MPI_HOME /usr/local/)
set(MPI_C /usr/local/bin/mpicc)
#find_package(MPI REQUIRED)
add_definitions(-DOMPI_SKIP_MPICXX)
set(MPI_INCLUDE_PATH, /usr/local/include/)
include_directories(${MPI_INCLUDE_PATH})

set(UCX_POC_ROOT ${PROJECT_SOURCE_DIR}/../ucx_poc)


#set(Boost_NO_BOOST_CMAKE ON)
set(Boost_USE_STATIC_LIBS ON)
set (BOOST_ROOT /opt/boost/1.72.0)
find_package(Boost 1.72.0 REQUIRED COMPONENTS system filesystem serialization log mpi)
set(CMAKE_INSTALL_RPATH ${Boost_LIBRARY_DIRS})

set(UCX_LIBRARIES_DIR /usr/lib)
#set(UCX_LIBRARIES ${UCX_LIBRARIES_DIR}/libucp.a ${UCX_LIBRARIES_DIR}/libuct.a ${UCX_LIBRARIES_DIR}/libucs.a ${UCX_LIBRARIES_DIR}/libucm.a)
set(UCX_LIBRARIES ucp uct ucs ucm)

# include directories
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${UCX_POC_ROOT}/src)
link_directories(${Boost_LIBRARY_DIRS})
link_directories(${UCX_POC_ROOT}/build/src)
link_directories(${UCX_LIBRARIES_DIR})

add_subdirectory(src)


