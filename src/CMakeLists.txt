file(GLOB_RECURSE SOURCES *.h *.cc)

add_executable(ib_benchmark ${SOURCES})

set(BOOST_LIBS_DIR "/home/uri/Downloads/boost_1_71_0/stage/lib")

# add imported target
add_library(imp_boost_mpi STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_mpi PROPERTY
            IMPORTED_LOCATION ${BOOST_LIBS_DIR}/libboost_mpi.a)

# add imported target
add_library(imp_boost_serialization STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_serialization PROPERTY
            IMPORTED_LOCATION ${BOOST_LIBS_DIR}/libboost_serialization.a)


# add imported target
add_library(imp_boost_log_setup STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_log_setup PROPERTY
            IMPORTED_LOCATION ${BOOST_LIBS_DIR}/libboost_log_setup.a)



# add imported target
add_library(imp_boost_thread STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_thread PROPERTY
            IMPORTED_LOCATION ${BOOST_LIBS_DIR}/libboost_thread.a)

# add imported target
add_library(imp_boost_filesystem STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_filesystem PROPERTY
            IMPORTED_LOCATION ${BOOST_LIBS_DIR}/libboost_filesystem.a)

# add imported target
add_library(imp_boost_system STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_system PROPERTY
            IMPORTED_LOCATION ${BOOST_LIBS_DIR}/libboost_system.a)

# add imported target
add_library(imp_boost_log STATIC IMPORTED)
# point the imported target at the real file
set_property(TARGET imp_boost_log PROPERTY
            IMPORTED_LOCATION ${BOOST_LIBS_DIR}/libboost_log.a)

target_link_libraries(ib_benchmark -static-libgcc -static-libstdc++)
target_link_libraries(ib_benchmark ${MPI_LIBRARIES} pthread)
target_link_libraries(ib_benchmark imp_boost_mpi imp_boost_serialization imp_boost_log_setup imp_boost_log imp_boost_filesystem  imp_boost_thread imp_boost_system)
