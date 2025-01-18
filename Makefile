OUTPUT = output/cyth
CXX = clang
CXXFLAGS = -Ithird_party/binaryen/src -MMD -O0 -g -Wall -Wextra -pedantic -fsanitize=address -fsanitize=undefined
LINKFLAGS = -Wl,-rpath,third_party/binaryen/output -Lthird_party/binaryen/output -lbinaryen

EM_OUTPUT = output/cyth.js
EM_CXX = emcc
EM_CXXFLAGS = -Ithird_party/binaryen/src -MMD -flto -fno-rtti -O3 -Wall -Wextra -pedantic
EM_LINKFLAGS = --closure 1 -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 -sALLOW_TABLE_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=addFunction,UTF8ToString \
               -sEXPORTED_FUNCTIONS=_free,_memory_alloc,_run,_set_error_callback,_set_result_callback -Lthird_party/binaryen/output -lbinaryen

BINARYEN = third_party/binaryen/output/libbinaryen.so
EM_BINARYEN = third_party/binaryen/output/libbinaryen.a

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%, objects/%, $(patsubst %.c, %.o, $(SRCS)))
DEPS = $(patsubst %.o, %.d, $(OBJS))

EM_OBJS = $(patsubst src/%, objects/%, $(patsubst %.c, %.wasm.o, $(SRCS)))
EM_DEPS = $(patsubst %.wasm.o, %.wasm.d, $(EM_OBJS))

all: desktop 

objects:
	mkdir objects
 
output:
	mkdir output

objects/%.o: src/%.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

objects/%.wasm.o: src/%.c
	$(EM_CXX) $(EM_CXXFLAGS) -c $< -o $@

$(BINARYEN):
	$(MAKE) -C third_party/binaryen

$(EM_BINARYEN):
	$(MAKE) web -C third_party/binaryen

.PHONY: desktop
desktop: output objects $(BINARYEN) $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(OUTPUT) $(OBJS) $(LINKFLAGS)

.PHONY: web
web: output objects $(EM_BINARYEN) $(EM_OBJS)
	$(EM_CXX) $(EM_CXXFLAGS) -o $(EM_OUTPUT) $(EM_OBJS) $(EM_LINKFLAGS)
	@cp output/cyth.js editor/cyth.js
	@cp output/cyth.wasm editor/cyth.wasm

.PHONY: test
test:
	bun test --bail test/

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) $(EM_OBJS) $(EM_DEPS)

.PHONY: cleanall
cleanall: clean
	$(MAKE) clean -C third_party/binaryen
	rm -f $(BINARYEN)
	rm -f $(EM_BINARYEN)

-include $(DEPS)
-include $(EM_DEPS)