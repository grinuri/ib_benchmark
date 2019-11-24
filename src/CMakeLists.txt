file(GLOB_RECURSE SOURCES *.h *.cc)

add_executable(ib_benchmark ${SOURCES})

# add imported target
add_library(imp_boost_mpi STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_mpi PROPERTY
            IMPORTED_LOCATION /usr/lib/x86_64-linux-gnu/libboost_mpi.a)

# add imported target
add_library(imp_boost_serialization STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_serialization PROPERTY
            IMPORTED_LOCATION /usr/lib/x86_64-linux-gnu/libboost_serialization.a)

# add imported target
add_library(imp_boost_log STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_log PROPERTY
            IMPORTED_LOCATION /usr/lib/x86_64-linux-gnu/libboost_log.a)

# add imported target
add_library(imp_boost_log_setup STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_log_setup PROPERTY
            IMPORTED_LOCATION /usr/lib/x86_64-linux-gnu/libboost_log_setup.a)

# add imported target
add_library(imp_boost_thread STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_thread PROPERTY
            IMPORTED_LOCATION /usr/lib/x86_64-linux-gnu/libboost_thread.a)

# add imported target
add_library(imp_boost_filesystem STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_filesystem PROPERTY
            IMPORTED_LOCATION /usr/lib/x86_64-linux-gnu/libboost_filesystem.a)

# add imported target
add_library(imp_boost_system STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_system PROPERTY
            IMPORTED_LOCATION /usr/lib/x86_64-linux-gnu/libboost_system.a)

target_link_libraries(ib_benchmark -static-libgcc -static-libstdc++)
target_link_libraries(ib_benchmark ${MPI_LIBRARIES} pthread)
target_link_libraries(ib_benchmark imp_boost_mpi imp_boost_serialization imp_boost_log imp_boost_log_setup imp_boost_thread imp_boost_filesystem imp_boost_system)
