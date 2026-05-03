#ifndef ASSET_H
#define ASSET_H

#include <solus/api.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdint.h>

static inline bool smc_is_solc(char *name) {
    size_t len = strlen(name);
    return len >= 5 && memcmp(name + len - 5, ".solc",  5) == 0;
}

typedef struct {
    uint32_t width, height;
} smc_size;
typedef struct {
    int32_t x, y;
} smc_point;

typedef struct {
    uint32_t x, y;
    uint32_t width, height;
    smc_point origin;
} smc_rect;
typedef struct {
    solu_i64 x, y;
    solu_i64 width, height;
    smc_point origin;
} smc_irect;
typedef struct {
    solu_f64 x, y;
    solu_f64 width, height;
} smc_frect;

typedef struct {
    void *g;
    sf_str name;
    SDL_Texture *texture;
    smc_size size;
    smc_rect *frames;
    uint32_t frame_c;
} smc_spritedata;
static inline void smc_spritedata_free(smc_spritedata sprite) {
    sf_str_free(sprite.name);
    SDL_DestroyTexture(sprite.texture);
    if (sprite.frames) free(sprite.frames);
}

typedef struct {
    void *g;
    sf_str name;
    enum {
        SMC_SOUND,
        SMC_MUSIC,
    } tt;
    union {
        Mix_Chunk *sound;
        Mix_Music *music;
    };
    solu_f64 default_volume;
    int channel;
    bool loop;
} smc_sounddata;
static inline void smc_sounddata_free(smc_sounddata sound) {
    sf_str_free(sound.name);
    if (sound.tt == SMC_SOUND)
        Mix_FreeChunk(sound.sound);
    else Mix_FreeMusic(sound.music);
}

typedef struct smc_sprites smc_sprites;
void _smc_sprites_cleanup(smc_sprites *);
#define MAP_NAME smc_sprites
#define MAP_K sf_str
#define MAP_V smc_spritedata
#define EQUAL_FN(s1, s2) (sf_str_eq(s1, s2))
#define HASH_FN(s) (sf_str_hash(s))
#define CLEANUP_FN _smc_sprites_cleanup
#define KCLEANUP sf_str_free
#include <sf/containers/map.h>

#define EXPECTED_NAME smc_spr_ex
#define EXPECTED_O smc_spritedata
#define EXPECTED_E sf_str
#include <sf/containers/expected.h>
smc_spr_ex smc_open_sprite(SDL_Renderer *ren, solu_state *state, sf_str spr_dir, char *name);

#define EXPECTED_NAME smc_snd_ex
#define EXPECTED_O smc_sounddata
#define EXPECTED_E sf_str
#include <sf/containers/expected.h>
smc_snd_ex smc_open_sound(solu_state *state, sf_str snd_dir, char *name);
smc_snd_ex smc_open_music(solu_state *state, sf_str snd_dir, char *name);

solu_val smc_manifest_load(solu_state *state);

#endif // ASSET_H
