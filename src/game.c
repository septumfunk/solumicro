#include "game.h"
#include "asset.h"
#include "script.h"
#include "sf/containers/buffer.h"
#include "sf/fs.h"
#include "sf/str.h"
#include "solus/bytecode.h"
#include "solus/val.h"
#include "solus/vm.h"
#include <errno.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void _sgb_sprites_fe(void *ud, sf_str k, sgb_spritedata spr) {
    (void)ud;
    sf_str_free(k);
    if (spr.frames) free(spr.frames);
}
void _sgb_sprites_cleanup(sgb_sprites *spr) {
    sgb_sprites_foreach(spr, _sgb_sprites_fe, NULL);
}

static inline bool sgb_callmethod(sgb_game *g, solu_dobj *obj, char *name) {
    solu_val update = solu_dobj_strget(obj, name);
    if (solu_isdtype(update, SOLU_DFUN)) {
        solu_call_ex call_ex = solu_call(g->s, update.dyn, NULL, 0);
        if (!call_ex.is_ok) {
            solu_val id = solu_dobj_strget(obj, "id");
            solu_val type = solu_dobj_strget(obj, "type");
            fprintf(stderr, "Object [%lld]:%s:%s() error:\n-> %s\n",
                id.tt == SOLU_TI64 ? id.i64 : -1,
                solu_isdtype(type, SOLU_DSTR) ? (char *)type.dyn : "???",
                name,
                call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt)
            );
            if (call_ex.err.panic)
                free(call_ex.err.panic);
        }
        return true;
    }
    return false;
}

static void sgb_callmethods(sgb_game *g, solu_dobj *om, char *name) {
    for (solu_val *obj = om->array.data; obj < om->array.data + om->array.count; ++obj) {
        sgb_callmethod(g, obj->dyn, name);
    }
}

int sgb_changeroom(sgb_game *g, char *name) {
    char *path = solu_findfile(g->room_dir.c_str, name);
    if (!path) return -1;

    solu_val cache = solu_dobj_strget(g->load_cache.dyn, name);
    solu_val room = SOLU_NIL;
    if (solu_isdtype(cache, SOLU_DOBJ)) {
        room = cache;
    } else {
        solu_compile_ex comp_ex = solu_cfile(g->s, path);
        if (!comp_ex.is_ok) {
            fprintf(stderr, "Unable to load %s: %s\n", path, solu_err_string(comp_ex.err.tt));
            free(path);
            return -1;
        }
        solu_call_ex call_ex = solu_call(g->s, &comp_ex.ok, NULL, 0);
        if (!call_ex.is_ok) {
            fprintf(stderr, "Panic during room init: %s\n", call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt));
            if (call_ex.err.panic)
                free(call_ex.err.panic);
            free(path);
            return -1;
        }
        room = call_ex.ok;
        solu_dobj_strset(g->load_cache.dyn, path, room);
    }
    solu_val spawns = solu_dobj_strget(room.dyn, "spawns");
    if (!solu_isdtype(spawns, SOLU_DOBJ)) {
        fprintf(stderr, "Expected spawns:obj in room\n");
        free(path);
        return -1;
    }

    solu_val r_name = solu_dobj_strget(room.dyn, "name");
    if (!solu_isdtype(r_name, SOLU_DSTR)) {
        fprintf(stderr, "Expected name:str in room\n");
        free(path);
        return -1;
    }

    sf_str_free(g->room);
    g->room = sf_str_cdup(name);
    solu_dobj_strset(g->ginfo.dyn, "room", solu_dnstr(g->s, g->room.c_str));

    sgb_callmethods(g, g->objects.dyn, "cleanup");
    solu_drelease(g->objects);
    solu_setg(g->s, "objects", (g->objects = solu_dnew(g->s, SOLU_DOBJ)));
    solu_dhold(g->objects);
    g->id_c = 0;

    solu_dobj *obj = spawns.dyn;
    solu_dhold(spawns);
    for (solu_val *v = obj->array.data; v < obj->array.data + obj->array.count; ++v) {
        if (!solu_isdtype(*v, SOLU_DOBJ))
            continue;
        solu_val type = solu_dobj_strget(v->dyn, "type");
        if (!solu_isdtype(type, SOLU_DSTR))
            continue;

        solu_val obj = sgb_object_new(g, g->id_c++, sf_ref(type.dyn));
        if (solu_isdtype(obj, SOLU_DERR)) {
            fprintf(stderr, "Object [%u]:%s:load() error:\n-> %s\n", g->id_c - 1, (char *)type.dyn, (char *)obj.dyn);
            continue;
        }
        type = solu_dobj_strget(v->dyn, "type");
        sgb_callmethod(g, obj.dyn, "start");
        solu_dappend(obj, *v);
    }
    solu_drelease(spawns);

    solu_val start = solu_dobj_strget(room.dyn, "start");
    if (solu_isdtype(start, SOLU_DFUN)) {
        solu_call_ex call_ex = solu_call(g->s, start.dyn, NULL, 0);
        if (!call_ex.is_ok) {
            solu_val name = solu_dobj_strget(room.dyn, "name");
            fprintf(stderr, "Room %s:start() error:\n-> %s\n",
                solu_isdtype(name, SOLU_DSTR) ? (char *)name.dyn : "???",
                call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt)
            );
            if (call_ex.err.panic)
                free(call_ex.err.panic);
            free(path);
            return -1;
        }
    }
    free(path);
    return 0;
}

static inline char *make_dir(char *path) {
    char *p = solu_realpath(path);
    if (!p) {
        int status = mkdir(path, S_IRWXU);
        if (status || !(p = solu_realpath(path))) {
            fprintf(stderr, "Failed to create objects dir: %s\n", strerror(errno));
            return NULL;
        }
    }
    return p;
}

sgb_game *sgb_game_new(void) {
    sgb_game *game = malloc(sizeof(sgb_game));
    solu_state *s = solu_state_new();
    solu_usestd(s);
    *game = (sgb_game){
        s,
        .manifest = sgb_manifest_load(s),
        .open = true,

        .ginfo = solu_dnew(s, SOLU_DOBJ),

        .objects = solu_dnew(s, SOLU_DOBJ),
        .rooms = solu_dnew(s, SOLU_DOBJ),
        .sprites = solu_valmap_new(),
        .load_cache = solu_dnew(s, SOLU_DOBJ),
    };
    if (solu_isdtype(game->manifest, SOLU_DERR)) {
        fprintf(stderr, "%s\n", (char *)game->manifest.dyn);
        sgb_game_free(game);
        return NULL;
    }

    // Managed by the runtime
    solu_dhold(game->objects);
    solu_dhold(game->rooms);
    solu_dhold(game->load_cache);
    solu_setg(s, "rooms", game->rooms);
    solu_setg(s, "objects", game->objects);

    // Manifest Info
    game->title = sf_str_cdup(solu_dobj_strget(game->manifest.dyn, "title").dyn);
    solu_val resolution = solu_dobj_strget(game->manifest.dyn, "resolution");
    game->resolution.x = (float)solu_dobj_strget(resolution.dyn, "width").i64;
    game->resolution.y = (float)solu_dobj_strget(resolution.dyn, "height").i64;
    game->scale = solu_dobj_strget(resolution.dyn, "scale").i64;

    // solus value that stores a pointer to the game
    solu_val gptr = solu_dnusr(s,
        sizeof(sgb_game *),
        "gameptr",
        &game,
        NULL, NULL
    );
    solu_setg(s, "__game", gptr);
    game->gptr = gptr;
    solu_dhold(gptr);

    // 'game' setter
    solu_val gcaps[] = {game->ginfo, gptr};
    solu_dobj *ginfo = game->ginfo.dyn;
    ginfo->meta = solu_dnew(s, SOLU_DOBJ);
    solu_dhold(game->ginfo);
    solu_dobj_strset(ginfo, "width", (solu_val){SOLU_TI64, .i64=(solu_i64)game->resolution.x});
    solu_dobj_strset(ginfo, "height", (solu_val){SOLU_TI64, .i64=(solu_i64)game->resolution.y});
    solu_dobj_strset(ginfo, "quit", solu_wrapcfun(s, sgb_quit, 0, &gptr, 1));

    // setter fields
    solu_val set = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(set.dyn, "room", solu_wrapcfun(s, sgb_set_room, 2, gcaps, 2));
    solu_dobj_strset(set.dyn, "title", solu_wrapcfun(s, sgb_set_title, 2, gcaps, 2));
    solu_dobj_strset(set.dyn, "width", solu_wrapcfun(s, sgb_noset, 2, gcaps, 2));
    solu_dobj_strset(set.dyn, "height", solu_wrapcfun(s, sgb_noset, 2, gcaps, 2));
    solu_dobj_strset(ginfo->meta.dyn, "set", set);

    solu_val _set = solu_wrapcfun(s, sgb_set, 2, gcaps, 1);
    solu_dobj_strset(ginfo->meta.dyn, "_set", _set);
    ginfo->metafuns[SOLU_META_SET] = _set;

    solu_setg(s, "game", game->ginfo);

    // 'camera'
    solu_val camera = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(camera.dyn, "x", (solu_val){SOLU_TF64, .f64 = 0});
    solu_dobj_strset(camera.dyn, "y", (solu_val){SOLU_TF64, .f64 = 0});
    solu_setg(s, "camera", camera);

    // init function (game start)
    solu_val init = solu_dobj_strget(game->manifest.dyn, "init");
    if (solu_isdtype(init, SOLU_DFUN)) {
        solu_call_ex call_ex = solu_call(s, init.dyn, NULL, 0);
        if (!call_ex.is_ok) {
            fprintf(stderr, "Panic during game init: %s\n", call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt));
            if (call_ex.err.panic)
                free(call_ex.err.panic);
            free(game);
            return NULL;
        }
    }

    // paths
    solu_val paths = solu_dobj_strget(game->manifest.dyn, "path");
    solu_val obj_p = solu_dobj_strget(paths.dyn, "objects");
    char *p = make_dir(obj_p.dyn);
    if (!p) { sgb_game_free(game); return NULL; }
    game->obj_dir = sf_own(p);
    solu_val room_p = solu_dobj_strget(paths.dyn, "rooms");
    p = make_dir(room_p.dyn);
    if (!p) { sgb_game_free(game); return NULL; }
    game->room_dir = sf_own(p);
    solu_val spr_p = solu_dobj_strget(paths.dyn, "sprites");
    p = make_dir(spr_p.dyn);
    if (!p) { sgb_game_free(game); return NULL; }
    game->spr_dir = sf_own(p);

    InitWindow(
        (int)(game->resolution.x * (float)game->scale),
        (int)(game->resolution.y * (float)game->scale),
        game->title.c_str
    );
    game->screen = LoadRenderTexture((int)game->resolution.x, (int)game->resolution.y);
    SetTextureFilter(game->screen.texture, TEXTURE_FILTER_POINT);
    SetTargetFPS(60);
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    sgb_register(game);

    if (sgb_changeroom(game, "start")) {
        sf_str p = sf_str_join(game->room_dir, sf_lit("/start.solu"));
        if (sf_file_exists(p)) {
            sf_str_free(p);
            free(game);
            return NULL;
        }
        FILE *f = fopen(p.c_str, "w");
        sf_str_free(p);
        if (!f) {
            fprintf(stderr, "Expected room 'start'\n");
            free(game);
            return NULL;
        }
        fwrite(SGB_DEFAULT_ROOM, 1, strlen(SGB_DEFAULT_ROOM), f);
        fclose(f);
        sgb_changeroom(game, "start");
    }

    return game;
}

typedef struct {
    solu_f64 depth;
    solu_val drawable;
} sgb_draw;

static inline void sgb_update_camera(sgb_game *g) {
    // Update Camera
    solu_val camera = solu_getg(g->s, "camera");
    if (solu_isdtype(camera, SOLU_DOBJ)) {
        solu_val x = solu_dobj_strget(camera.dyn, "x");
        if (x.tt != SOLU_TF64 && x.tt != SOLU_TI64)
            solu_dobj_strset(camera.dyn, "x", (solu_val){SOLU_TF64, .f64=g->camera.x});
        else g->camera.x = (float)(x.tt == SOLU_TI64 ? (float)x.i64 : x.f64);
        solu_val y = solu_dobj_strget(camera.dyn, "y");
        if (y.tt != SOLU_TF64 && y.tt != SOLU_TI64)
            solu_dobj_strset(camera.dyn, "y", (solu_val){SOLU_TF64, .f64=g->camera.y});
        else g->camera.y = (float)(y.tt == SOLU_TI64 ? (float)y.i64 : y.f64);
    } else {
        solu_val camera = solu_dnew(g->s, SOLU_DOBJ);
        solu_dobj_strset(camera.dyn, "x", (solu_val){SOLU_TF64, .f64=g->camera.x});
        solu_dobj_strset(camera.dyn, "y", (solu_val){SOLU_TF64, .f64=g->camera.y});
        solu_setg(g->s, "camera", camera);
    }
}

int sgb_game_run(void) {
    sgb_game *g = sgb_game_new();
    if (!g) return -1;
    solu_dobj *om = g->objects.dyn;

    sgb_draw *sort = NULL;
    uint32_t pc = 0;

    while (!WindowShouldClose()) { // Raylib Loop
        for (solu_val *obj = om->array.data; obj < om->array.data + om->array.count; ++obj) {
            if (!solu_isdtype(*obj, SOLU_DOBJ)) continue;
            if (sgb_callmethod(g, obj->dyn, "update")) {
                if (WindowShouldClose()) goto close;
                if (g->roomchange)
                    break;
            }
        }
        if (g->roomchange) { // Interrupt update if room changes
            g->roomchange = false;
            om = g->objects.dyn;
            if (sort) free(sort);
            sort = NULL;
            continue;
        }
        sgb_update_camera(g);

        if (!sort || pc > om->array.count) {
            if (pc) free(sort);
            sort = calloc((pc = om->array.count > pc ? om->array.count : pc), sizeof(sgb_draw));
        }
        for (solu_val *obj = om->array.data; obj < om->array.data + om->array.count; ++obj) {
            if (!solu_isdtype(*obj, SOLU_DOBJ)) continue;
            solu_val dv = solu_dobj_strget(obj->dyn, "depth");
            solu_f64 depth = dv.tt == SOLU_TF64 ? dv.f64 : 0;
            for (sgb_draw *cc = sort; cc < sort + om->array.count; ++cc) {
                if (cc->drawable.tt == SOLU_TNIL) {
                    *cc = (sgb_draw){depth, *obj};
                    break;
                }
                if (cc->depth > depth) {
                    memcpy(cc + 1, cc, sort + om->array.count - cc);
                    *cc = (sgb_draw){depth, *obj};
                    break;
                }
            }
        }

        BeginTextureMode(g->screen);
            ClearBackground(BLACK);
            g->drawing = true;
            for (int i = 0; i < 2; ++i) {
                g->gui = i;
                for (sgb_draw *draw = sort; draw < sort + om->array.count; ++draw)
                    if (sgb_callmethod(g, draw->drawable.dyn, i ? "draw_gui" : "draw")) {
                        if (WindowShouldClose()) goto close;
                        sgb_update_camera(g);
                    }
            }
            g->drawing = g->gui = false;
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            float scaleX = (float)GetScreenWidth() / g->resolution.x;
            float scaleY = (float)GetScreenHeight() / g->resolution.y;
            float scale = scaleX < scaleY ? scaleX : scaleY;
            float dw = g->resolution.x * scale;
            float dh = g->resolution.y * scale;

            DrawTexturePro(g->screen.texture,
                (Rectangle){ 0, 0, g->resolution.x, -g->resolution.y },
                (Rectangle){
                    ((float)GetScreenWidth() - dw) / 2,
                    ((float)GetScreenHeight() - dh) / 2,
                    dw, dh,
                },
                (Vector2){0,0},
                0.0f,
                WHITE
            );
        EndDrawing();
    }
close:
    if (pc) free(sort);

    sgb_game_free(g);
    return 0;
}

void sgb_game_free(sgb_game *game) {
    solu_state_free(game->s);
    sf_str_free(game->title);
    sf_str_free(game->room);
    sf_str_free(game->room_dir);
    sf_str_free(game->obj_dir);
    sf_str_free(game->spr_dir);
    solu_valmap_free(&game->sprites);
    if (game->open) CloseWindow();
}

int main(void) {
    return sgb_game_run();
}


