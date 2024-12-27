OUTPUT = output/cyth
CXX = clang
CXXFLAGS = -Ithird_party/includes -MMD -O0 -g -Wall -Wextra -pedantic -fsanitize=address -fsanitize=undefined
LINKFLAGS = -Wl,-rpath,third_party/libs -Lthird_party/libs -lbinaryen

EM_OUTPUT = output/cyth.js
EM_CXX = emcc
EM_CXXFLAGS = -Ithird_party/includes -MMD -flto -fno-rtti -O3 -Wall -Wextra -pedantic
EM_LINKFLAGS = --closure 1 -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 -sALLOW_TABLE_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=cwrap,addFunction,UTF8ToString \
               -sEXPORTED_FUNCTIONS=_free,_run,_set_error_callback,_set_result_callback -Lthird_party/libs -lbinaryen

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%, objs/%, $(patsubst %.c, %.o, $(SRCS)))
DEPS = $(patsubst %.o, %.d, $(OBJS))

EM_OBJS = $(patsubst src/%, objs/%, $(patsubst %.c, %.wasm.o, $(SRCS)))
EM_DEPS = $(patsubst %.wasm.o, %.wasm.d, $(EM_OBJS))

ifeq ($(strip $(shell which $(CXX))),)
$(error $(CXX) is not installed)
endif

ifeq ($(strip $(shell which $(EM_CXX))),)
$(error $(EM_CXX) is not installed)
endif
 
all: objs/ output/ desktop 

objs/:
	mkdir objs
 
output/:
	mkdir output

objs/%.o: src/%.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

objs/%.wasm.o: src/%.c
	$(EM_CXX) $(EM_CXXFLAGS) -c $< -o $@

desktop: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(OUTPUT) $(OBJS) $(LINKFLAGS)

web: $(EM_OBJS)
	$(EM_CXX) $(EM_CXXFLAGS) -o $(EM_OUTPUT) $(EM_OBJS) $(EM_LINKFLAGS)
	@cp output/cyth.js editor/cyth.js
	@cp output/cyth.wasm editor/cyth.wasm

test:
	bun test --bail tests/

clean:
	rm -f $(OBJS) $(DEPS) $(EM_OBJS) $(EM_DEPS)

-include $(DEPS)
-include $(EM_DEPS)