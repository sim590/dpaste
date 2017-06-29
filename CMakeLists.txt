cmake_minimum_required(VERSION 3.1)
project(dpaste)
set(dpaste_VERSION 0.3.3)
add_definitions(-DVERSION="${dpaste_VERSION}")
add_definitions(-DPACKAGE_NAME="dpaste")

set(CMAKE_CXX_STANDARD 11)
set(THREADS_PREFER_PTHREAD_FLAG ON)

###################
#  CMake modules  #
###################
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

############################
#  Find required packages  #
############################
find_package(opendht 1.2.0 REQUIRED)
find_package(CURLpp REQUIRED)
find_package(glibmm REQUIRED)
find_package(B64 REQUIRED)
find_package(Threads REQUIRED)

#####################################
#  dpaste headers and source files  #
#####################################
list(APPEND dpaste_HEADERS
    src/node.h
    src/conf.h
)
list(APPEND dpaste_SOURCES
    src/main.cpp
    src/node.cpp
    src/conf.cpp
)

#################################
#  dpaste building and linking  #
#################################
include_directories(${CURLPP_INCLUDE_DIRS} ${glibmm_INCLUDE_DIRS} ${B64_INCLUDE_DIRS})
add_executable(dpaste ${dpaste_SOURCES} ${dpaste_HEADERS})
target_link_libraries(dpaste LINK_PUBLIC opendht ${CURLPP_LIBRARIES} ${glibmm_LIBRARIES} ${B64_LIBRARIES})

#####################
#  install targets  #
#####################
install(TARGETS dpaste DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

