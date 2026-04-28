#ifndef ASSET_H
#define ASSET_H

#include <solus/vm.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdint.h>


static inline bool sgb_is_solc(char *name) {
    size_t len = strlen(name);
    return len >= 5 && memcmp(name + len - 5, ".solc",  5) == 0;
}

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
    solu_i64 x, y;
    solu_i64 width, height;
    sgb_point origin;
} sgb_irect;
typedef struct {
    solu_f64 x, y;
    solu_f64 width, height;
} sgb_frect;

typedef struct {
    void *g;
    sf_str name;
    SDL_Texture *texture;
    sgb_size size;
    sgb_rect *frames;
    uint32_t frame_c;
} sgb_spritedata;
static inline void sgb_spritedata_free(sgb_spritedata sprite) {
    sf_str_free(sprite.name);
    SDL_DestroyTexture(sprite.texture);
    if (sprite.frames) free(sprite.frames);
}

typedef struct {
    void *g;
    sf_str name;
    enum {
        SGB_SOUND,
        SGB_MUSIC,
    } tt;
    union {
        Mix_Chunk *sound;
        Mix_Music *music;
    };
    solu_f64 default_volume;
    int channel;
    bool loop;
} sgb_sounddata;
static inline void sgb_sounddata_free(sgb_sounddata sound) {
    sf_str_free(sound.name);
    if (sound.tt == SGB_SOUND)
        Mix_FreeChunk(sound.sound);
    else Mix_FreeMusic(sound.music);
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

#define EXPECTED_NAME sgb_spr_ex
#define EXPECTED_O sgb_spritedata
#define EXPECTED_E sf_str
#include <sf/containers/expected.h>
sgb_spr_ex sgb_open_sprite(SDL_Renderer *ren, solu_state *state, sf_str spr_dir, char *name);

#define EXPECTED_NAME sgb_snd_ex
#define EXPECTED_O sgb_sounddata
#define EXPECTED_E sf_str
#include <sf/containers/expected.h>
sgb_snd_ex sgb_open_sound(solu_state *state, sf_str snd_dir, char *name);
sgb_snd_ex sgb_open_music(solu_state *state, sf_str snd_dir, char *name);

solu_val sgb_manifest_load(solu_state *state);

#endif // ASSET_H
