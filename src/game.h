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
    solu_val load_cache;
    sf_str room_dir, obj_dir, spr_dir;

    sgb_sprites sprites;
} sgb_game;

sgb_game *sgb_game_new(void);
int sgb_game_run(void);
void sgb_game_free(sgb_game *game);

int sgb_changeroom(sgb_game *g, char *name);

#define EXPECTED_NAME sgb_spr_ex
#define EXPECTED_O sgb_spritedata
#define EXPECTED_E sf_str
#include <sf/containers/expected.h>
sgb_spr_ex sgb_sprite_data(sgb_game *game, char *name);

#endif // GAME_H
