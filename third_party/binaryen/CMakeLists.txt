cmake_minimum_required(VERSION 3.16)
project(binaryen)

add_library(binaryen src/binaryen-c.cpp src/ir/ExpressionAnalyzer.cpp src/ir/ExpressionManipulator.cpp
                     src/ir/debuginfo.cpp src/ir/drop.cpp src/ir/effects.cpp src/ir/eh-utils.cpp 
                     src/ir/export-utils.cpp src/ir/intrinsics.cpp src/ir/lubs.cpp src/ir/memory-utils.cpp
                     src/ir/module-utils.cpp src/ir/names.cpp src/ir/possible-contents.cpp src/ir/properties.cpp
                     src/ir/LocalGraph.cpp src/ir/LocalStructuralDominance.cpp src/ir/ReFinalize.cpp
                     src/ir/return-utils.cpp src/ir/stack-utils.cpp src/ir/table-utils.cpp src/ir/type-updating.cpp
                     src/ir/module-splitting.cpp src/asmjs/asm_v_wasm.cpp src/asmjs/asmangle.cpp
                     src/asmjs/shared-constants.cpp src/cfg/Relooper.cpp src/emscripten-optimizer/optimizer-shared.cpp
                     src/emscripten-optimizer/parser.cpp src/emscripten-optimizer/simple_ast.cpp src/passes/param-utils.cpp
                     
                     src/passes/pass.cpp src/passes/Print.cpp src/passes/PrintCallGraph.cpp src/passes/PrintFeatures.cpp
                     src/passes/PrintFunctionMap.cpp src/passes/CoalesceLocals.cpp src/passes/CodePushing.cpp
                     src/passes/CodeFolding.cpp src/passes/DeadArgumentElimination.cpp src/passes/DeadCodeElimination.cpp 
                     src/passes/Directize.cpp src/passes/DuplicateImportElimination.cpp src/passes/DuplicateFunctionElimination.cpp
                     src/passes/GlobalRefining.cpp src/passes/Heap2Local.cpp src/passes/HeapStoreOptimization.cpp src/passes/Inlining.cpp
                     src/passes/LocalCSE.cpp src/passes/LocalSubtyping.cpp src/passes/MemoryPacking.cpp src/passes/MergeBlocks.cpp
                     src/passes/OnceReduction.cpp src/passes/OptimizeCasts.cpp src/passes/OptimizeInstructions.cpp
                     src/passes/PickLoadSigns.cpp src/passes/Precompute.cpp src/passes/RemoveUnusedBrs.cpp 
                     src/passes/RemoveUnusedModuleElements.cpp src/passes/RemoveUnusedNames.cpp src/passes/ReorderGlobals.cpp 
                     src/passes/ReorderLocals.cpp src/passes/RedundantSetElimination.cpp src/passes/SimplifyGlobals.cpp 
                     src/passes/SimplifyLocals.cpp src/passes/SSAify.cpp src/passes/Vacuum.cpp
                     
                     src/parser/context-decls.cpp src/parser/context-defs.cpp src/parser/lexer.cpp src/parser/parse-1-decls.cpp
                     src/parser/parse-2-typedefs.cpp src/parser/parse-3-implicit-types.cpp src/parser/parse-4-module-types.cpp
                     src/parser/parse-5-defs.cpp src/parser/wat-parser.cpp src/support/suffix_tree.cpp
                     src/support/suffix_tree_node.cpp src/support/archive.cpp src/support/bits.cpp
                     src/support/colors.cpp src/support/command-line.cpp src/support/debug.cpp
                     src/support/dfa_minimization.cpp src/support/file.cpp src/support/istring.cpp
                     src/support/json.cpp src/support/name.cpp src/support/path.cpp src/support/safe_integer.cpp
                     src/support/string.cpp src/support/threads.cpp src/support/utilities.cpp src/wasm/literal.cpp
                     src/wasm/parsing.cpp src/wasm/source-map.cpp src/wasm/wasm.cpp src/wasm/wasm-binary.cpp
                     src/wasm/wasm-debug.cpp src/wasm/wasm-emscripten.cpp src/wasm/wasm-interpreter.cpp
                     src/wasm/wasm-io.cpp src/wasm/wasm-ir-builder.cpp src/wasm/wasm-stack.cpp
                     src/wasm/wasm-stack-opts.cpp src/wasm/wasm-type.cpp src/wasm/wasm-type-shape.cpp
                     src/wasm/wasm-validator.cpp src/analysis/cfg.cpp third_party/llvm-project/Debug.cpp 
                     third_party/llvm-project/ErrorHandling.cpp third_party/llvm-project/NativeFormatting.cpp
                     third_party/llvm-project/raw_ostream.cpp third_party/llvm-project/SmallVector.cpp
                     third_party/llvm-project/Twine.cpp)

if (MSVC)
    target_compile_options(binaryen PRIVATE /W0 /MP)
else()
    target_compile_options(binaryen PRIVATE -Wno-everything -w)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(binaryen PRIVATE -O1)
    else()
        target_compile_options(binaryen PRIVATE -flto=auto)
    endif()
endif()

set_target_properties(binaryen PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)
target_include_directories(binaryen PRIVATE third_party/FP16/include 
                                            third_party/llvm-project/include)
target_include_directories(binaryen PUBLIC src)
