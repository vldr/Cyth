cmake_minimum_required(VERSION 3.16)
project(mir)

add_library(mir OBJECT mir.c mir-gen.c mir.h mir-gen.h)
target_include_directories(mir PUBLIC ${PROJECT_SOURCE_DIR})

if (NOT MSVC)
    target_compile_options(mir PRIVATE -fsigned-char)
endif()
