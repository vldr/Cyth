# Windows

`cmake.exe -S . -B winbuild -G "Visual Studio 17 2022"`

# Linux

Debug: `cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -S . -B build`  
Release: `cmake -DCMAKE_BUILD_TYPE=Release -S . -B build`

# Emscripten

Debug: `emcmake cmake -DCMAKE_BUILD_TYPE=Debug -S . -B embuild`  
Release: `emcmake cmake -DCMAKE_BUILD_TYPE=Release -S . -B embuild`

# MacOS

`cmake -S . -B xbuild -G Xcode`
