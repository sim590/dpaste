cmake_minimum_required (VERSION 3.1)
project (dpaste)
set(dpaste_VERSION 0.0.1)

set (CMAKE_CXX_STANDARD 11)
set (THREADS_PREFER_PTHREAD_FLAG ON)

find_package (opendht 1.2.0 REQUIRED)
find_package(Threads REQUIRED)

add_definitions(-DVERSION="${dpaste_VERSION}")

add_executable (dpaste src/dpaste.cpp src/node.cpp src/conf.cpp)
target_link_libraries (dpaste LINK_PUBLIC opendht )

install (TARGETS dpaste DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
