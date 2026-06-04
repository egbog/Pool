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