cmake_minimum_required(VERSION 3.20)

if(TARGET pool::pool)
    return()
endif()

project(pool LANGUAGES CXX)

file(GLOB_RECURSE POOL_SRC
    "src/*.cpp"
    "include/*.h"
    "include/*.hpp"
    "include/*.inl"
)

add_library(pool STATIC ${POOL_SRC})

add_library(pool::pool ALIAS pool)

target_include_directories(pool
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(pool PUBLIC cxx_std_23)

option(POOL_BUILD_SANDBOX "Build debug sandbox executable" ON)

if(POOL_BUILD_SANDBOX)
    add_executable(pool_sandbox sandbox/main.cpp)
    target_link_libraries(pool_sandbox PRIVATE pool::pool)
    target_compile_features(pool_sandbox PRIVATE cxx_std_23)
endif()