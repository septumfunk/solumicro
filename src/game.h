#ifndef GAME_H
#define GAME_H

#include <solus/vm.h>
#include <raylib.h>
#include "asset.h"

typedef struct {
    solu_state *s;
    solu_val manifest;
    sf_vec2 resolution, camera;
    solu_i64 scale;
    sf_str title, room;
    uint32_t id_c;

    bool drawing, gui;
    bool roomchange, open;
    RenderTexture2D screen;

    solu_val ginfo, gptr;
    solu_val objects, rooms;
    solu_valmap sprites;
    solu_val load_cache;
    sf_str room_dir, obj_dir, spr_dir;
} sgb_game;

sgb_game *sgb_game_new(void);
int sgb_game_run(void);
void sgb_game_free(sgb_game *game);

int sgb_changeroom(sgb_game *g, char *name);

#endif // GAME_H
