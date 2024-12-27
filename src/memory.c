#include "memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BUCKET_SIZE 16384

Memory memory = { 0 };

struct _BUCKET
{
  Bucket* next;
  size_t count;
  size_t capacity;
  uintptr_t data[];
};

static Bucket* new_bucket(size_t capacity)
{
  Bucket* bucket = (Bucket*)calloc(capacity, sizeof(Bucket) + sizeof(uintptr_t));
  bucket->next = NULL;
  bucket->count = 0;
  bucket->capacity = capacity;

  return bucket;
}

static void free_bucket(Bucket* bucket)
{
  free(bucket);
}

void* memory_alloc(Memory* memory, size_t size_bytes)
{
  size_t size = (size_bytes + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

  if (memory->end == NULL)
  {
    size_t capacity = DEFAULT_BUCKET_SIZE;
    if (capacity < size)
      capacity = size;

    memory->end = new_bucket(capacity);
    memory->begin = memory->end;
  }

  while (memory->end->count + size > memory->end->capacity && memory->end->next != NULL)
  {
    memory->end = memory->end->next;
  }

  if (memory->end->count + size > memory->end->capacity)
  {
    size_t capacity = DEFAULT_BUCKET_SIZE;
    if (capacity < size)
      capacity = size;

    memory->end->next = new_bucket(capacity);
    memory->end = memory->end->next;
  }

  void* result = &memory->end->data[memory->end->count];
  memory->end->count += size;

  return result;
}

void* memory_realloc(Memory* memory, void* old_pointer, size_t old_size, size_t new_size)
{
  if (new_size <= old_size)
    return old_pointer;

  void* new_pointer = memory_alloc(memory, new_size);
  char* new_pointer_char = (char*)new_pointer;
  char* old_pointer_char = (char*)old_pointer;

  for (size_t i = 0; i < old_size; ++i)
  {
    new_pointer_char[i] = old_pointer_char[i];
  }

  return new_pointer;
}

char* memory_strdup(Memory* a, const char* cstr)
{
  size_t length = strlen(cstr);
  char* dup = (char*)memory_alloc(a, length + 1);
  memcpy(dup, cstr, length);
  dup[length] = '\0';

  return dup;
}

char* memory_strldup(Memory* a, const char* str, size_t length)
{
  if (!str)
    return "";

  char* dup = (char*)memory_alloc(a, length + 1);
  memcpy(dup, str, length);
  dup[length] = '\0';

  return dup;
}

void* memory_memdup(Memory* memory, void* data, size_t size)
{
  return memcpy(memory_alloc(memory, size), data, size);
}

char* memory_sprintf(Memory* memory, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  int n = vsnprintf(NULL, 0, format, args);
  va_end(args);

  char* result = (char*)memory_alloc(memory, n + 1);
  va_start(args, format);
  vsnprintf(result, n + 1, format, args);
  va_end(args);

  return result;
}

void memory_reset(Memory* memory)
{
  for (Bucket* bucket = memory->begin; bucket != NULL; bucket = bucket->next)
  {
    bucket->count = 0;
  }

  memory->end = memory->begin;
}

void memory_free(Memory* memory)
{
  Bucket* bucket = memory->begin;

  while (bucket)
  {
    Bucket* r0 = bucket;
    bucket = bucket->next;
    free_bucket(r0);
  }

  memory->begin = NULL;
  memory->end = NULL;
}
