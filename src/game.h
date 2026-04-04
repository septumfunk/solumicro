#ifndef GAME_H
#define GAME_H

#include "solus/val.h"
#include <solus/vm.h>
#include <raylib.h>

typedef struct {
    solu_state *s;
    solu_val manifest;
    sf_vec2 resolution, camera;
    solu_i64 scale;
    sf_str title, room;
    uint32_t id_c;

    bool paused, drawing, gui;
    bool roomchange, open;
    RenderTexture2D screen;
    Color clear_color;

    solu_val ginfo, gptr;
    solu_val objects, rooms;
    solu_val load_cache;
    sf_str room_dir, obj_dir, spr_dir, snd_dir;
    bool err_pause;

    solu_valmap spr_cache, mus_cache;
    solu_val sprite, snd, music;
    solu_f64 last_time;
} sgb_game;

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
