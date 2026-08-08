<p align="center">
    <img src='logo.svg?raw=true' height="140px">
</p>

---
A fast and simple, embeddable programming language that targets WebAssembly, x86-64, and ARM64.

- [Try it out](#try-it-out)
- [Motivation](#motivation)
- [Binaries](#binaries)
- [Examples](#examples)
- [Building](#building)
  - [Linux](#linux)
  - [MacOS](#macos)
  - [Windows](#windows)
  - [Web](#web)
- [Contributing](#contributing)
- [Overview](#overview)
- [Embedding Guide](#embedding-guide)

## Try it out

You can try out Cyth in the web playground:
[https://cyth.vldr.org](https://cyth.vldr.org)

## Motivation
Suppose we want to call a native C function from Cyth; for example, to `print` the 12th fibonacci number.

In Cyth, you just use the `print` function:

```jai
int fibonacci(int n)
  if n <= 1
    return n
  else
    return fibonacci(n - 2) + fibonacci(n - 1)

print("Fibonacci = " + fibonacci(12))
```

On the C side, we initialize the Cyth runtime, provide our implementation of `print` and run the program:

```cpp
#include <stdio.h>
#include <cyth.h>

void print(CyString* text) {
  printf("%s\n", text->data);
}

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();
  cyth_load_function(vm, "void print(string text)", (uintptr_t)print);
  cyth_load_file(vm, argv[1]);
  cyth_compile(vm);
  cyth_run(vm);
  cyth_destroy(vm);

  return 0;
}
```

That is it. With just a few lines of code, Cyth can call into C, and C can call back into Cyth.

If you're interested, you can look at some of the [examples](#examples) or read through the [overview](#overview) of the language.

## Binaries

Precompiled binaries are available in [Releases](https://github.com/vldr/Cyth/releases/latest).

## Examples

- [GFX](https://github.com/vldr/CythGFX)  
A graphical example program.

- [CGI](https://github.com/vldr/CythCGI)  
A server-side scripting environment for the Cyth programming language, written in Rust.

## Building

To build Cyth, you will need to have [CMake](https://cmake.org/) and GCC/Clang/MSVC installed. To run the test suite, you will need to have [Node.js](https://nodejs.org/) (v20 or higher) installed.

If you want to build the WASM backend, provide the `-DWASM=1` flag to CMake.

### Linux

Run the following commands from the root directory (in a terminal):

_Release_:  
```bash
mkdir build
cd build
cmake ..
make
```

_Debug_:  
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
make
```

_Manual C compilation_:  
`cc third_party/mir/mir.c third_party/mir/mir-gen.c third_party/bdwgc/extra/gc.c src/jit.c src/checker.c src/environment.c src/main.c src/memory.c src/lexer.c src/map.c src/parser.c -Ithird_party/mir -Ithird_party/bdwgc/include -fsigned-char -O3 -o cyth`

### MacOS

Run the following commands from the root directory (in a terminal):

_Release_:  
```bash
mkdir build
cd build
cmake ..
make
```

_Debug_:  
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
make
```

_Xcode project_:  
```
cmake -S . -B xbuild -G Xcode
```

Then, in the `xbuild` directory, open `cyth.xcodeproj` in Xcode.


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

_Manual C compilation_:  
`cl.exe third_party/mir/mir.c third_party/mir/mir-gen.c third_party/bdwgc/extra/gc.c src/jit.c src/checker.c src/environment.c src/main.c src/memory.c src/lexer.c src/map.c src/parser.c /Ithird_party/mir /Ithird_party/bdwgc/include /Ox /Fecyth`

### Web
For web builds, you will need to have [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) installed.

Run the following commands from the root directory (in a terminal):

_Release_:
```
mkdir embuild
cd embuild
emcmake cmake ..
make
```

_Debug_:  
```
mkdir embuild
cd embuild
cmake -DCMAKE_BUILD_TYPE=Debug .. 
make
``` 

## Contributing

If you would like to contribute to the project or simply provide feedback and ask questions, please email [cyth@vldr.org](mailto:cyth@vldr.org)

## Overview

- [Primitive Types](#primitive-types)
  - [`bool`](#bool)
  - [`char`](#char)
  - [`int`](#int)
  - [`float`](#float)
- [Types](#types)
  - [`string`](#string)
  - [`any`](#any)
  - [Array](#array)
  - [Object](#object)
  - [Function Pointers](#function-pointers)
- [Variables](#variables)
- [Functions](#functions)
- [Generics](#generics)
  - [Functions](#functions-1)
  - [Objects](#objects)
- [Overloading](#overloading)
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
```jai
string myString = "hello world"
```

### `any`
Possible values: `null`, `string`, [Array](#array) or [Object](#object)   
Default value: `null` 
- Casting `any` to the incorrect underlying type will trigger a panic. 

_Example:_
```jai
any myAny = "hello world"
string myString = (string)myAny
```

#### Array
Possible values: `[]` (empty list) or a list with one or more elements.  
Default value: `[]` (empty list)   

- Arrays can be multi-dimensional.
- All arrays are dynamic, meaning they can be resized.

_Example:_

```jai
int[] myArray
myArray.push(1)
myArray.push(2)
myArray.push(3)

string[][] myArray2D = [["I'm", "multidimensional"]]
```

#### Object
Possible values: `null` or a valid pointer (reference).  
Default value: `null`  

_Example:_

```jai
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

Objects in Cyth closely resemble structs rather than traditional classes. The key difference is that they can have **method functions**, which are functions that include an implicit `this` parameter.

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
> typedef float (*LengthFunc)(Vector*);
>
> LengthFunc length = (LengthFunc) cyth_get_function(vm, "Vector.length.float()");
> 
> Vector* vector = (Vector*) cyth_alloc(true, sizeof(Vector));
> vector->x = 1;
> vector->y = 2;
> vector->z = 3;
>
> float len = length(vector);
> if (cyth_error(vm))
>   len = 0.0f;
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
- Function pointers in Cyth are compatible with C function pointers.

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

```jai
int myVariable = 12
float mySecondVariable
```

If you do not initialize a variable, a default value is assigned automatically. For example, `mySecondVariable` will be set to `0.0`.

You can declare variables in the top-level scope of your program which will make them a global variable. 

> You can access global variables from C using `cyth_get_variable`.
>
> For example, to get `myVariable` from C, you would write:
> ```c
> int* myVariable = (int*) cyth_get_variable(vm, "myVariable.int");
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

  int myMethodFunction()
    return this.a + this.b
```

Method functions have an implicit `this` parameter which is a pointer to the object itself. This can be `null` if the method is called on a `null` pointer.

```python
class MyClass
  int a
  int b

  int myMethodFunction()
    int myInnerMethodFunction()
      return 2 * (this.a + this.b)

    return myInnerMethodFunction()
```

Nested functions inside method functions are themselves method functions with an implicit `this` parameter. Meaning these nested method functions can access object fields inside them.

> You can access global functions from C using `cyth_get_function`.
>
> For example, to get `myFunction` from C, you would write:
> ```c
> typedef int (*Func)(int, int);
> Func my_function = (Func) cyth_get_function(vm, "myFunction.int(int, int)");
>
> int sum = my_function(10, 20);
> if (cyth_error(vm))
>   sum = 0;
> ```
> The address returned from `cyth_get_function` will be `NULL` if it was not found, or the signature is incorrect.
> 
> - Make sure you wrap all calls to Cyth functions with `cyth_try_catch` (see `cyth.h` for details).
> - Make sure you call `cyth_run` before calling functions obtained from `cyth_get_function`, otherwise global variables will be uninitialized.
>

## Generics
You can declare generic [functions](#functions-1) and [objects](#objects-1). Generics use duck typing and work similarly to [templates](https://en.wikipedia.org/wiki/Template_(C%2B%2B)), where a generic function or object is only created when it is first used, not when it is declared.

Additionally, generic types must always be explicitly provided. This may change in the future, but the current requirement exists for readability reasons; especially since, in many cases, you may not have access to an LSP when writing Cyth code.

### Functions

You can declare a generic function like this:

```cpp
T myGenericFunction<T>(T a, T b)
  return a + b

int sum = myGenericFunction<int>(10, 20)
```

### Objects

You can declare a generic object like this:

```cpp
class Object<T>
  T myField

  void __init__(T value)
    myField = value

Object<int> myObject = Object<int>(10)
```

## Overloading

You can overload functions based on their parameters, like this:

```c++
int myFunction(int a, int b)
  return a + b

int myFunction(int a, int b, int c)
  return a + b + c

myFunction(10, 20, 30)
myFunction(10, 20)
```

Additionally, you can overload generic functions, like this:

```c++
T myFunction<T>(T a, T b)
  return a + b

T myFunction<T>(T a, T b, T c)
  return a + b + c

myFunction<int>(10, 20, 30)
myFunction<float>(10, 20)
```

## `if` statement

You can declare `if` statements, like this:

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

You can declare a `while` loop, like this:

```python
bool condition = true

while condition
  # while loop
```

## `for` loop

You can declare a C-style `for` loop, like this:

```python
for int i = 0; i < 10; i += 1
  # for loops
```

You can also declare a `for` each loop, like this:

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

## Embedding Guide

- [Calling C functions from Cyth](#calling-c-functions-from-cyth)
- [Calling Cyth functions from C](#calling-cyth-functions-from-c)
- [Sharing data between C and Cyth](#sharing-data-between-c-and-cyth)
  - [Objects](#objects-1)
  - [Arrays](#arrays)
  - [Strings](#strings)

Cyth's whole ethos is that it is an embeddable language. Cyth is designed to be embedded inside C, C++ and Rust applications.
This is done through the C API provided in `cyth.h`. 

This guide will show you how to embed Cyth inside a simple C application. It will show you how to call C functions from Cyth, call Cyth functions from C and how to share data between C and Cyth.
Even though this guide shows C code, the code can be easily adapted to C++ or even Rust (though this will require some extra effort).

### Calling C functions from Cyth

To start, let's create a main function and import `cyth.h`. This header includes all the important functionality required to embed Cyth into your project. The header also includes documentation for each function, so be sure to check it out if you happen to forget something.

Inside the `main` function, the very first thing to do is to create a Cyth VM instance. This VM instance is what you will provide as the first argument to all the `cyth_` functions in the `cyth.h` header. The VM instance internally keeps track of all the source code you provide, the compiled code you run, and much more.

Right before the application closes, we'll also call `cyth_destroy` which cleans up all the resources allocated by the VM instance.

**Side note:** Even though it's called a "VM", that's just convention. Cyth is a JIT compiled language, there is no concept of a virtual machine (aside from the internal intermediate instructions used to get to the final assembly).

```cpp
#include <stdio.h>
#include <cyth.h>

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();

  cyth_destroy(vm);

  return 0;
}
```

Now that we have a VM initialized, the next step is to load some functions. This means that you will be able to call C functions from Cyth.


This can be done using this function:
```c++
int cyth_load_function(CyVM* vm, const char* signature, uintptr_t func)
```
For the first argument, you provide the Cyth VM instance that you previously created. Then you provide a signature and address. 

The signature argument can be a bit tricky. A function signature is just a Cyth function without a body.

For following example, we want to add a `print` function that prints an integer to the terminal. That means this function has to accept an `int` and return nothing, `void`, so the signature would be: `void print(int input)`

As for the last argument, we simply get the address of the function that will be called by Cyth and cast it to a `uintptr_t`. This cast is unfortunate, but officially in C, there is no way to pass function pointers as `void*`.

So, let's add that `print` function:

```cpp
#include <stdio.h>
#include <cyth.h>

void print(int input) {
  printf("%d\n", input);
}

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();
  cyth_load_function(vm, "void print(int input)", (uintptr_t)print);
  cyth_destroy(vm);

  return 0;
}
```

At this point, the `print` function is accessible from Cyth. All that is left to do is load some Cyth code, compile it and run it!

To load Cyth code, you have two functions to choose from:
```c++
int cyth_load_string(CyVM* vm, const char* filename, const char* string)
```

and 

```c++
int cyth_load_file(CyVM* vm, const char* filename)
```

Both of these functions will load some Cyth code, the first one from a string and the last one from a file. The `filename` argument is used when an error occurs to show what file the error refers to.

As the embedder, be aware that load order matters. Load your internal code before the user's code. This way, if a conflict occurs, the error will appear inside the user's code rather than your internal code, which the user may or may not have access to.

Moving on, we will load some code directly from a string that calls the `print` function with the integer `1234`:

```cpp
#include <stdio.h>
#include <cyth.h>

void print(int input) {
  printf("%d\n", input);
}

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();
  cyth_load_function(vm, "void print(int input)", (uintptr_t)print);
  cyth_load_string(vm, "example.cy", "print(1234)");
  cyth_destroy(vm);

  return 0;
}
```

Lastly, we need to compile the Cyth code and run it. That is done using these two functions:

```c++
int cyth_compile(CyVM* vm)
```
This function will transform all the code you previously loaded into machine code.

```c++
int cyth_run(CyVM* vm)
```

This function will run the top-level scope of the program. There is no main function in the traditional sense, anything in the global scope is run top-to-bottom, for every file. 

**Important:** You should *ALWAYS* call `cyth_run`. This is because if a user has defined some global variables, then these global variables won't be initialized until you call `cyth_run`.

So with that said, let's add `cyth_compile` and `cyth_run`:

```cpp
#include <stdio.h>
#include <cyth.h>

void print(int input) {
  printf("%d\n", input);
}

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();
  cyth_load_function(vm, "void print(int input)", (uintptr_t)print);
  cyth_load_string(vm, "example.cy", "print(1234)");
  cyth_compile(vm);
  cyth_run(vm);
  cyth_destroy(vm);

  return 0;
}
```

You can now run the above code and you should see `1234` appear in your terminal.

Something important to mention is that the above code does not check for errors.
Most of the functions mentioned return an `int` which means they will return `1`, if they ran successfully, or `0` if they failed.

When one of these functions fail, an "error callback" is also called. By default, the internal Cyth error callback is used which just prints the error to the terminal. But, you can override the error callback using the `cyth_set_error_callback` function found in `cyth.h`.

### Calling Cyth functions from C

In this next section, we will call a Cyth function from C.

Cyth functions are basically C functions. In Cyth, whenever you call a function, you're really just calling a C function. Under the hood, each Cyth function is just a JIT compiled C function (generated when you call `cyth_compile`).

To call a Cyth function, you first have to get its address using this function:
```c++
uintptr_t cyth_get_function(CyVM* vm, const char* name)
```

The `name` argument is a bit tricky. The format is `<function name>.<type name>`.

For example, look at this Cyth function:

```jai
int sum(int a, int b)
  return a + b
```

The function name is `sum` and the type name is: `int(int, int)`. So the name you provide is: `sum.int(int, int)`

The `cyth_get_function` function returns an address; you can cast this address to a function pointer. This function can also return `NULL` if it fails to find a function with the given `name`.

Once we have the function pointer, we can just call it. 

There is one caveat: in Cyth, functions can panic (crash). When a function obtained from `cyth_get_function` panics, the return value will always be `0`/`NULL` (if the return type is not `void`). You can check whether the function you just ran crashed by calling the `cyth_error` function. The `cyth_error` function will return `1` if the last Cyth function called crashed; otherwise, it will return `0` if it ran successfully.

```cpp
#include <stdio.h>
#include <cyth.h>

void print(CyString* text) {
  printf("%s\n", text->data);
}

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();
  cyth_load_function(vm, "void print(string text)", (uintptr_t)print);
  cyth_load_string(vm, "example.cy", "int sum(int a, int b)\n"
                                     "  return a + b\n");
  cyth_compile(vm);
  cyth_run(vm);

  int (*sum)(int, int) = (int (*)(int, int)) cyth_get_function(vm, "sum.int(int, int)");
  int result = sum(10, 20);

  if (!cyth_error(vm))
    printf("The sum is %d\n", result);

  cyth_destroy(vm);
  return 0;
}
```

You can now run the above code and you should see `The sum is 30` appear in your terminal.

**Important:** You should *ALWAYS* call `cyth_error` after calling a Cyth function to check whether the function ran successfully or not.

**Important:** If a Cyth function panics (crashes), it will *ALWAYS* return `0` or `NULL`. This includes strings and arrays, which are *NEVER* allowed to be `NULL`. So make sure to call `cyth_error` to see whether a given function ran successfully or not.

One last thing to mention is that there exists a `cyth_get_function_unsafe` function. This function is similar to `cyth_get_function`, except that the address it returns is an unsafe variant, meaning that if you call the Cyth function and a runtime panic occurs, then your C application will crash alongside the Cyth application.

For performance reasons, this function is useful if you know that the function you are calling will not panic, or if it does panic, you have already called a safe Cyth function and you are inside a callback or `cyth_run`.

### Sharing data between C and Cyth

Sharing data between C and Cyth is designed to be simple. All primitive types in Cyth are identical to their C counterparts. So, an `int` in Cyth, is an `int` in C.

Objects, arrays and strings are all pointers to C structs; they are called pointer types. So, a `Node` object in Cyth, is a `Node *` in C.

The fundamental rule to remember is: if it's not a primitive type, then it is a pointer type.

| C | Cyth |
| -------- | -------- |
| int   | int |
| float   | float |
| bool   | bool |
| char   | char |
|  |  |
| CyString*   | string |
| CyArray*   | T[] |

#### Objects

Cyth objects (classes) are binary compatible with C structs. This is best illustrated with an example.

Let's create a simple linked list `Node` object in Cyth:

```cpp
class Node
  int value
  Node next

  void __init__(int value, Node next)
    this.value = value
    this.next = next
```

Now, let's pass this `Node` object to our C application through the `printList` function which prints each item in the list:

```cpp
#include <stdio.h>
#include <cyth.h>

typedef struct Node {
  int value;
  struct Node* next;
} Node;

void printList(Node* node) {
  while (node) {
    printf("%d\n", node->value);
  
    node = node->next;
  }
}

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();
  cyth_load_function(vm, "void printList(Node node)", (uintptr_t)printList);
  cyth_load_string(vm, "example.cy", "class Node\n"
                                     "  int value\n"
                                     "  Node next\n"

                                     "  void __init__(int value, Node next)\n"
                                     "    this.value = value\n"
                                     "    this.next = next\n"

                                     "printList(Node(10, Node(20, Node(30, null))))");
  cyth_compile(vm);
  cyth_run(vm);
  cyth_destroy(vm);

  return 0;
}
```

You can now run the above code and you should see `10 20 30` appear in your terminal.

**Important:** The memory layout of the Cyth class *MUST* match the memory layout of the C struct.

**Important:** Make sure to keep Cyth pointers on the stack and not somewhere the garbage collector can't scan, like inside a heap-allocated C array. The GC can't see outside the stack and may collect an object out from under you if it's stored outside the stack.

**Important:** Objects can be `NULL`, so make sure to check them. Strings and arrays are *NEVER* null, so you don't have to check them.

---


Next, let's consider allocating Cyth objects in C and then passing them into Cyth.

To allocate Cyth objects in C, you need to use this function:
```c++
void* cyth_alloc(int atomic, uintptr_t size)
```

The first argument is a bit tricky. Atomic *MUST* be 1, if the object you are allocating does not contain any pointers. If it does contain pointers, it *MUST* be 0. Your object will typically contain pointers if it contains arrays, strings or other objects.

The second argument is the size of the object, you can fetch the size of structs in C using `sizeof`.

The `cyth_alloc` function uses Cyth's garbage collector to allocate memory. As long as the memory pointer is on the stack, the garbage collector will be able to find it. It is able to do so because it scans the stack. Cyth uses [Boehm GC](https://en.wikipedia.org/wiki/Boehm_garbage_collector) under the hood.

So, with this information, let's create a function in our C application that will allocate a 2-item linked list.

```cpp
#include <stdio.h>
#include <cyth.h>

typedef struct Node {
  int value;
  struct Node* next;
} Node;

Node* createList() {
  Node* secondNode = (Node*) cyth_alloc(0, sizeof(Node));
  secondNode->value = 1;
  secondNode->next = NULL;

  Node* node = (Node*) cyth_alloc(0, sizeof(Node));
  node->value = 2;
  node->next = secondNode;

  return node;
}

void printList(Node* node) {
  while (node) {
    printf("%d\n", node->value);
  
    node = node->next;
  }
}

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();
  cyth_load_function(vm, "void printList(Node node)", (uintptr_t)printList);
  cyth_load_function(vm, "Node createList()", (uintptr_t)createList);
  cyth_load_string(vm, "example.cy", "class Node\n"
                                     "  int value\n"
                                     "  Node next\n"
                                     ""
                                     "  void __init__(int value, Node next)\n"
                                     "    this.value = value\n"
                                     "    this.next = next\n"
                                     ""
                                     "printList(createList())");
  cyth_compile(vm);
  cyth_run(vm);
  cyth_destroy(vm);

  return 0;
}
```

You can now run the above code and you should see `1 2` appear in your terminal.

Have a look at the `cyth_alloc` function call where we passed `0` in for the first argument. This was done because the `Node` class is *NOT* atomic, since it contains an object (the `next` field), which is a pointer type.

#### Arrays

In this next section, we will show how to share arrays between C and Cyth.

The `cyth.h` header provides a `CyArray` struct definition which matches the memory layout of arrays in Cyth:

```cpp
typedef struct _CY_ARRAY
{
  int size;
  int capacity;
  void* data;
} CyArray;
```

The `size` field is how many elements are in the array.
The `capacity` field is how many elements are allocated.
Lastly, the `data` field points to the underlying allocated buffer.

Now, for example, let's pass an `int[]` array from Cyth to C:

```cpp
printArray([1,2,3,4,5,6])
```

Then, in our C application, we'll create the `printArray` function which will print each element in the array.

```cpp
#include <stdio.h>
#include <cyth.h>

void printArray(CyArray* array) {
  int* data = array->data;

  for (int i = 0; i < array->size; i++) {
    printf("%d\n", data[i]);
  }
}

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();
  cyth_load_function(vm, "void printArray(int[] array)", (uintptr_t)printArray);
  cyth_load_string(vm, "example.cy", "printArray([1,2,3,4,5,6])");
  cyth_compile(vm);
  cyth_run(vm);
  cyth_destroy(vm);

  return 0;
}
```

You can now run the above code and you should see `1 2 3 4 5 6` appear in your terminal.

**Important:** You don't need to check if `array` is a `NULL` pointer because arrays are *NEVER* null. By default, they are empty. But, the `data` field may be `NULL` if the `size`/`capacity` is zero.

---

Next, let's consider allocating Cyth arrays in C and then passing them into Cyth.

To allocate Cyth arrays in C, you need to use this function:
```c++
void* cyth_alloc(int atomic, uintptr_t size)
```

To allocate a Cyth array, we will need to perform two allocations. One for the `CyArray` struct and another for the underlying `data` field.

So, with this in mind, let's create a `createArray` function which will allocate a 2-item array and pass it into Cyth:

```cpp
#include <stdio.h>
#include <cyth.h>

CyArray* createArray() {
  const int SIZE = 2;

  int* data = cyth_alloc(1, sizeof(int) * SIZE);
  data[0] = 10;
  data[1] = 20;

  CyArray* array = cyth_alloc(0, sizeof(CyArray));
  array->size = SIZE;
  array->capacity = SIZE;
  array->data = data;

  return array;
}

void printArray(CyArray* array) {
  int* data = array->data;

  for (int i = 0; i < array->size; i++) {
    printf("%d\n", data[i]);
  }
}

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();
  cyth_load_function(vm, "int[] createArray()", (uintptr_t)createArray);
  cyth_load_function(vm, "void printArray(int[] array)", (uintptr_t)printArray);
  cyth_load_string(vm, "example.cy", "printArray(createArray())");
  cyth_compile(vm);
  cyth_run(vm);
  cyth_destroy(vm);

  return 0;
}
```

You can now run the above code and you should see `10 20` appear in your terminal.

Have a look at the first `cyth_alloc` function call, where we passed `1` in for the first argument. This was done because `int` is *NOT* a pointer type, it is a primitive type therefore it is atomic.

In contrast, have a look at the second `cyth_alloc` function call, where we passed `0` in for the first argument. This was done because `CyArray` contains a pointer (in its `data` field) therefore it is *NOT* atomic.

#### Strings

In this final section, we will show how to share strings between C and Cyth.

The `cyth.h` header provides a `CyString` struct definition which matches the memory layout of strings in Cyth:

```cpp
typedef struct _CY_STRING
{
  int size;
  char data[];
} CyString;
```

The `size` field is how many characters are in the string.

The `data` field is the character data array. Specifically, it's a flexible array member, meaning the string characters are stored directly within the CyString object rather than in a separate allocation. The character data is contiguous and null-terminated (for C compatibility).

Now, for example, let's pass a `string` from Cyth to C:

```cpp
printString("Hello World")
```

Then, in our C application, we'll create the `printString` function which will print a string to the terminal.

```cpp
#include <stdio.h>
#include <cyth.h>

void printString(CyString* string) {
  printf("%s\n", string->data);
}

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();
  cyth_load_function(vm, "void printString(string array)", (uintptr_t)printString);
  cyth_load_string(vm, "example.cy", "printString(\"Hello World\")");
  cyth_compile(vm);
  cyth_run(vm);
  cyth_destroy(vm);

  return 0;
}
```

You can now run the above code and you should see `Hello World` appear in your terminal.

Have a look at the `printf` function call, we passed `string->data` directly into `printf`. This works because, internally, the `data` array is `NULL` terminated.

**Important:** You don't need to check if `string` is a `NULL` pointer because strings are *NEVER* null. By default, they are empty.

---

Next, let's consider allocating Cyth strings in C and then passing them into Cyth.

To allocate Cyth strings in C, you need to use this function:
```c++
void* cyth_alloc(int atomic, uintptr_t size)
```

Allocating Cyth strings can be tricky, because you need to ensure three things:
1. The size of the allocation must be `sizeof(CyString) + size + 1`.

   Where `size` is the number of characters in the string and the extra `1` is for the NULL terminator which appears at the end of the `data` array.
  
2. You *MUST* place a `NULL` terminator at the end of the `data` array.

3. You *MUST* pass `1` in for the `atomic` argument, since strings are atomic, because they do not contain any pointers.

So, with this in mind, let's create a `createString` function which will allocate a string and pass it into Cyth:

```cpp
#include <stdio.h>
#include <string.h>
#include <cyth.h>

CyString* createString() {
  const char* text = "Hi everyone!";
  int size = strlen(text);

  CyString* string = (CyString*) cyth_alloc(1, sizeof(CyString) + size + 1);
  memcpy(string->data, text, size);
  string->data[size] = '\0';
  string->size = size;

  return string;
}

void printString(CyString* string) {
  printf("%s\n", string->data);
}

int main(int argc, char* argv[]) {
  CyVM* vm = cyth_init();
  cyth_load_function(vm, "string createString()", (uintptr_t)createString);
  cyth_load_function(vm, "void printString(string array)", (uintptr_t)printString);
  cyth_load_string(vm, "example.cy", "printString(createString())");
  cyth_compile(vm);
  cyth_run(vm);
  cyth_destroy(vm);

  return 0;
}
```

You can now run the above code and you should see `Hi everyone!` appear in your terminal.

This concludes the guide. This is only a handful of the interesting things possible with Cyth that can be mentioned. 

For more reference material, have a look at the [examples](#examples) section where Cyth is embedded inside a Rust and C application.