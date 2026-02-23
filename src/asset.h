#ifndef ASSET_H
#define ASSET_H

#include <sf/str.h>
#include <raylib.h>
#include <stdint.h>

typedef struct {
    uint32_t width, height;
} sgb_size;
typedef struct {
    int32_t x, y;
} sgb_point;
typedef struct {
    uint32_t x, y;
    uint32_t width, height;
    sgb_point origin;
} sgb_rect;

typedef struct {
    Texture2D texture;
    sgb_size size;
    sgb_rect *frames;
    uint32_t frame_c;
} sgb_spritedata;

static inline void sgb_spritedata_free(sgb_spritedata sprite) {
    if (sprite.frames) free(sprite.frames);
}
typedef struct sgb_sprites sgb_sprites;
void _sgb_sprites_cleanup(sgb_sprites *);

#define MAP_NAME sgb_sprites
#define MAP_K sf_str
#define MAP_V sgb_spritedata
#define EQUAL_FN(s1, s2) (sf_str_eq(s1, s2))
#define HASH_FN(s) (sf_str_hash(s))
#define CLEANUP_FN _sgb_sprites_cleanup
#define KCLEANUP sf_str_free
#include <sf/containers/map.h>

#endif // ASSET_H
