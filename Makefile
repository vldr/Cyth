OUTPUT = output/cyth
CXX = clang
CXXFLAGS = -Ithird_party/binaryen/src -MMD -O0 -g -Wall -Wextra -pedantic -fsanitize=address -fsanitize=undefined
LINKFLAGS = -Wl,-rpath,libs -Lthird_party/binaryen/output -lbinaryen

EM_OUTPUT = output/cyth.js
EM_CXX = emcc
EM_CXXFLAGS = -Ithird_party/binaryen/src -MMD -flto -fno-rtti -O3 -Wall -Wextra -pedantic
EM_LINKFLAGS = --closure 1 -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 -sALLOW_TABLE_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=addFunction,UTF8ToString \
               -sEXPORTED_FUNCTIONS=_free,_memory_alloc,_run,_set_error_callback,_set_result_callback -Lthird_party/binaryen/output -lbinaryen

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%, objects/%, $(patsubst %.c, %.o, $(SRCS)))
DEPS = $(patsubst %.o, %.d, $(OBJS))

EM_OBJS = $(patsubst src/%, objects/%, $(patsubst %.c, %.wasm.o, $(SRCS)))
EM_DEPS = $(patsubst %.wasm.o, %.wasm.d, $(EM_OBJS))
 
all: objects/ output/ desktop 

objects/:
	mkdir objects
 
output/:
	mkdir output

third_party/binaryen/output/libbinaryen.a:
	$(MAKE) web -C third_party/binaryen
	
third_party/binaryen/output/libbinaryen.so:
	$(MAKE) -C third_party/binaryen

objects/%.o: src/%.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

objects/%.wasm.o: src/%.c
	$(EM_CXX) $(EM_CXXFLAGS) -c $< -o $@

desktop: third_party/binaryen/output/libbinaryen.so $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(OUTPUT) $(OBJS) $(LINKFLAGS)

web: third_party/binaryen/output/libbinaryen.a $(EM_OBJS)
	$(EM_CXX) $(EM_CXXFLAGS) -o $(EM_OUTPUT) $(EM_OBJS) $(EM_LINKFLAGS)
	@cp output/cyth.js editor/cyth.js
	@cp output/cyth.wasm editor/cyth.wasm

test:
	bun test --bail tests/

clean:
	rm -f $(OBJS) $(DEPS) $(EM_OBJS) $(EM_DEPS)

cleanall: clean
	$(MAKE) clean -C third_party/binaryen

-include $(DEPS)
-include $(EM_DEPS)