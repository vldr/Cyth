OUTPUT = output/cyth
CXX = clang
CXXFLAGS = -Ithird_party/includes -MMD -O0 -Wall -Wextra -pedantic
LINKFLAGS = -Wl,-rpath,third_party/libs -Lthird_party/libs -lbinaryen -fsanitize=address -fsanitize=undefined

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%, objs/%, $(patsubst %.c, %.o, $(SRCS)))
DEPS = $(patsubst %.o, %.d, $(OBJS))

ifeq ($(strip $(shell which $(CXX))),)
$(error $(CXX) is not installed)
endif
 
all: objs/ output/ build 

objs/:
	mkdir objs
 
output/:
	mkdir output

build: $(OBJS)
	$(CXX) -o $(OUTPUT) $(OBJS) $(LINKFLAGS)

objs/%.o: src/%.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: build
	./$(OUTPUT)

clean:
	rm -f $(OBJS) $(DEPS)

-include $(DEPS)