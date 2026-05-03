#ifndef GAME_H
#define GAME_H

#include "asset.h"
#include "platforms/platforms.h"
#include "solus/val.h"
#include <solus/api.h>
#include <SDL2/SDL.h>

#define VEC_NAME smc_partition
#define VEC_T struct smc_collider *
#define VSIZE_T uint16_t
#define VSIZE_MAX UINT16_MAX
#include <sf/containers/vec.h>
typedef struct {
    smc_partition *partitions;
    uint32_t pcount;
    smc_irect world;
    uint32_t grid;
} smc_collision;
void smc_update_world(smc_collision *c, smc_irect world, uint32_t grid);

typedef struct {
    solu_state *s;
    solu_val manifest;
    sf_vec2 resolution, camera;
    solu_i64 scale;
    sf_str title;
    uint32_t id_c;

    sf_str room;
    smc_point room_size;
    solu_i64 grid;

    bool paused, drawing, gui;
    bool roomchange, open;
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *screen;
    SDL_Color clear_color;

    solu_val ginfo, gptr;
    solu_val objects, rooms;
    solu_val load_cache;
    sf_str room_dir, obj_dir, spr_dir, snd_dir;
    bool err_pause;

    solu_valmap spr_cache, mus_cache;
    solu_val sprite, snd, music, obj;
    solu_f64 last_time;

    smc_collision collision_data;
    solu_val ocall;

    bool keys_pressed[SDL_NUM_SCANCODES];
    bool keys_released[SDL_NUM_SCANCODES];
    bool mouse_pressed[8];
    bool mouse_released[8];
    double mouse_wheel;

    char text_input[256];
    size_t text_input_len;
    uint32_t pc;

    smc_platformdata platform;
} smc_game;

typedef struct smc_collider {
    smc_game *g;
    uint32_t id;
    smc_frect rect;
    bool seen, enabled;
} smc_collider;

smc_game *smc_game_new(void);
int smc_game_run(void);
void smc_game_free(smc_game *game);

int smc_changeroom(smc_game *g, char *name);

bool smc_callmethod(smc_game *g, solu_dobj *om, char *name);
static inline void smc_callmethods(smc_game *g, solu_dobj *om, char *name) {
    for (solu_val *obj = om->array.data; obj < om->array.data + om->array.count; ++obj) {
        if (solu_isdtype(*obj, SOLU_DOBJ))
            smc_callmethod(g, obj->dyn, name);
    }
}

#endif // GAME_H
