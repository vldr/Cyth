#ifndef array_h
#define array_h

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define array_def(T, name)                                                                                             \
    typedef struct                                                                                                     \
    {                                                                                                                  \
        bool oom;                                                                                                      \
        size_t cap;                                                                                                    \
        size_t size;                                                                                                   \
        T *elems;                                                                                                      \
    } array_##name

#define array_init(a)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        memset((a), 0, sizeof(*(a)));                                                                                  \
    } while (0)

#define array_add(a, k)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        const size_t _max = SIZE_MAX / sizeof(*(a)->elems);                                                            \
        size_t _cap;                                                                                                   \
        size_t _element_size;                                                                                          \
        void *_p;                                                                                                      \
                                                                                                                       \
        if ((a)->cap == (a)->size)                                                                                     \
        {                                                                                                              \
            if ((a)->cap > _max / 2)                                                                                   \
            {                                                                                                          \
                (a)->oom = true;                                                                                       \
                break;                                                                                                 \
            }                                                                                                          \
                                                                                                                       \
            _cap = (a)->cap == 0 ? 8 : (a)->cap * 2;                                                                   \
            _element_size = sizeof(*((a)->elems));                                                                     \
            _p = memory_realloc(&memory, (a)->elems, (a)->cap * _element_size, _cap * _element_size);                  \
            if (_p == NULL)                                                                                            \
            {                                                                                                          \
                (a)->oom = true;                                                                                       \
                break;                                                                                                 \
            }                                                                                                          \
            (a)->cap = _cap;                                                                                           \
            (a)->elems = _p;                                                                                           \
        }                                                                                                              \
        (a)->oom = false;                                                                                              \
        (a)->elems[(a)->size++] = k;                                                                                   \
    } while (0)

#define array_clear(a)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        (a)->size = 0;                                                                                                 \
        (a)->oom = false;                                                                                              \
    } while (0)

#define array_oom(a) ((a)->oom)
#define array_at(a, i) ((a)->elems[i])
#define array_size(a) ((a)->size)
#define array_del(a, i)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        size_t idx = (i);                                                                                              \
        assert(idx < (a)->size);                                                                                       \
                                                                                                                       \
        const size_t _cnt = (a)->size - (idx) - 1;                                                                     \
        if (_cnt > 0)                                                                                                  \
        {                                                                                                              \
            memmove(&((a)->elems[idx]), &((a)->elems[idx + 1]), _cnt * sizeof(*((a)->elems)));                         \
        }                                                                                                              \
        (a)->size--;                                                                                                   \
    } while (0)

#define array_del_unordered(a, i)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        size_t idx = (i);                                                                                              \
        assert(idx < (a)->size);                                                                                       \
        (a)->elems[idx] = (a)->elems[(--(a)->size)];                                                                   \
    } while (0)

#define array_del_last(a)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        assert((a)->size != 0);                                                                                        \
        (a)->size--;                                                                                                   \
    } while (0)

#define array_sort(a, cmp) (qsort((a)->elems, (a)->size, sizeof(*(a)->elems), cmp))

#define array_last(a) (a)->elems[(a)->size - 1]
#define array_foreach(a, elem)                                                                                         \
    for (size_t _k = 1, _i = 0; _k && _i != (a)->size; _k = !_k, _i++)                                                 \
        for ((elem) = (a)->elems[_i]; _k; _k = !_k)

array_def(int, int);
array_def(unsigned int, uint);
array_def(long, long);
array_def(long long, ll);
array_def(unsigned long, ulong);
array_def(unsigned long long, ull);
array_def(uint32_t, 32);
array_def(uint64_t, 64);
array_def(double, double);
array_def(const char *, str);
array_def(void *, ptr);

#endif
