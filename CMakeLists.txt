cmake_minimum_required(VERSION 3.1)
project(dpaste)
set(dpaste_VERSION 0.4.0)
add_definitions(-DVERSION="${dpaste_VERSION}")
add_definitions(-DPACKAGE_NAME="dpaste")

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

###################
#  CMake modules  #
###################
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

############################
#  Find required packages  #
############################
find_package(opendht 1.2.0 REQUIRED)
find_package(CURLpp 0.8.1 REQUIRED)
find_package(glibmm REQUIRED)
find_package(B64 REQUIRED)
find_package(Gpgme)
find_package(Threads REQUIRED)
find_package(nlohmann_json 2.1.1 REQUIRED)

#####################################
#  dpaste headers and source files  #
#####################################
list(APPEND dpaste_HEADERS
    src/node.h
    src/conf.h
    src/http_client.h
    src/bin.h
    src/gpgcrypto.h
    src/log.h
	src/cipher.h
	src/aescrypto.h
)
list(APPEND dpaste_SOURCES
    src/main.cpp
    src/node.cpp
    src/conf.cpp
    src/http_client.cpp
    src/bin.cpp
    src/gpgcrypto.cpp
    src/log.cpp
	src/cipher.cpp
	src/aescrypto.cpp
)

#################################
#  dpaste building and linking  #
#################################
include_directories(${CURLPP_INCLUDE_DIRS} ${glibmm_INCLUDE_DIRS} ${B64_INCLUDE_DIRS} ${GPGME_INCLUDE_DIRS})
add_executable(dpaste ${dpaste_SOURCES} ${dpaste_HEADERS})
target_link_libraries(dpaste LINK_PUBLIC opendht -lgnutls -lnettle -largon2 -lpthread ${CURLPP_LIBRARIES} ${glibmm_LIBRARIES} ${B64_LIBRARIES} -lgpgmepp ${GPGME_VANILLA_LIBRARIES})

#####################
#  install targets  #
#####################
install(TARGETS dpaste DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

#  vim: set ts=4 sw=4 tw=120 noet :

