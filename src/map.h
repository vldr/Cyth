#ifndef map_h
#define map_h

#include <memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define map_dec_strkey(name, K, V)                                                                                     \
    struct map_item_##name                                                                                             \
    {                                                                                                                  \
        K key;                                                                                                         \
        V value;                                                                                                       \
        uint32_t hash;                                                                                                 \
    };                                                                                                                 \
                                                                                                                       \
    map_of(name, K, V)

#define map_dec_scalar(name, K, V)                                                                                     \
    struct map_item_##name                                                                                             \
    {                                                                                                                  \
        K key;                                                                                                         \
        V value;                                                                                                       \
    };                                                                                                                 \
                                                                                                                       \
    map_of(name, K, V)

#define map_of(name, K, V)                                                                                             \
    struct map_##name                                                                                                  \
    {                                                                                                                  \
        struct map_item_##name *mem;                                                                                   \
        uint32_t cap;                                                                                                  \
        uint32_t size;                                                                                                 \
        uint32_t load_fac;                                                                                             \
        uint32_t remap;                                                                                                \
        bool used;                                                                                                     \
        bool oom;                                                                                                      \
        bool found;                                                                                                    \
    };                                                                                                                 \
                                                                                                                       \
    typedef struct map_##name map_##name;                                                                              \
                                                                                                                       \
    bool map_init_##name(struct map_##name *map, uint32_t cap, uint32_t load_factor);                                  \
    uint32_t map_size_##name(struct map_##name *map);                                                                  \
    void map_clear_##name(struct map_##name *map);                                                                     \
    V map_put_##name(struct map_##name *map, K key, V val);                                                            \
    V map_get_##name(struct map_##name *map, K key);                                                                   \
    V map_del_##name(struct map_##name *map, K key);

#define map_found(map) ((map)->found)

#define map_oom(map) ((map)->oom)

#define map_foreach(map, K, V)                                                                                         \
    for (int64_t _i = -1, _b = 0; !_b && _i < (map)->cap; _i++)                                                        \
        for ((V) = (map)->mem[_i].value, (K) = (map)->mem[_i].key, _b = 1;                                             \
             _b && ((_i == -1 && (map)->used) || (K) != 0) ? 1 : (_b = 0); _b = 0)

#define map_foreach_key(map, K)                                                                                        \
    for (int64_t _i = -1, _b = 0; !_b && _i < (map)->cap; _i++)                                                        \
        for ((K) = (map)->mem[_i].key, _b = 1; _b && ((_i == -1 && (map)->used) || (K) != 0) ? 1 : (_b = 0); _b = 0)

#define map_foreach_value(map, V)                                                                                      \
    for (int64_t _i = -1, _b = 0; !_b && _i < (map)->cap; _i++)                                                        \
        for ((V) = (map)->mem[_i].value, _b = 1;                                                                       \
             _b && ((_i == -1 && (map)->used) || (map)->mem[_i].key != 0) ? 1 : (_b = 0); _b = 0)

// clang-format off

map_dec_scalar(int, int, int)
map_dec_scalar(intv, int, void *)
map_dec_scalar(ints, int, const char *)
map_dec_scalar(ll, long long, long long)
map_dec_scalar(llv, long long, void *)
map_dec_scalar(lls, long long, const char *)
map_dec_scalar(32, uint32_t, uint32_t)
map_dec_scalar(64, uint64_t, uint64_t)
map_dec_scalar(64v, uint64_t, void *)
map_dec_scalar(64s, uint64_t, const char *)

map_dec_strkey(str, const char *, const char *)
map_dec_strkey(sv, const char *, void *)
map_dec_strkey(s64, const char *, uint64_t)
map_dec_strkey(sll, const char *, long long)

#endif
