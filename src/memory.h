#ifndef memory_h
#define memory_h

#include <stddef.h>

#define ALLOC(type) ((type*)memory_alloc(sizeof(type)))

void* memory_alloc(size_t size_bytes);
void* memory_realloc(void* old_pointer, size_t old_size, size_t new_size);
char* memory_strdup(const char* cstr);
char* memory_strldup(const char* str, size_t length);
void* memory_memdup(void* data, size_t size);
char* memory_sprintf(const char* format, ...);
void memory_reset(void);
void memory_free(void);

#endif
