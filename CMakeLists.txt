cmake_minimum_required(VERSION 3.10)
project(webchat CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-missing-field-initializers -Wno-implicit-fallthrough -DASIO_STANDALONE")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O2 -DBRPC_ENABLE_CPU_PROFILER -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O2 -DNDEBUG -DBRPC_ENABLE_CPU_PROFILER")
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)

include(${CMAKE_CURRENT_BINARY_DIR}/conan_toolchain.cmake)

find_package(gperftools REQUIRED)
link_libraries(${gperftools_LIBRARIES})

# brpc
include_directories("${CMAKE_CURRENT_BINARY_DIR}/thirdparty/brpc/output/include")
add_subdirectory(thirdparty/brpc)

# uSockets
include_directories(thirdparty/uWebsockets/uSockets/src)
add_subdirectory(thirdparty/uWebsockets/uSockets)
link_libraries(uSockets)

include_directories(thirdparty/uWebsockets/src)
include_directories(thirdparty/rapidjson/include)

# common
include_directories(common)
file(GLOB COMMON_SOURCE "common/*.cc" "common/*.cpp")
add_library(common ${COMMON_SOURCE})
link_libraries(common)

# utils
include_directories(utils)
file(GLOB UTILS_SOURCE "utils/*.cc")
add_library(utils ${UTILS_SOURCE})
link_libraries(utils)

# thirdparty
find_package(gflags REQUIRED)
find_package(mongoc-1.0 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(protobuf REQUIRED)

include_directories(
    ${gflags_INCLUDE_DIR}
    "${mongo-c-driver_PACKAGE_FOLDER_RELEASE}/include"
)


link_libraries(
    ${gflags_LIBRARIES}
    ${mongoc-1.0_LIBRARIES}
    brpc-static
    ${OpenSSL_LIBRARIES}
    ${protobuf_LIBRARIES}
)

include_directories(${PROJECT_SOURCE_DIR})

# chat server
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/")
file(GLOB CHAT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/chat_server/*.cc")
add_executable(chat_server ${CHAT_SOURCE})
file(COPY ${PROJECT_SOURCE_DIR}/resource DESTINATION ${EXECUTABLE_OUTPUT_PATH} )