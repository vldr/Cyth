# Windows

`cmake.exe -S . -B winbuild -G "Visual Studio 17 2022"`

# Linux

Debug: `cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -S . -B build`  
Release: `cmake -DCMAKE_BUILD_TYPE=Release -S . -B build`

Manual C compilation: `cc third_party/mir/mir.c third_party/mir/mir-gen.c third_party/bdwgc/extra/gc.c src/jit.c src/checker.c src/environment.c src/main.c src/memory.c src/lexer.c src/map.c src/parser.c -Ithird_party/mir -Ithird_party/bdwgc/include -fsigned-char -O3 -flto -o cyth`

# Emscripten

Debug: `emcmake cmake -DCMAKE_BUILD_TYPE=Debug -S . -B embuild`  
Release: `emcmake cmake -DCMAKE_BUILD_TYPE=Release -S . -B embuild`

# MacOS

`cmake -S . -B xbuild -G Xcode`
