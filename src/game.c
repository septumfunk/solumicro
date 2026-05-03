#include "game.h"
#include "asset.h"
#include "api.h"
#include <inttypes.h>
#include "sf/fs.h"
#include "sf/str.h"
#include "solus/bytecode.h"
#include "solus/val.h"
#include "solus/api.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void smc_update_world(smc_collision *c, smc_irect world, uint32_t grid) {
    c->world = world;
    uint32_t width = (uint32_t)world.width / grid;
    uint32_t height = (uint32_t)world.height / grid;
    uint32_t pcount = width * height;
    if (c->partitions) {
        for (uint32_t i = 0; i < c->pcount; ++i)
            smc_partition_free(&c->partitions[i]);
        if (pcount != c->pcount) free(c->partitions);
    }
    if (pcount != c->pcount)
        c->partitions = malloc(pcount * sizeof(smc_partition));
    c->pcount = pcount;
    c->grid = grid;

    for (uint32_t i = 0; i < c->pcount; ++i)
        c->partitions[i] = smc_partition_new();
}

void _smc_sprites_fe(void *ud, sf_str k, smc_spritedata spr) {
    (void)ud;
    sf_str_free(k);
    if (spr.frames) free(spr.frames);
}
void _smc_sprites_cleanup(smc_sprites *spr) {
    smc_sprites_foreach(spr, _smc_sprites_fe, NULL);
}

static inline void smc_pause(smc_game *g, bool toggle) {
    g->paused = toggle;
    solu_dobj_strset(g->ginfo.dyn, "paused", (solu_val){SOLU_TBOOL, .boolean=toggle});
}

bool smc_callmethod(smc_game *g, solu_dobj *obj, char *name) {
    g->ocall = (solu_val){SOLU_TDYN, .dyn=obj};
    solu_val update = solu_dobj_strget(obj, name);
    solu_dhold(update);
    if (solu_isdtype(update, SOLU_DFUN)) {
        solu_call_ex call_ex = solu_call(g->s, update.dyn, NULL, 0);
        if (!call_ex.is_ok) {
            solu_val type = solu_dobj_strget(obj, "type");
            char *trace = solu_trace_print(call_ex.err.trace, 5, 2, 1);
            if (trace) {
                smc_err(
                    "Object %s:%s() error: %s\n" TUI_CLEAR "%s",
                    solu_isdtype(type, SOLU_DSTR) ? (char *)type.dyn : "???", name,
                    call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt),
                    trace
                );
                free(trace);
            } else smc_err(
                TUI_ERR "Object %s:%s() error: %s\n" TUI_CLEAR,
                solu_isdtype(type, SOLU_DSTR) ? (char *)type.dyn : "???", name,
                call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt)
            );

            if (g->err_pause)
                smc_pause(g, true);
        }
        solu_drelease(update);
        g->ocall = SOLU_NIL;
        return true;
    }
    solu_drelease(update);
    g->ocall = SOLU_NIL;
    return false;
}

int smc_changeroom(smc_game *g, char *name) {
    g->camera = (sf_vec2){0, 0};
    solu_val cache = solu_dobj_strget(g->load_cache.dyn, name);
    solu_val room = SOLU_NIL;
    if (solu_isdtype(cache, SOLU_DOBJ)) {
        room = cache;
    } else {
        char *path = sf_str_fmt("%s/%s", g->room_dir.c_str, name).c_str;
        char *rp = solu_findfile(g->s, path);
        smc_info("Path: %s", rp);
        free(path);
        if (!rp) {
            smc_err("Unable to locate room %s\n", name);
            return -1;
        }
        path = rp;

        solu_fproto fp;
        if (smc_is_solc(path)) {
            solu_load_ex load_ex = solu_loadfun(g->s, path);
            if (!load_ex.is_ok) {
                smc_err("Unable to load %s: %s", path, solu_err_string(load_ex.err));
                free(path);
                return -1;
            }
            fp = load_ex.ok;
        } else {
            solu_compile_ex comp_ex = solu_cfile(g->s, path);
            if (!comp_ex.is_ok) {
                char *trace = solu_ctrace_print(path, comp_ex.err, 15, 2, 1);
                smc_err("Unable to load %s\n%s", trace);
                free(path); free(trace);
                return -1;
            }
            fp = comp_ex.ok;
        }

        solu_call_ex call_ex = solu_call(g->s, &fp, NULL, 0);
        solu_fproto_free(&fp);

        if (!call_ex.is_ok) {
            smc_err("Panic during room init: %s", call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt));
            free(path);
            return -1;
        }
        room = call_ex.ok;
        solu_dobj_strset(g->load_cache.dyn, name, room);
        free(path);
    }
    solu_val spawns = solu_dobj_strget(room.dyn, "spawns");
    if (!solu_isdtype(spawns, SOLU_DOBJ)) {
        smc_err("Expected spawns:obj in room", NULL);
        return -1;
    }

    solu_val size = solu_dobj_strget(room.dyn, "size");
    if (!solu_arrptype(size, SOLU_TI64, 2)) {
        smc_err("Expected size[2:i64] in room", NULL);
        return -1;
    }
    solu_val grid = solu_dobj_strget(room.dyn, "grid");
    if (grid.tt != SOLU_TI64) {
        smc_err("Expected grid:i64 in room", NULL);
        return -1;
    }
    g->room_size = (smc_point){
        abs((int32_t)((solu_dobj *)size.dyn)->array.data[0].i64),
        abs((int32_t)((solu_dobj *)size.dyn)->array.data[1].i64)
    };
    g->grid = grid.i64;
    if (g->grid <= 0) {
        smc_err("Grid size must be positive", NULL);
        return -1;
    }
    if (g->room_size.x <= grid.i64 || g->room_size.y <= grid.i64) {
        smc_err("Room size must be at least one grid unit wide", NULL);
        return -1;
    }
    smc_update_world(&g->collision_data, (smc_irect){
        -g->room_size.x / 2,
        -g->room_size.y / 2,
         g->room_size.x,
         g->room_size.y,
         {0, 0}
    }, (uint32_t)g->grid);


    solu_val r_name = solu_dobj_strget(room.dyn, "name");
    if (!solu_isdtype(r_name, SOLU_DSTR)) {
        smc_err("Expected name:str in room", NULL);
        return -1;
    }

    sf_str_free(g->room);
    g->room = sf_str_cdup(name);
    solu_dobj_strset(g->ginfo.dyn, "room", solu_dnstr(g->s, g->room.c_str));
    smc_callmethods(g, g->objects.dyn, "cleanup");
    solu_drelease(g->objects);
    g->objects = solu_dnew(g->s, SOLU_DOBJ);
    solu_setg(g->s, "objects", g->objects);
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

        solu_val obj = smc_object_new(g, g->id_c++, sf_ref(type.dyn));
        if (solu_isdtype(obj, SOLU_DERR)) {
            printf("%s\n", (char *)obj.dyn);
            continue;
        }
        solu_dappend(obj, *v);
        smc_callmethod(g, obj.dyn, "start");
    }
    solu_drelease(spawns);

    solu_val start = solu_dobj_strget(room.dyn, "start");
    if (solu_isdtype(start, SOLU_DFUN)) {
        solu_call_ex call_ex = solu_call(g->s, start.dyn, NULL, 0);
        if (!call_ex.is_ok) {
            solu_val name = solu_dobj_strget(room.dyn, "name");
            smc_err("Room %s:start() error:\n-> %s",
                solu_isdtype(name, SOLU_DSTR) ? (char *)name.dyn : "???",
                call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt)
            );
            return -1;
        }
    }

    return 0;
}

smc_game *smc_game_new(void) {
    smc_game *game = malloc(sizeof(smc_game));
    solu_state *s = solu_state_new();
    solu_usestd(s);
    *game = (smc_game){
        s,
        .ginfo = solu_dnew(s, SOLU_DOBJ),

        .objects = solu_dnew(s, SOLU_DOBJ),
        .rooms = solu_dnew(s, SOLU_DOBJ),
        .spr_cache = solu_valmap_new(),
        .mus_cache = solu_valmap_new(),
        .load_cache = solu_dnew(s, SOLU_DOBJ),
        .clear_color = (SDL_Color){0, 0, 0, 0},
        .last_time = solu_timesec(),
        .collision_data = {.partitions = NULL}
    };
    smc_platform_init(&game->platform);
    // Managed by the runtime
    solu_dhold(game->ginfo);
    solu_dhold(game->objects);
    solu_dhold(game->rooms);
    solu_dhold(game->load_cache);
    solu_setg(s, "rooms", game->rooms);
    solu_setg(s, "objects", game->objects);

    game->manifest = smc_manifest_load(s);
    if (solu_isdtype(game->manifest, SOLU_DERR)) {
        smc_err("%s", (char *)game->manifest.dyn);
        smc_game_free(game);
        return NULL;
    }

    // Manifest Info
    game->title = sf_str_cdup(solu_dobj_strget(game->manifest.dyn, "title").dyn);
    solu_val window = solu_dobj_strget(game->manifest.dyn, "window");
    game->resolution.x = (float)solu_dobj_strget(window.dyn, "width").i64;
    game->resolution.y = (float)solu_dobj_strget(window.dyn, "height").i64;

    solu_val color = solu_dobj_strget(window.dyn, "clear_color");
    solu_dobj *dcolor = color.dyn;
    if (solu_isdtype(color, SOLU_DOBJ) && dcolor->array.count >= 3 &&
        dcolor->array.data[0].tt == SOLU_TI64 &&
        dcolor->array.data[1].tt == SOLU_TI64 &&
        dcolor->array.data[2].tt == SOLU_TI64) {
        game->clear_color = (SDL_Color){
            (uint8_t)max(0, min(dcolor->array.data[0].i64, UINT8_MAX)),
            (uint8_t)max(0, min(dcolor->array.data[1].i64, UINT8_MAX)),
            (uint8_t)max(0, min(dcolor->array.data[2].i64, UINT8_MAX)),
            255,
        };
    }

    solu_val scale = solu_dobj_strget(window.dyn, "scale");
    scale = scale.tt != SOLU_TI64 ? (solu_val){SOLU_TI64, .i64=1} : scale;
    game->scale = scale.i64;
    solu_val err_pause = solu_dobj_strget(game->manifest.dyn, "err_pause");
    game->err_pause = err_pause.tt == SOLU_TBOOL ? err_pause.boolean : false;

    // solus value that stores a pointer to the game
    solu_val gptr = solu_dnusr(s,
        sizeof(smc_game *),
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
    solu_dalloc *da = solu_dheader(game->ginfo);
    da->meta = solu_dnew(s, SOLU_DOBJ);
    solu_dhold(game->ginfo);
    solu_dobj_strset(ginfo, "width", (solu_val){SOLU_TI64, .i64=(solu_i64)game->resolution.x});
    solu_dobj_strset(ginfo, "height", (solu_val){SOLU_TI64, .i64=(solu_i64)game->resolution.y});
    solu_dobj_strset(ginfo, "platform", solu_dnstr(s, smc_platform_string()));
    solu_dobj_strset(ginfo, "paused", (solu_val){SOLU_TBOOL, .boolean = false});
    solu_dobj_strset(ginfo, "quit", solu_wrapcfun(s, smc_quit, 0, &gptr, 1));

    // setter fields
    solu_val set = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(set.dyn, "room", solu_wrapcfun(s, smc_set_room, 1, gcaps, 2));
    solu_dobj_strset(set.dyn, "title", solu_wrapcfun(s, smc_set_title, 1, gcaps, 2));
    solu_dobj_strset(set.dyn, "paused", solu_wrapcfun(s, smc_set_paused, 1, gcaps, 2));
    solu_dobj_strset(set.dyn, "width", solu_wrapcfun(s, smc_noset, 1, gcaps, 2));
    solu_dobj_strset(set.dyn, "height", solu_wrapcfun(s, smc_noset, 1, gcaps, 2));
    solu_dobj_strset(da->meta.dyn, "set", set);

    solu_val _set = solu_wrapcfun(s, smc_set, 3, gcaps, 1);
    solu_dobj_strset(da->meta.dyn, "_set", _set);
    da->metadata[SOLU_META_SET] = _set;

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
            smc_err("Panic during game init: %s", call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt));
            free(game);
            return NULL;
        }
    }

    // paths
    solu_val paths = solu_dobj_strget(game->manifest.dyn, "path");
    solu_val obj_p = solu_dobj_strget(paths.dyn, "objects");
    char *p = smc_data_dir(obj_p.dyn);
    if (!p) { smc_game_free(game); return NULL; }
    game->obj_dir = sf_own(p);
    solu_val room_p = solu_dobj_strget(paths.dyn, "rooms");
    p = smc_data_dir(room_p.dyn);
    if (!p) { smc_game_free(game); return NULL; }
    game->room_dir = sf_own(p);
    solu_val spr_p = solu_dobj_strget(paths.dyn, "sprites");
    p = smc_data_dir(spr_p.dyn);
    if (!p) { smc_game_free(game); return NULL; }
    game->spr_dir = sf_own(p);
    solu_val snd_p = solu_dobj_strget(paths.dyn, "sounds");
    p = smc_data_dir(snd_p.dyn);
    if (!p) { smc_game_free(game); return NULL; }
    game->snd_dir = sf_own(p);

    smc_info(
        "=============== Game Start ===============\n"
        "Loaded manifest for game '%s'.\n"
        "Window Resolution: %dx%d:%lld\n"
        "Paths:\n"
        " - objects:%s\n"
        " - rooms:%s\n"
        " - sprites:%s\n"
        " - sounds:%s\n"
        "==========================================",
        game->title.c_str,
        (int)game->resolution.x,
        (int)game->resolution.y,
        game->scale,
        game->obj_dir.c_str, game->room_dir.c_str, game->spr_dir.c_str, game->snd_dir.c_str
    );

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        smc_err(TUI_ERR "SDL_Init Error: %s\n" TUI_CLEAR, SDL_GetError());
        return NULL;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        smc_err(TUI_ERR "IMG_Init Error: %s\n" TUI_CLEAR, IMG_GetError());
        return NULL;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        smc_err("Mix_OpenAudio Error: %s\n", Mix_GetError());
        return NULL;
    }
    if (!(Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG))) {
        smc_err("Mix_Init Error: %s\n", Mix_GetError());
        return NULL;
    }

    sf_vec2 res = smc_platform_screensize(game->resolution, (float)game->scale);
    game->win = SDL_CreateWindow(
        game->title.c_str,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)res.x, (int)res.y,
        SDL_WINDOW_SHOWN
    );
    game->ren = SDL_CreateRenderer(
        game->win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!game->win || !game->ren) {
        printf(TUI_ERR "Create Error: %s\n" TUI_CLEAR, SDL_GetError());
        smc_game_free(game);
        return NULL;
    }

    game->open = true;
    game->screen = SDL_CreateTexture(
        game->ren,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        (int)game->resolution.x,
        (int)game->resolution.y
    );
    SDL_SetTextureScaleMode(game->screen, SDL_ScaleModeNearest);
    SDL_SetWindowResizable(game->win, SDL_TRUE);
    smc_register(game);

    if (smc_changeroom(game, "start")) {
        sf_str p = sf_str_join(game->room_dir, sf_lit("/start.solu"));
        if (sf_file_exists(p)) {
            sf_str_free(p);
            smc_game_free(game);
            return NULL;
        }
        if (!SMC_READONLY) {
            FILE *f = fopen(p.c_str, "w");
            sf_str_free(p);
            if (!f) {
                smc_err("Expected room 'start'", NULL);
                smc_game_free(game);
                return NULL;
            }
            fwrite(SMC_DEFAULT_ROOM, 1, strlen(SMC_DEFAULT_ROOM), f);
            fclose(f);
            smc_changeroom(game, "start");
        } else {
            smc_err("Cannot create room 'start'", NULL);
            return NULL;
        }
    }

    return game;
}

typedef struct {
    solu_f64 depth;
    solu_val drawable;
} smc_draw;

static inline void smc_update_globals(smc_game *g) {
    solu_val mouse = solu_getg(g->s, "mouse");
    if (!solu_isdtype(mouse, SOLU_DOBJ)) {
        mouse = solu_dnew(g->s, SOLU_DOBJ);
        solu_setg(g->s, "mouse", mouse);
    }
    int x, y, win_w, win_h;
    SDL_GetMouseState(&x, &y);
    SDL_GetWindowSize(g->win, &win_w, &win_h);

    float scaleX = (float)win_w / g->resolution.x;
    float scaleY = (float)win_h / g->resolution.y;
    float scale = scaleX < scaleY ? scaleX : scaleY;

    float gx = ((float)x - (((float)win_w - g->resolution.x * scale) / 2)) / scale;
    float gy = ((float)y - (((float)win_h - g->resolution.y * scale) / 2)) / scale;
    solu_dobj_strset(mouse.dyn, "x", (solu_val){SOLU_TF64, .f64=gx});
    solu_dobj_strset(mouse.dyn, "y", (solu_val){SOLU_TF64, .f64=gy});

    solu_f64 now = solu_timesec();
    solu_dobj_strset(g->ginfo.dyn, "delta_time", (solu_val){SOLU_TF64, .f64 = now - g->last_time});
    g->last_time = now;
}

static inline void smc_update_camera(smc_game *g) {
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

static int smc_game_input(smc_game *g) {
    memset(g->keys_pressed, 0, sizeof(g->keys_pressed));
    memset(g->keys_released, 0, sizeof(g->keys_released));
    memset(g->mouse_pressed, 0, sizeof(g->mouse_pressed));
    memset(g->mouse_released, 0, sizeof(g->mouse_released));

    g->mouse_wheel = 0;
    g->text_input_len = 0;
    g->text_input[0] = '\0';
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            g->open = false;
            return -1;
        }
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            SDL_Scancode sc = e.key.keysym.scancode;
            g->keys_pressed[sc] = true;
        }
        if (e.type == SDL_KEYUP) {
            SDL_Scancode sc = e.key.keysym.scancode;
            g->keys_released[sc] = true;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button < 8)
            g->mouse_pressed[e.button.button] = true;
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button < 8)
            g->mouse_released[e.button.button] = true;
        if (e.type == SDL_MOUSEWHEEL)
            g->mouse_wheel += e.wheel.preciseY;
        if (e.type == SDL_TEXTINPUT) {
            size_t n = strlen(e.text.text);
            size_t room = sizeof(g->text_input) - g->text_input_len - 1;
            if (n > room)
                n = room;
            memcpy(g->text_input + g->text_input_len, e.text.text, n);
            g->text_input_len += n;
            g->text_input[g->text_input_len] = '\0';
        }
    }

    smc_controller_poll(&g->platform);
    return 0;
}

static inline int check_room(smc_game *g, solu_val *snap, uint32_t count) {
    if (g->roomchange) {
        g->roomchange = false;
        g->camera = (sf_vec2){0, 0};
        for (uint32_t i = 0; i < count; ++i)
            solu_drelease(snap[i]);
        free(snap);

        int i;
        if ((i = smc_changeroom(g, g->room.c_str))) {
            smc_err("failed to change to room '%s'", g->room.c_str);
            return i;
        }
        return 1;
    }
    return 0;
}

static int smc_game_update(smc_game *g) {
    solu_dobj *om = g->objects.dyn;
    uint32_t count = om->array.count;
    solu_val *snap = NULL;
    int rc = 0;
    int ir = 0;

    if (count) {
        snap = malloc(sizeof(*snap) * count);
        if (!snap) abort();

        for (uint32_t i = 0; i < count; ++i) {
            snap[i] = om->array.data[i];
            solu_dhold(snap[i]);
        }
    } else {
        smc_update_camera(g);
        return 0;
    };

    for (uint32_t i = 0; i < count; ++i) {
        solu_val obj = snap[i];
        if (!solu_isdtype(obj, SOLU_DOBJ))
            continue;

        if (!g->paused && smc_callmethod(g, obj.dyn, "update")) {
            if (!g->open) {
                rc = -1;
                break;
            }
            if ((ir = check_room(g, snap, count)))
                return ir > 0 ? 0 : -1;
        }

        if (smc_callmethod(g, obj.dyn, "tick")) {
            if (!g->open) {
                rc = -1;
                break;
            }
            if ((ir = check_room(g, snap, count)))
                return ir > 0 ? 0 : -1;
        }
    }

    for (uint32_t i = 0; i < count; ++i)
        solu_drelease(snap[i]);
    free(snap);

    if (rc < 0)
        return -1;

    smc_update_camera(g);
    return 0;
}

static int smc_game_draw(smc_game *g) {
    solu_dobj *om = g->objects.dyn;
    smc_draw *sort = NULL;
    uint32_t sort_c = om->array.count;
    if (sort_c) {
        sort = calloc(sort_c, sizeof(smc_draw));
        if (!sort) return -1;
    }

    for (uint32_t i = 0; i < om->array.count; ++i) {
        solu_val *obj = om->array.data + i;
        if (!solu_isdtype(*obj, SOLU_DOBJ)) continue;
        solu_val dv = solu_dobj_strget(obj->dyn, "depth");
        solu_f64 depth = dv.tt == SOLU_TF64 ? dv.f64 : 0;
        for (smc_draw *cc = sort; cc < sort + om->array.count; ++cc) {
            if (cc->drawable.tt == SOLU_TNIL) {
                *cc = (smc_draw){depth, *obj};
                break;
            }
            if (cc->depth > depth) {
                size_t tail = (size_t)((sort + sort_c) - (cc + 1));
                memmove(cc + 1, cc, tail * sizeof(*cc));
                *cc = (smc_draw){ depth, *obj };
                break;
            }
        }
    }

    SDL_SetRenderTarget(g->ren, g->screen);
        SDL_SetRenderDrawColor(
            g->ren,
            g->clear_color.r,
            g->clear_color.g,
            g->clear_color.b,
            g->clear_color.a
        );
        SDL_RenderClear(g->ren);
        g->drawing = true;
        for (int i = 0; i < 2; ++i) {
            g->gui = i;
            for (smc_draw *draw = sort; draw < sort + sort_c; ++draw) {
                if (!solu_isdtype(draw->drawable, SOLU_DOBJ)) continue;
                if (smc_callmethod(g, draw->drawable.dyn, i ? "draw_gui" : "draw")) {
                    if (!g->open) {
                        free(sort);
                        return -1;
                    }
                    smc_update_camera(g);
                }
            }
        }
        g->drawing = g->gui = false;
    SDL_SetRenderTarget(g->ren, NULL);
    free(sort);
    return 0;
}

int smc_game_run(void) {
    smc_game *g = smc_game_new();
    if (!g) return -1;

    while (g->open) { // SDL2 Loop
        if (smc_game_input(g) < 0)
            goto close;
        smc_update_globals(g);
        if (smc_game_update(g) < 0)
            goto close;
        if (smc_game_draw(g) < 0)
            goto close;

        // Draw screen to window
        int winW, winH;
        SDL_GetRendererOutputSize(g->ren, &winW, &winH);
        float scaleX = (float)winW / g->resolution.x;
        float scaleY = (float)winH / g->resolution.y;
        float scale = scaleX < scaleY ? scaleX : scaleY;
        int dw = (int)(g->resolution.x * scale);
        int dh = (int)(g->resolution.y * scale);

        SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
        SDL_RenderClear(g->ren);
        SDL_RenderCopy(g->ren, g->screen, NULL, &(SDL_Rect){
            (winW - dw) / 2,
            (winH - dh) / 2,
            dw,
            dh
        });
        SDL_RenderPresent(g->ren);
    }
close:
    smc_info("Bye bye!", NULL);
    smc_game_free(g);
    return 0;
}

void smc_game_free(smc_game *game) {
    if (!game) return;
    solu_state_free(game->s);
    sf_str_free(game->title);
    sf_str_free(game->room);
    sf_str_free(game->room_dir);
    sf_str_free(game->obj_dir);
    sf_str_free(game->spr_dir);
    sf_str_free(game->snd_dir);
    solu_valmap_free(&game->spr_cache);
    solu_valmap_free(&game->mus_cache);
    if (game->screen)
        SDL_DestroyTexture(game->screen);
    if (game->ren)
        SDL_DestroyRenderer(game->ren);
    if (game->win) {
        SDL_DestroyWindow(game->win);
        Mix_Quit();
        IMG_Quit();
        SDL_Quit();
    }
    if (game->collision_data.partitions) {
        for (uint32_t i = 0; i < game->collision_data.pcount; ++i)
            smc_partition_free(&game->collision_data.partitions[i]);
        free(game->collision_data.partitions);
    }
    free(game);
}

#ifndef _WIN32
#include <unistd.h>
#endif

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    #if !defined(_WIN32) && !defined(__vita__)
    char *cwd, *f = solu_realpath(argv[0]);
    if (!f || !(cwd = solu_realdir(f)) || chdir(cwd) != 0) {
        perror("Failed to find file");
        return -1;
    }
    free(f); free(cwd);
    #endif
    return smc_game_run();
}


