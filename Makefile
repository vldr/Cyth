OUTPUT = output/cyth
CXX = clang
CXXFLAGS = -MMD -O0 -Wall -Wextra -pedantic -glldb -fsanitize=address
LINKFLAGS =

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
	@$(CXX) $(CXXFLAGS) $(LINKFLAGS) -o $(OUTPUT) $(OBJS)

objs/%.o: src/%.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: build
	./$(OUTPUT)

clean:
	rm -f $(OBJS) $(DEPS)

-include $(DEPS)