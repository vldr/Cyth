#ifndef memory_h
#define memory_h

#include <stddef.h>
#include <stdint.h>

typedef struct _BUCKET Bucket;
typedef struct
{
  Bucket* begin;
  Bucket* end;
} Memory;

extern Memory memory;

void* memory_alloc(Memory* memory, size_t size_bytes);
void* memory_realloc(Memory* memory, void* old_pointer, size_t old_size, size_t new_size);
char* memory_strdup(Memory* memory, const char* cstr);
void* memory_memdup(Memory* memory, void* data, size_t size);
char* memory_sprintf(Memory* memory, const char* format, ...);
void memory_reset(Memory* memory);
void memory_free(Memory* memory);

#endif
