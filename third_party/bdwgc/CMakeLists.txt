cmake_minimum_required(VERSION 3.16)
project(bdwgc)

add_library(bdwgc OBJECT extra/gc.c)
target_include_directories(bdwgc PUBLIC include)

if (MSVC)
    target_compile_options(bdwgc PRIVATE /MP)
else()
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(bdwgc PRIVATE -flto)
    endif()
endif()
