#ifndef GAME_H
#define GAME_H

#include "asset.h"
#include "platforms/platforms.h"
#include "solus/val.h"
#include <solus/vm.h>
#include <SDL2/SDL.h>

#define VEC_NAME sgb_partition
#define VEC_T struct sgb_collider *
#define VSIZE_T uint16_t
#define VSIZE_MAX UINT16_MAX
#include <sf/containers/vec.h>
typedef struct {
    sgb_partition *partitions;
    uint32_t pcount;
    sgb_irect world;
    uint32_t grid;
} sgb_collision;
void sgb_update_world(sgb_collision *c, sgb_irect world, uint32_t grid);

typedef struct {
    solu_state *s;
    solu_val manifest;
    sf_vec2 resolution, camera;
    solu_i64 scale;
    sf_str title;
    uint32_t id_c;

    sf_str room;
    sgb_point room_size;
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

    sgb_collision collision_data;
    solu_val ocall;

    bool keys_pressed[SDL_NUM_SCANCODES];
    bool keys_released[SDL_NUM_SCANCODES];
    bool mouse_pressed[8];
    bool mouse_released[8];
    double mouse_wheel;

    char text_input[256];
    size_t text_input_len;
    uint32_t pc;

    sgb_platformdata platform;
} sgb_game;

typedef struct sgb_collider {
    sgb_game *g;
    uint32_t id;
    sgb_frect rect;
    bool seen, enabled;
} sgb_collider;

sgb_game *sgb_game_new(void);
int sgb_game_run(void);
void sgb_game_free(sgb_game *game);

int sgb_changeroom(sgb_game *g, char *name);

bool sgb_callmethod(sgb_game *g, solu_dobj *om, char *name);
static inline void sgb_callmethods(sgb_game *g, solu_dobj *om, char *name) {
    for (solu_val *obj = om->array.data; obj < om->array.data + om->array.count; ++obj) {
        if (solu_isdtype(*obj, SOLU_DOBJ))
            sgb_callmethod(g, obj->dyn, name);
    }
}

#endif // GAME_H
