#ifndef array_h
#define array_h

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define sizeof_ptr(a) (/* NOLINTBEGIN */ sizeof(*a) /* NOLINTEND */)

#define array_def(T, name)                                                                         \
  typedef struct                                                                                   \
  {                                                                                                \
    bool oom;                                                                                      \
    unsigned int cap;                                                                              \
    unsigned int size;                                                                             \
    T* elems;                                                                                      \
  } Array##name

#define array_init(a)                                                                              \
  do                                                                                               \
  {                                                                                                \
    memset((a), 0, sizeof(*(a)));                                                                  \
  } while (0)

#define array_add(a, k)                                                                            \
  do                                                                                               \
  {                                                                                                \
    const unsigned int _max = 0xffffffff / sizeof_ptr((a)->elems);                                 \
    unsigned int _cap;                                                                             \
    unsigned int _element_size;                                                                    \
    void* _p;                                                                                      \
                                                                                                   \
    if ((a)->cap == (a)->size)                                                                     \
    {                                                                                              \
      if ((a)->cap > _max / 2)                                                                     \
      {                                                                                            \
        (a)->oom = true;                                                                           \
        break;                                                                                     \
      }                                                                                            \
                                                                                                   \
      _cap = (a)->cap == 0 ? 8 : (a)->cap * 2;                                                     \
      _element_size = sizeof_ptr(((a)->elems));                                                    \
      _p = memory_realloc(&memory, (a)->elems, (a)->cap * _element_size, _cap * _element_size);    \
      if (_p == NULL)                                                                              \
      {                                                                                            \
        (a)->oom = true;                                                                           \
        break;                                                                                     \
      }                                                                                            \
      (a)->cap = _cap;                                                                             \
      (a)->elems = _p;                                                                             \
    }                                                                                              \
    (a)->oom = false;                                                                              \
    (a)->elems[(a)->size++] = k;                                                                   \
  } while (0)

#define array_clear(a)                                                                             \
  do                                                                                               \
  {                                                                                                \
    (a)->size = 0;                                                                                 \
    (a)->oom = false;                                                                              \
  } while (0)

#define array_oom(a) ((a)->oom)
#define array_at(a, i) ((a)->elems[i])
#define array_size(a) ((a)->size)
#define array_del(a, i)                                                                            \
  do                                                                                               \
  {                                                                                                \
    unsigned int idx = (i);                                                                        \
    assert(idx < (a)->size);                                                                       \
                                                                                                   \
    const unsigned int _cnt = (a)->size - (idx)-1;                                                 \
    if (_cnt > 0)                                                                                  \
    {                                                                                              \
      memmove(&((a)->elems[idx]), &((a)->elems[idx + 1]), _cnt * sizeof(*((a)->elems)));           \
    }                                                                                              \
    (a)->size--;                                                                                   \
  } while (0)

#define array_del_unordered(a, i)                                                                  \
  do                                                                                               \
  {                                                                                                \
    unsigned int idx = (i);                                                                        \
    assert(idx < (a)->size);                                                                       \
    (a)->elems[idx] = (a)->elems[(--(a)->size)];                                                   \
  } while (0)

#define array_del_last(a)                                                                          \
  do                                                                                               \
  {                                                                                                \
    assert((a)->size != 0);                                                                        \
    (a)->size--;                                                                                   \
  } while (0)

#define array_sort(a, cmp) (qsort((a)->elems, (a)->size, sizeof(*(a)->elems), cmp))

#define array_last(a) (a)->elems[(a)->size - 1]
#define array_foreach(a, elem)                                                                     \
  for (unsigned int _k = 1, _i = 0; _k && _i != (a)->size; _k = !_k, _i++)                         \
    for ((elem) = (a)->elems[_i]; _k; _k = !_k)

array_def(int, Int);
array_def(unsigned int, Uint);
array_def(long, Long);
array_def(long long, LL);
array_def(unsigned long, Ulong);
array_def(unsigned long long, Ull);
array_def(uint32_t, 32);
array_def(uint64_t, 64);
array_def(double, Double);
array_def(const char*, Str);
array_def(bool, Bool);
array_def(void*, Ptr);

#endif
