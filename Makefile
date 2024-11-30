OUTPUT = output/cyth
CC = clang
CXX = clang++
CCFLAGS = -Ithird_party/includes  -MMD -O0 -Wall -Wextra -pedantic
LINKFLAGS = -Lthird_party/libs -lpthread -lbinaryen

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%, objs/%, $(patsubst %.c, %.o, $(SRCS)))
DEPS = $(patsubst %.o, %.d, $(OBJS))

ifeq ($(strip $(shell which $(CXX))),)
$(error $(CXX) is not installed)
endif

ifeq ($(strip $(shell which $(CC))),)
$(error $(CC) is not installed)
endif
 
all: objs/ output/ build 

objs/:
	mkdir objs
 
output/:
	mkdir output

build: $(OBJS)
	$(CXX) -o $(OUTPUT) $(OBJS) $(LINKFLAGS)

objs/%.o: src/%.c
	$(CC) $(CCFLAGS) -c $< -o $@

run: build
	./$(OUTPUT)

clean:
	rm -f $(OBJS) $(DEPS)

-include $(DEPS)