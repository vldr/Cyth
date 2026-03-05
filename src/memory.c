#include "memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_feature)
#if __has_feature(address_sanitizer) && !defined(__SANITIZE_ADDRESS__)
#define __SANITIZE_ADDRESS__
#endif
#endif

#if defined(__SANITIZE_ADDRESS__)
void __asan_poison_memory_region(void const volatile* addr, size_t size);
void __asan_unpoison_memory_region(void const volatile* addr, size_t size);

#define ASAN_POISON_MEMORY_REGION(addr, size)                                                      \
  do                                                                                               \
  {                                                                                                \
    __asan_unpoison_memory_region((addr), (size));                                                 \
    memset((addr), 0xAB, (size));                                                                  \
    __asan_poison_memory_region((addr), (size));                                                   \
  } while (0)
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) __asan_unpoison_memory_region((addr), (size))
#define ASAN_REDZONE_BYTES() (4 * sizeof(long double))
#else
#define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define ASAN_REDZONE_BYTES() (0)
#endif

#include <stdint.h>
#define DEFAULT_BUCKET_SIZE 16384

typedef struct _BUCKET
{
  struct _BUCKET* next;
  size_t count;
  size_t capacity;
  long double data[];
} Bucket;

static struct
{
  Bucket* begin;
  Bucket* end;
} memory;

static Bucket* new_bucket(size_t capacity)
{
  Bucket* bucket = (Bucket*)malloc(sizeof(Bucket) + sizeof(long double) * capacity);
  bucket->next = NULL;
  bucket->count = 0;
  bucket->capacity = capacity;

  ASAN_POISON_MEMORY_REGION(bucket->data, sizeof(long double) * bucket->capacity);
  return bucket;
}

static void free_bucket(Bucket* bucket)
{
  free(bucket);
}

void* memory_alloc(size_t size_bytes)
{
  size_t size = (size_bytes + ASAN_REDZONE_BYTES() + sizeof(long double) - 1) / sizeof(long double);

  if (memory.end == NULL)
  {
    size_t capacity = DEFAULT_BUCKET_SIZE;
    if (capacity < size)
      capacity = size;

    memory.end = new_bucket(capacity);
    memory.begin = memory.end;
  }

  while (memory.end->count + size > memory.end->capacity && memory.end->next != NULL)
  {
    memory.end = memory.end->next;
  }

  if (memory.end->count + size > memory.end->capacity)
  {
    size_t capacity = DEFAULT_BUCKET_SIZE;
    if (capacity < size)
      capacity = size;

    memory.end->next = new_bucket(capacity);
    memory.end = memory.end->next;
  }

  void* result = &memory.end->data[memory.end->count];
  memory.end->count += size;

  ASAN_UNPOISON_MEMORY_REGION(result, size_bytes);
  return result;
}

void memory_reset(void)
{
  for (Bucket* bucket = memory.begin; bucket != NULL; bucket = bucket->next)
  {
    bucket->count = 0;

    ASAN_POISON_MEMORY_REGION(bucket->data, sizeof(long double) * bucket->capacity);
  }

  memory.end = memory.begin;
}

void memory_free(void)
{
  Bucket* bucket = memory.begin;

  while (bucket)
  {
    Bucket* freed_bucket = bucket;
    bucket = bucket->next;
    free_bucket(freed_bucket);
  }

  memory.begin = NULL;
  memory.end = NULL;
}

void* memory_realloc(void* old_pointer, size_t old_size, size_t new_size)
{
  if (new_size <= old_size)
    return old_pointer;

  void* new_pointer = memory_alloc(new_size);
  char* new_pointer_char = (char*)new_pointer;
  char* old_pointer_char = (char*)old_pointer;

  for (size_t i = 0; i < old_size; ++i)
  {
    new_pointer_char[i] = old_pointer_char[i];
  }

  return new_pointer;
}

char* memory_strdup(const char* cstr)
{
  size_t length = strlen(cstr);
  char* dup = memory_alloc(length + 1);
  memcpy(dup, cstr, length);
  dup[length] = '\0';

  return dup;
}

char* memory_strldup(const char* str, size_t length)
{
  if (!str)
    return "";

  char* dup = memory_alloc(length + 1);
  memcpy(dup, str, length);
  dup[length] = '\0';

  return dup;
}

void* memory_memdup(void* data, size_t size)
{
  return memcpy(memory_alloc(size), data, size);
}

char* memory_sprintf(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  int n = vsnprintf(NULL, 0, format, args);
  va_end(args);

  char* result = (char*)memory_alloc(n + 1);
  va_start(args, format);
  vsnprintf(result, n + 1, format, args);
  va_end(args);

  return result;
}
