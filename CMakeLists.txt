cmake_minimum_required (VERSION 3.1)
project (dpaste)
set(dpaste_VERSION 0.0.1)
add_subdirectory(cpr)

set (CMAKE_CXX_STANDARD 11)
set (THREADS_PREFER_PTHREAD_FLAG ON)

find_package (opendht 1.2.0 REQUIRED)
find_package(Threads REQUIRED)

add_definitions(-DVERSION="${dpaste_VERSION}")

include_directories(${CPR_INCLUDE_DIRS})
add_executable (dpaste src/dpaste.cpp src/node.cpp src/conf.cpp)
target_link_libraries (dpaste LINK_PUBLIC opendht ${CPR_LIBRARIES})

install (TARGETS dpaste DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
