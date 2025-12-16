cmake_minimum_required(VERSION 3.16)
project(mir)

add_library(mir OBJECT mir.c mir-gen.c mir.h mir-gen.h)
target_include_directories(mir PUBLIC ${PROJECT_SOURCE_DIR})

if (MSVC)
    target_compile_options(mir PRIVATE /MP)
else()
    target_compile_options(mir PRIVATE -fsigned-char)

    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(mir PRIVATE -flto)
    endif()
endif()
