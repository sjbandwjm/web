cmake_minimum_required(VERSION 3.10)
project(webchat CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

link_libraries(profiler)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-missing-field-initializers -Wno-implicit-fallthrough -DASIO_STANDALONE")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O2 -DBRPC_ENABLE_CPU_PROFILER -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O2 -DNDEBUG -DBRPC_ENABLE_CPU_PROFILER")

include_directories("${CMAKE_CURRENT_BINARY_DIR}/thirdparty/brpc/output/include")
add_subdirectory(thirdparty/brpc)



if(CMAKE_BUILD_TYPE STREQUAL "Release")
link_libraries(-Wl,--whole-archive brpc-static -Wl,--no-whole-archive)
link_libraries(brpc-static)
else()
 link_libraries(brpc-shared)
endif()

link_libraries(bson-1.0 crypto dl gflags gtest leveldb protoc pthread ssl z)

include_directories(thirdparty/uWebsockets/uSockets/src)
add_subdirectory(thirdparty/uWebsockets/uSockets)
link_libraries(uSockets)

include_directories(thirdparty/uWebsockets/src)