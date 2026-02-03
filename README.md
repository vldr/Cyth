# Cyth
A tiny, embeddable and statically typed programming language inspired by C and Python, targeting WebAssembly, x86-64, and ARM64.

- [Try it out](#try-it-out)
- [Motivation](#motivation)
- [Binaries](#binaries)
- [Examples](#examples)
- [Building](#building)
  - [Linux](#linux)
  - [MacOS](#macos)
  - [Windows](#windows)
  - [Web](#web)
- [Overview](#overview)

## Try it out

You can try out Cyth in the web playground:
[https://cyth.vldr.org](https://cyth.vldr.org)

## Motivation
Suppose we want to call a native C function from Cyth; for example, to print `Hello World!`. In Cyth, you just import the function and call it: no stack juggling and no wrapper code.

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

- [Raylib](https://github.com/vldr/CythRay)  
Cyth with Raylib bindings, allowing direct access to Raylib functions from a Cyth script file.

## Building

To build Cyth, you will need to have [CMake](https://cmake.org/) and gcc/clang/MSVC installed. To run the test suite, you will need to have [Node.js](https://nodejs.org/) (v20 or higher) installed.

If you want to build the WASM backend, provide the `-DWASM=1` flag to CMake.

_Note:_ The supported platforms for the JIT compiler are:
- Windows x64 and ARM64
- Linux x64 and ARM64
- MacOS x64 and ARM64

### Linux

Run the following commands from the root directory (in a terminal):

_Debug_:  
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -S . -B build
cd build
make
```

_Release_:  
```bash
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cd build
make
```

_Manual C compilation_:  
`cc third_party/mir/mir.c third_party/mir/mir-gen.c third_party/bdwgc/extra/gc.c src/jit.c src/checker.c src/environment.c src/main.c src/memory.c src/lexer.c src/map.c src/parser.c -Ithird_party/mir -Ithird_party/bdwgc/include -fsigned-char -O3 -o cyth`

### MacOS

Run the following commands from the root directory (in a terminal):

_Xcode project_:  
```
cmake -S . -B xbuild -G Xcode
```

Then, in the `xbuild` directory, open `cyth.xcodeproj` in Xcode.

_Makefile (Debug)_:  
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -S . -B build
cd build
make
```

_Makefile (Release)_:  
```bash
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cd build
make
```

### Windows

Run the following commands from the root directory (in a terminal):

_Visual Studio 2022 project_:  
```
cmake.exe -S . -B winbuild -G "Visual Studio 17 2022"
```

_Visual Studio 2026 project_:  
```
cmake.exe -S . -B winbuild -G "Visual Studio 18 2026"
```

Then, in the `winbuild` directory, open `cyth.sln` / `cyth.slnx` in Visual Studio.

### Web
For web builds, you will need to have [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) installed.

Run the following commands from the root directory (in a terminal):

_Debug_:  
```
emcmake cmake -DCMAKE_BUILD_TYPE=Debug -S . -B embuild
cd embuild
make
``` 

_Release_:  
```
emcmake cmake -DCMAKE_BUILD_TYPE=Release -S . -B embuild
cd embuild
make
```

## Overview

- [Primitive Types](#primitive-types)
  - [`bool`](#bool)
  - [`char`](#char)
  - [`int`](#int)
  - [`float`](#float)
- [Types](#types)
  - [`string`](#string)
  - [`any`](#any)
  - [Arrays](#arrays)
  - [Objects](#objects)
  - [Function Pointers](#function-pointers)
- [Variables](#variables)
- [Functions](#functions)
- [`if` statement](#if-statement)
- [`while` loop](#while-loop)
- [`for` loop](#for-loop)
- [`break` statement](#break-statement)
- [`continue` statement](#continue-statement)


### Primitive Types
#### `bool`
Possible values: `false` or `true`  
Default value: `false`  

_Example:_
```cpp
bool myBool = true
```

#### `char`
Possible values: `0` to `255`  
Default value: `'\0'`  

_Example:_
```cpp
char myChar = 'a'
```

#### `int`
Possible values: `-2147483648` to `2147483647`  
Default value: `0`  

_Example:_
```cpp
int myInt = 10
```

#### `float`
Possible values: `± 1.5 x 10−45` to `± 3.4 x 1038`  
Default value: `0.0`  

_Example:_
```cpp
float myFloat = 12.25
```

### Types

#### `string`
Possible values: UTF-8 text  
Default value: `""` (empty string)  

- All types can be cast to a `string`, which will convert to the type's string representation; casting `any` to a string will attempt to convert the `any` to the underlying string type rather than its string representation.

_Example:_
```cpp
string myString = "hello world"
```

### `any`
Possible values: `null`, `string`, Array or Object   
Default value: null 
- Casting `any` to the incorrect underlying type will trigger a panic. 

_Example:_
```cpp
any myAny = "hello world"
string myString = (string)myAny
```

#### Arrays
Possible values: Empty list or a list of one or more elements.  
Default value: `[]` (empty list)   

- Arrays can be multi-dimensional.
- All arrays are dynamic, meaning they can be resized.

_Example:_

```cpp
int[] myArray
myArray.push(1)
myArray.push(2)
myArray.push(3)

string[][] myArray2D = [["I'm", "multidimensional"]]
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

Vector myVector = Vector(10, 20, 30)
```

Although the keyword `class` is used, there is no support for inheritance or other common object-oriented concepts in Cyth.

Objects in Cyth closely resemble structs rather than proper classes, except there are method functions. All method functions have an implicit `this` parameter.

_Example:_

```cpp
class Vector
  float x
  float y
  float z

  float length()
    return (x*x  + y*y + z*z).sqrt()
```

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


#### Function Pointers
Possible values: `null` or a valid pointer (reference).  
Default value: `null`  

- Function pointers cannot be placed into `any` (limitation added due to WASM not supporting them).
- Function pointers compatible with C function pointers.

_Example:_

```cpp
int adder(int a, int b)
  return a + b

int(int, int) myFunctionPointer = adder
myFunctionPointer(10, 20)
```

_Example (Function Member):_

```cpp
class Vector
  float x
  float y
  float z

  float length()
    return (x * x  + y * y + z * z).sqrt()

Vector myVector = Vector()

float(Vector) myFunctionPointer = Vector.length
myFunctionPointer(myVector)
```

## Variables

You can declare variables like this:

```c
int myVariable = 12
float mySecondVariable
```

If you do not initialize a variable, a default value is assigned automatically. For example, `mySecondVariable` will be set to `0.0`.

You can declare variables in the top-level scope of your program which will make them a global variable. 

> You can easily access global variables from C using `cyth_get_variable`.
>
> For example, to get `myVariable` from C, you would write:
> ```c
> int* myVariable = (int*) cyth_get_variable(jit, "myVariable.int");
> ```
> The address returned from `cyth_get_variable` will be `NULL` if it was not found, or the signature is incorrect.
>
> Make sure to only call `cyth_get_variable` after calling `cyth_run`, otherwise global variables will be 
> uninitialized which can lead to issues if you're using types that have special default initializations (like arrays and strings).
>

## Functions

You can declare functions like this:

```c
int myFunction(int a, int b)
  return a + b
```

You can place functions inside other functions:

```c
int myFunction(int a, int b)
  int myInnerFunction(int c)
    return 2 * c
  
  return myInnerFunction(a + b)
```

Nested functions are **not** closures, meaning they can't access variables outside their body.

Functions can appear inside objects making them method functions:

```python
class MyClass
  int a
  int b

  int myFunction()
    return this.a + this.b
```

Method functions have an implicit `this` parameter which is a pointer to the object itself. This can be `null` if the method is called on a `null` pointer.

```python
class MyClass
  int a
  int b

  int myFunction()
    int myInnerFunction()
      return 2 * (this.a + this.b)

    return myInnerFunction()
```

Nested functions inside method functions are themselves method functions with an implicit `this` parameter. Meaning these nested method functions can access object fields inside them.

## `if` statement

You can declare `if` statements like this:

```python
bool condition

if condition
  # true
else if not condition
  # else if
else
  # false
```

## `while` loop

You can declare a `while` loop like this:

```python
bool condition = true

while condition
  # while loop
```

## `for` loop

You can declare a C-style `for` loop like this:

```python
for int i = 0; i < 10; i += 1
  # for loops
```

You can also declare a `for` each loop like this:

```python
for int number in [1, 2, 3]
  # for each loop
```

The index of the element is stored into an implicit `it` variable.

## `break` statement

You can use `break` to immediately exit a loop:

```python
for int i = 0; i < 10; i += 1
  # Exit when i == 5
  if i == 5
    break
```

## `continue` statement

You can use `continue` to immediately start the next iteration of a loop:

```python
for int i = 0; i < 10; i += 1
  # Skip iteration when i == 5
  if i == 5
    continue
```


