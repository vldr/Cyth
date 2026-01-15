cmake_minimum_required(VERSION 3.16)

option(WASM "WASM codegen" OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose Debug or Release" FORCE)
endif()

project(cyth)

if (NOT MSVC)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_link_options(-fsanitize=address -fsanitize=undefined)
    endif()
endif()

if (EMSCRIPTEN)
    add_link_options(--closure 1 -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 -sALLOW_TABLE_GROWTH=1 
                     -sEXPORTED_RUNTIME_METHODS=addFunction,UTF8ToString,HEAPU8 -sSTACK_SIZE=524288
                     -sEXPORTED_FUNCTIONS=_free,_memory_alloc,_run,_set_error_callback,_set_result_callback)

    add_custom_target(test
        COMMAND node test/_test_wasm_web.js
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS cyth
    )

    add_custom_target(serve
        COMMAND npx http-server -c-1 editor/
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS cyth
    )
else()
    if (NOT MSVC)
        add_link_options(-pthread -lm)
    endif()

    add_custom_target(test
        COMMAND node test/_test.js $<TARGET_FILE:cyth>
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS cyth
    )

    if (WASM)
        add_custom_target(testwasm
            COMMAND node test/_test_wasm.js $<TARGET_FILE:cyth>
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS cyth
        )
    endif()
endif()

set(SOURCES
    src/array.h src/checker.c src/checker.h src/environment.c src/environment.h
    src/expression.h src/lexer.c src/lexer.h src/main.h src/map.c
    src/map.h src/memory.c src/memory.h src/parser.c src/parser.h src/statement.h
)

if (WASM OR EMSCRIPTEN)
    add_subdirectory(third_party/binaryen)
    list(APPEND SOURCES src/codegen.c)
endif()

if (NOT EMSCRIPTEN)
    add_subdirectory(third_party/mir)
    add_subdirectory(third_party/bdwgc)
    list(APPEND SOURCES src/jit.c)

    if (MSVC)
        if (CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
            enable_language(C ASM_MARMASM)
            list(APPEND SOURCES src/longjmp.arm64.asm)
        else()
            enable_language(C ASM_MASM)
            list(APPEND SOURCES src/longjmp.x64.asm)
        endif()
    endif()
endif()

add_executable(cyth src/main.c ${SOURCES})

if (NOT MSVC)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(cyth PRIVATE -fsanitize=address -fsanitize=undefined -D_DEBUG)
    else()
        target_compile_options(cyth PRIVATE -flto=auto)
    endif()
endif()

if (NOT EMSCRIPTEN)
    add_library(libcyth STATIC ${SOURCES})
    target_include_directories(libcyth PUBLIC src/include/)

    set_property(TARGET libcyth PROPERTY OUTPUT_NAME cyth)
endif()

if (NOT MSVC)
    target_compile_options(cyth PRIVATE -Wall -Wextra -pedantic -fsigned-char)
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

if (WASM OR EMSCRIPTEN)
    target_compile_definitions(cyth PRIVATE WASM=1)
    target_link_libraries(cyth PRIVATE binaryen)

    if (NOT EMSCRIPTEN)
        target_compile_definitions(libcyth PRIVATE WASM=1)
        target_link_libraries(libcyth PRIVATE binaryen)
    endif()
endif()

if (NOT EMSCRIPTEN)
    target_link_libraries(cyth PRIVATE mir)
    target_link_libraries(cyth PRIVATE bdwgc)

    target_link_libraries(libcyth PRIVATE mir)
    target_link_libraries(libcyth PRIVATE bdwgc)
endif()

