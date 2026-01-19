# Cyth
A tiny, embeddable and statically typed programming language inspired by C and Python, targeting WebAssembly, x86-64, and ARM64.

- [Motivation](#motivation)
- [Binaries](#binaries)
- [Examples](#examples)
- [Building](#building)
  - [Linux](#linux)
  - [MacOS](#macos)
  - [Windows](#windows)
  - [Web](#web)
- [Overview](#overview)
  - [Primitive Types](#primitive-types)
    - [`bool`](#bool)
    - [`char`](#char)
    - [`int`](#int)
    - [`float`](#float)
  - [Types](#types)
    - [`string`](#string)
    - [`any`](#any)
    - [Objects](#objects)
    - [Arrays](#arrays)
    - [Function Pointers](#function-pointers)

## Motivation
Suppose we want to print "Hello World!". In most embedded languages, this takes boilerplate, stack juggling, and wrappers. In Cyth, you just import the function and call it.

```cpp
import "std"
  void print(string text)

print("Hello World!")
```

On the C side, we have to just initialize the Cyth runtime, provide our implementation of `print` and run the program:

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

That is it. With just a few lines of code, Cyth can call into C, and C can call back into Cyth.

If you're interested, you can look at some of the [examples](#examples) or read through the [overview](#overview) of the language.
## Binaries

Precompiled binaries are available in [Releases](https://github.com/vldr/Cyth/releases).

## Examples

## Building

To build Cyth, you will need to have [CMake](https://cmake.org/) and gcc/clang/MSVC installed. To run the test suite, you will need to have [Node.js](https://nodejs.org/) (v20 or higher) installed.

If you want to build the WASM backend, provide the `-DWASM=1` flag to CMake.

### Linux

_Debug_:  
`cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -S . -B build`  

_Release_:  
`cmake -DCMAKE_BUILD_TYPE=Release -S . -B build`

_Manual C compilation_:  
`cc third_party/mir/mir.c third_party/mir/mir-gen.c third_party/bdwgc/extra/gc.c src/jit.c src/checker.c src/environment.c src/main.c src/memory.c src/lexer.c src/map.c src/parser.c -Ithird_party/mir -Ithird_party/bdwgc/include -fsigned-char -O3 -flto -o cyth`

### MacOS

_Xcode project_:  
`cmake -S . -B xbuild -G Xcode`

_Makefile (Debug)_:  
`cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -S . -B build`

_Makefile (Release)_:  
`cmake -DCMAKE_BUILD_TYPE=Release -S . -B build`

### Windows

_Visual Studio 2022 project_:  
`cmake.exe -S . -B winbuild -G "Visual Studio 17 2022"`

_Visual Studio 2026 project_:  
`cmake.exe -S . -B winbuild -G "Visual Studio 18 2026"`

### Web
For web builds, you will need to have [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) installed.

_Debug_:  
`emcmake cmake -DCMAKE_BUILD_TYPE=Debug -S . -B embuild`  

_Release_:  
`emcmake cmake -DCMAKE_BUILD_TYPE=Release -S . -B embuild`

## Overview

### Primitive Types
#### `bool`
Possible values: `false` or `true`  
Default value: `false`  

_Example:_
```cpp
bool mybool = true
```

#### `char`
Possible values: `0` to `255`  
Default value: `'\0'`  

_Example:_
```cpp
char mychar = 'a'
```

#### `int`
Possible values: `-2147483648` to `2147483647`  
Default value: `0`  

_Example:_
```cpp
int myint = 10
```

#### `float`
Possible values: `± 1.5 x 10−45` to `± 3.4 x 1038`  
Default value: `0.0`  

_Example:_
```cpp
float myfloat = 12.25
```

### Types

#### `string`
Possible values: UTF-8 text  
Default value: `""` (empty string)  

_Example:_
```cpp
string mystring = "hello world"
```

### `any`
Possible values: `null`, `string`, Array or Object   
Default value: null 
- Casting `any` to the incorrect underlying type will trigger a panic. 

_Example:_
```cpp
any myany = "hello world"
string mystring = (string)myany
```

#### Objects
Possible values: `null` or a valid pointer (reference).  
Default value: `null`  

_Example:_

```cpp
class Vector
  float x
  float y
  float z

  void __init__(int x, int y, int z)
    this.x = x
    this.y = y
    this.z = z

Vector myvector = Vector(10, 20, 30)
```

Although the keyword `class` is used, there is no support for inheritance or other common object-oriented concepts in Cyth.

Objects in Cyth closely resemble structs rather than proper classes, except there are method functions.

```cpp
class Vector
  float x
  float y
  float z

  float length()
    return (x*x  + y*y + z*z).sqrt()
```

All method functions have an implicit `this` parameter.

> Cyth objects are compatible with C structs. In C, the Vector object would look like:
> ```c
> struct Vector {
>   float x;
>   float y;
>   float z;
> };
> ```
> 
> Calling the `length` method function from C would look like:
> ```c
> typedef float (*LengthFunction)(Vector*);
>
> LengthFunction length = (LengthFunction) cyth_get_function(jit, "Vector.length.float()");
> 
> Vector vec = {1.0f, 1.0f, 1.0f};
> float len = length(&vec);
> ```
>

Objects have special method functions:

*Constructors*
```cpp
void __init__()
```

*Index overload*
```cpp
V __get__(T index)
```


*Index and assign overload*
```cpp
V __set__(T index, U value)
```

*Operator overloads*
```python
V __add__(T other)
V __sub__(T other)
V __div__(T other)
V __mul__(T other)
V __mod__(T other)
V __and__(T other)
V __or__(T other)
V __xor__(T other)
V __lshift__(T other)
V __rshift__(T other)
V __lt__(T other)
V __le__(T other)
V __gt__(T other)
V __ge__(T other)
V __eq__(T other)
V __ne__(T other)
V __str__(T other)
```

#### Arrays
Possible values: Empty list or a list of one or more elements.  
Default value: `[]` (empty list)   

- Arrays can be multi-dimensional.
- All arrays are dynamic, meaning they can be resized.

_Example:_

```cpp
int[] a 
a.push(1)
a.push(2)
a.push(3)

string[][] b = [["I'm", "multidimensional"]]
```

#### Function Pointers
Possible values: `null` or a valid pointer (reference).  
Default value: `null`  

- Function pointers cannot be placed into `any`.

_Example:_

```cpp
int adder(int a, int b)
  return a + b

int(int, int) myfunctionpointer = adder
myfnptr(10, 20)
```

> Cyth function pointers are compatible with C.