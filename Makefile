OUTPUT = output/cyth
CXX = clang
CXXFLAGS = -Iincludes -MMD -O0 -g -Wall -Wextra -pedantic -fsanitize=address -fsanitize=undefined
LINKFLAGS = -Wl,-rpath,libs -Llibs -lbinaryen

EM_OUTPUT = output/cyth.js
EM_CXX = emcc
EM_CXXFLAGS = -Iincludes -MMD -flto -fno-rtti -O3 -Wall -Wextra -pedantic
EM_LINKFLAGS = --closure 1 -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 -sALLOW_TABLE_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=addFunction,UTF8ToString \
               -sEXPORTED_FUNCTIONS=_free,_memory_alloc,_run,_set_error_callback,_set_result_callback -Llibs -lbinaryen

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%, objects/%, $(patsubst %.c, %.o, $(SRCS)))
DEPS = $(patsubst %.o, %.d, $(OBJS))

EM_OBJS = $(patsubst src/%, objects/%, $(patsubst %.c, %.wasm.o, $(SRCS)))
EM_DEPS = $(patsubst %.wasm.o, %.wasm.d, $(EM_OBJS))

ifeq ($(strip $(shell which $(CXX))),)
$(error $(CXX) is not installed)
endif

ifeq ($(strip $(shell which $(EM_CXX))),)
$(error $(EM_CXX) is not installed)
endif
 
all: objects/ output/ desktop 

objects/:
	mkdir objects
 
output/:
	mkdir output

includes/:
	unzip -q .dependencies "includes/*" -d .
	
libs/:
	unzip -q .dependencies "libs/*" -d .

objects/%.o: src/%.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

objects/%.wasm.o: src/%.c
	$(EM_CXX) $(EM_CXXFLAGS) -c $< -o $@

desktop: includes/ libs/ $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(OUTPUT) $(OBJS) $(LINKFLAGS)

web: includes/ libs/ $(EM_OBJS)
	$(EM_CXX) $(EM_CXXFLAGS) -o $(EM_OUTPUT) $(EM_OBJS) $(EM_LINKFLAGS)
	@cp output/cyth.js editor/cyth.js
	@cp output/cyth.wasm editor/cyth.wasm

test:
	bun test --bail tests/

clean:
	rm -f $(OBJS) $(DEPS) $(EM_OBJS) $(EM_DEPS)

-include $(DEPS)
-include $(EM_DEPS)