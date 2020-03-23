file(GLOB_RECURSE SOURCES *.h *.cc)

add_executable(ib_benchmark ${SOURCES})
list(GET MPI_LIBRARIES 0 MPI_LIB)
get_filename_component(MPI_PATH ${MPI_LIB} DIRECTORY)
message(BOOST ${Boost_LIBRARIES})
message(UCX ${UCX_POC_ROOT})
set(LIBS /opt/boost/1.72.0/lib/libboost_system.a /opt/boost/1.72.0/lib/libboost_serialization.a /opt/boost/1.72.0/lib/libboost_mpi.a /opt/boost/1.72.0/lib/libboost_date_time.a /opt/boost/1.72.0/lib/libboost_log_setup.a /opt/boost/1.72.0/lib/libboost_thread.a /opt/boost/1.72.0/lib/libboost_regex.a /opt/boost/1.72.0/lib/libboost_chrono.a /opt/boost/1.72.0/lib/libboost_atomic.a /opt/boost/1.72.0/lib/libboost_log.a /opt/boost/1.72.0/lib/libboost_filesystem.a )
target_link_libraries(ib_benchmark ${UCX_POC_ROOT}/build/src/libucx_poc.a ${UCX_LIBRARIES} ${MPI_LIBRARIES} ${LIBS} ${MPI_PATH}/liboshmem.so pthread)

