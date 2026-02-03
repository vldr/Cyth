cmake_minimum_required(VERSION 3.16)
project(bdwgc)

add_library(bdwgc OBJECT extra/gc.c)
target_include_directories(bdwgc PUBLIC include)
