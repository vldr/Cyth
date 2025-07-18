cmake_minimum_required(VERSION 3.16)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose Debug or Release" FORCE)
endif()

project(cyth)

if (MSVC)
    add_compile_options(/MP)
else()
    add_compile_options(-Wall -Wextra -pedantic)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_link_options(-fsanitize=address -fsanitize=undefined)
    endif()
endif()

if (EMSCRIPTEN)
    add_link_options(--closure 1 -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 -sALLOW_TABLE_GROWTH=1 
                     -sEXPORTED_RUNTIME_METHODS=addFunction,UTF8ToString,HEAPU8 -sSTACK_SIZE=131072
                     -sEXPORTED_FUNCTIONS=_free,_memory_alloc,_run,_set_error_callback,_set_result_callback)

    add_custom_target(test
        COMMAND node test/_test.js
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS cyth
    )
elseif (NOT MSVC)
    add_link_options(-pthread)
endif()

add_subdirectory(third_party/binaryen)
add_executable(cyth src/array.h src/checker.c src/checker.h src/codegen.c src/codegen.h
                    src/environment.c src/environment.h src/expression.h src/lexer.c
                    src/lexer.h src/main.c src/main.h src/map.c src/map.h src/memory.c
                    src/memory.h src/parser.c src/parser.h src/statement.h)

if (NOT MSVC)
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g -fsanitize=address -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -flto")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto -fno-rtti")
endif()

if (EMSCRIPTEN)
    add_custom_command(TARGET cyth POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_FILE_DIR:cyth>/cyth.js
                ${CMAKE_SOURCE_DIR}/editor/cyth.js
        COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_FILE_DIR:cyth>/cyth.wasm
                ${CMAKE_SOURCE_DIR}/editor/cyth.wasm)
endif()

target_link_libraries(cyth PRIVATE binaryen)