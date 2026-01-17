# Cyth
A tiny, embeddable and statically typed language combining the simplicity of C and Python, targeting WebAssembly, x86-64, and ARM64.

- [Motivation](#motivation)
- [Binaries](#binaries)
- [Examples](#examples)
- [Building](#building)
  - [Linux](#linux)
  - [MacOS](#macos)
  - [Windows](#windows)
  - [Web](#web)
- [Overview](#overview)

## Motivation
Suppose we want to print "Hello World!". First, import the `print` function from the `std` module (you can name this anything you want) and then, call it directly from Cyth:

```cpp
import "std"
  void print(string text)

print("Hello World!")
```

In C, we initialize the Cyth runtime, bind our print implementation, and run the program:

```cpp
#include <stdio.h>
#include <cyth.h>

void print(String *string) {
  printf("%s\n", string->data);
}

int main(int argc, char *argv[]) {
  Jit* jit = cyth_init(argv[1], NULL, NULL);
  if (!jit)
    return -1;

  cyth_set_function(jit, "std.print.void(string)", (uintptr_t)print);
  cyth_generate(jit, 0);
  cyth_run(jit);
  cyth_destroy(jit);

  return 0;
}
```

That's it! With a couple of lines of code you easily interop with C from Cyth.

If you're interested, you can look at some of the [examples](#examples) or read through the [overview](#overview) of the language.
## Binaries

Precompiled binaries are available in [Releases](https://github.com/vldr/Cyth/releases).

## Examples

## Building

To build Cyth, you will need to have [CMake](https://cmake.org/) and gcc/clang/MSVC installed. To run the test suite, you will need to have [Node.js](https://nodejs.org/) (v22 or higher) installed.

If you want to build the WASM backend, provide the `-DWASM=1` flag to CMake.

### Linux

Debug:  
`cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -S . -B build`  

Release:  
`cmake -DCMAKE_BUILD_TYPE=Release -S . -B build`

Manual C compilation:  
`cc third_party/mir/mir.c third_party/mir/mir-gen.c third_party/bdwgc/extra/gc.c src/jit.c src/checker.c src/environment.c src/main.c src/memory.c src/lexer.c src/map.c src/parser.c -Ithird_party/mir -Ithird_party/bdwgc/include -fsigned-char -O3 -flto -o cyth`

### MacOS

Xcode project:  
`cmake -S . -B xbuild -G Xcode`

Makefile (Debug):  
`cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -S . -B build`

Makefile (Release):  
`cmake -DCMAKE_BUILD_TYPE=Release -S . -B build`

### Windows

Visual Studio 2022 project:  
`cmake.exe -S . -B winbuild -G "Visual Studio 17 2022"`

Visual Studio 2026 project:  
`cmake.exe -S . -B winbuild -G "Visual Studio 18 2026"`

### Web
For web builds, you will need to have [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) installed.

Debug:  
`emcmake cmake -DCMAKE_BUILD_TYPE=Debug -S . -B embuild`  

Release:  
`emcmake cmake -DCMAKE_BUILD_TYPE=Release -S . -B embuild`

## Overview