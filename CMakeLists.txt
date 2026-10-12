cmake_minimum_required(VERSION 3.20)
project(dpaste)
include(GNUInstallDirs)
set(dpaste_VERSION 0.5.0)
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
find_package(opendht 2.4.9 REQUIRED)
find_package(CURLpp REQUIRED)
find_package(glibmm REQUIRED)
find_package(B64 REQUIRED)
find_package(Gpgme)
find_package(Threads REQUIRED)
find_package(nlohmann_json 3 REQUIRED)

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
    src/node.cpp
    src/conf.cpp
    src/http_client.cpp
    src/bin.cpp
    src/gpgcrypto.cpp
    src/log.cpp
	src/cipher.cpp
	src/aescrypto.cpp
)

#####################################
#  dpaste library and executable    #
#####################################
include_directories(${CURLPP_INCLUDE_DIRS} ${glibmm_INCLUDE_DIRS} ${B64_INCLUDE_DIRS} ${GPGME_INCLUDE_DIRS})
add_library(dpaste_lib STATIC ${dpaste_SOURCES} ${dpaste_HEADERS})
target_link_libraries(dpaste_lib PUBLIC -lopendht -lgnutls -lnettle -largon2 -lpthread ${CURLPP_LIBRARIES} ${glibmm_LIBRARIES} ${B64_LIBRARIES} -lgpgmepp ${GPGME_VANILLA_LIBRARIES})

add_executable(dpaste src/main.cpp)
target_link_libraries(dpaste PRIVATE dpaste_lib)

#####################
#  unit tests       #
#####################
option(DPASTE_BUILD_TESTS "Build the unit tests" OFF)
if(DPASTE_BUILD_TESTS)
    enable_testing()
    target_compile_definitions(dpaste_lib PUBLIC DPASTE_TEST)
    find_package(Catch2 3 REQUIRED)
    add_executable(dptest
        tests/tests.cpp
        tests/bin.cpp
        tests/node.cpp
        tests/conf.cpp
        tests/aes.cpp
        tests/http_client.cpp
    )
    target_include_directories(dptest PRIVATE src)
    target_link_libraries(dptest PRIVATE dpaste_lib Catch2::Catch2WithMain)
    include(Catch)
    catch_discover_tests(dptest WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

#####################
#  install targets  #
#####################
install(TARGETS dpaste RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES systemd/dpaste-dhtnode.service
        DESTINATION lib/systemd/user)

#  vim: set ts=4 sw=4 tw=120 noet :
