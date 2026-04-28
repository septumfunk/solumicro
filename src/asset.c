#include "asset.h"
#include "platforms/platforms.h"
#include "sf/str.h"
#include "solus/bytecode.h"
#include "solus/val.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_render.h>
#include <limits.h>
#include <stdlib.h>

const char SGB_DEFAULT_CONFIG[] = "{\n\
    title = 'solumicro'\n\
    window = {\n\
        width = 160\n\
        height = 144\n\
        scale = 3\n\
    }\n\
    path = {\n\
        objects = 'scripts'\n\
        rooms = 'rooms'\n\
        sprites = 'sprites'\n\
        sounds = 'sounds'\n\
    }\n\
}";

sgb_spr_ex sgb_open_sprite(SDL_Renderer *ren, solu_state *s, sf_str spr_dir, char *name) {
    char *fpath = sf_str_fmt("%s/%s", spr_dir.c_str, name).c_str;
    char *rpath = solu_findfile(s, fpath);
    free(fpath);
    if (!rpath)
        return sgb_spr_ex_err(sf_str_fmt("Failed to load sprite '%s'", name));
    fpath = rpath;

    solu_compile_ex comp_ex = solu_cfile(s, fpath);
    free(fpath);
    if (!comp_ex.is_ok)
        return sgb_spr_ex_err(sf_str_fmt(
            "Failed to compile sprite '%s': %s",
            name, solu_err_string(comp_ex.err.tt)
        ));

    solu_call_ex call_ex = solu_call(s, &comp_ex.ok, NULL, 0);
    solu_fproto_free(&comp_ex.ok);
    if (!call_ex.is_ok) {
        sf_str e = sf_str_fmt("Panic during sprite '%s': %s", name, call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt));
        if (call_ex.err.panic)
            free(call_ex.err.panic);
        return sgb_spr_ex_err(e);
    }
    if (!solu_isdtype(call_ex.ok, SOLU_DOBJ))
        return sgb_spr_ex_err(sf_str_fmt("Expected sprite '%s' to return obj, got %s", name, solu_typename(call_ex.ok).c_str));

    solu_val frames = solu_dobj_strget(call_ex.ok.dyn, "frames");
    solu_val _auto = solu_dobj_strget(call_ex.ok.dyn, "auto");
    solu_dobj *f_obj = frames.dyn;
    solu_dobj *a_obj = _auto.dyn;
    if ((!solu_isdtype(frames, SOLU_DOBJ) || f_obj->array.count == 0) &&
         !solu_isdtype(_auto, SOLU_DOBJ))
        return sgb_spr_ex_err(sf_str_fmt("Expected sprite '%s' to contain frames:obj[>0] or auto:obj", name));

    solu_val source = solu_dobj_strget(call_ex.ok.dyn, "source");
    if (!solu_isdtype(source, SOLU_DSTR))
        return sgb_spr_ex_err(sf_str_fmt("Expected sprite '%s' to contain source:str", name));

    sf_str join = sf_str_join(spr_dir, sf_lit("/"));
    sf_str j2 = sf_str_join(join, sf_ref(source.dyn));
    sf_str_free(join);
    join = j2;

    char *spath = solu_realpath(join.c_str);
    sf_str_free(join);
    if (!spath)
        return sgb_spr_ex_err(sf_str_fmt("Failed to find sprite '%s' source sprite '%s'", name, source.dyn));

    SDL_Texture *texture = IMG_LoadTexture(ren, spath);
    free(spath);
    if (!texture) return sgb_spr_ex_err(sf_str_fmt(
        "Failed to load sprite '%s' source sprite '%s': %s",
        name,
        (char *)source.dyn,
        IMG_GetError()
    ));
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);

    uint32_t format;
    int access, w, h;
    if (SDL_QueryTexture(texture, &format, &access, &w, &h) != 0) {
        SDL_DestroyTexture(texture);
        return sgb_spr_ex_err(sf_str_fmt(
            "Failed to query sprite '%s': %s",
            name,
            SDL_GetError()
        ));
    }

    sgb_spritedata spr = {
        .texture = texture,
        .size = {(uint32_t)w, (uint32_t)h},
    };

    if (solu_isdtype(frames, SOLU_DOBJ)) {
        spr.frames = malloc(f_obj->array.count * sizeof(sgb_rect));
        spr.frame_c = f_obj->array.count;
        uint32_t o = 0;
        for (uint32_t i = 0; i < f_obj->array.count; ++i) {
            solu_val frame = f_obj->array.data[i];
            if (!solu_isdtype(frame, SOLU_DOBJ)) {
                free(spr.frames);
                return sgb_spr_ex_err(sf_str_fmt("Expected sprite '%s' frames[%u]:obj[4:i64]", name, i));
            }
            solu_dobj *obj = frame.dyn;
            if (obj->array.count < 4 ||
                obj->array.data[0].tt != SOLU_TI64 ||
                obj->array.data[1].tt != SOLU_TI64 ||
                obj->array.data[2].tt != SOLU_TI64 ||
                obj->array.data[3].tt != SOLU_TI64) {
                free(spr.frames);
                return sgb_spr_ex_err(sf_str_fmt("Expected sprite '%s' frames[%u]:obj[4:i64]", name, i));
            }
            solu_val origin = solu_dobj_strget(obj, "origin");
            sgb_point og = {0, 0};
            if (solu_isdtype(origin, SOLU_DOBJ)) {
                solu_dobj *oo = origin.dyn;
                if (oo->array.count < 2 || oo->array.data[0].tt != SOLU_TI64 || oo->array.data[1].tt != SOLU_TI64) {
                    free(spr.frames);
                    return sgb_spr_ex_err(sf_str_fmt("Expected sprite '%s' frames[%u]:obj:origin[2:i64]", name, i));
                }
                og = (sgb_point){(int32_t)oo->array.data[0].i64, (int32_t)oo->array.data[1].i64};
            }

            solu_val repeat = solu_dobj_strget(obj, "repeat");
            repeat = repeat.tt != SOLU_TI64 ? (solu_val){SOLU_TI64, .i64=(repeat.tt == SOLU_TF64 ? (solu_i64)repeat.f64 : 0)} : repeat;
            if (repeat.i64 > 0)
                spr.frames = realloc(spr.frames, (spr.frame_c += (uint32_t)repeat.i64) * sizeof(sgb_rect));
            uint32_t emit = 1u + (uint32_t)repeat.i64;
            for (uint32_t j = 0; j < emit; ++j) {
                spr.frames[o++] = (sgb_rect){
                    (uint32_t)obj->array.data[0].i64,
                    (uint32_t)obj->array.data[1].i64,
                    (uint32_t)obj->array.data[2].i64,
                    (uint32_t)obj->array.data[3].i64,
                    og
                };
            }
        }
    } else {
        solu_val rect = solu_dobj_strget(a_obj, "rect");
        f_obj = rect.dyn;
        if (!solu_isdtype(rect, SOLU_DOBJ) || f_obj->array.count < 2 ||
            f_obj->array.data[0].tt != SOLU_TI64 || f_obj->array.data[1].tt != SOLU_TI64)
            return sgb_spr_ex_err(sf_str_fmt("Expected sprite '%s' auto.rect[2:i64]", name));

        sgb_point origin = {0, 0};
        solu_val og = solu_dobj_strget(a_obj, "origin");
        solu_dobj *o_obj = og.dyn;
        if (solu_isdtype(og, SOLU_DOBJ) && o_obj->array.count >= 2 &&
            o_obj->array.data[0].tt == SOLU_TI64 && o_obj->array.data[1].tt == SOLU_TI64) {
            origin = (sgb_point){
                (int)(min(max(o_obj->array.data[0].i64, INT_MIN), INT_MAX)),
                (int)(min(max(o_obj->array.data[1].i64, INT_MIN), INT_MAX))
            };
        }

        int fw = (int)(min(max(f_obj->array.data[0].i64, INT_MIN), INT_MAX));
        int fh = (int)(min(max(f_obj->array.data[1].i64, INT_MIN), INT_MAX));
        if (fw <= 0 || w % fw)
            return sgb_spr_ex_err(sf_str_fmt(
                "Sprite '%s' auto.frame_size[0]: width %d does not fit in %d",
                name, fw, w
            ));
        if (fh <= 0 || h % fh)
            return sgb_spr_ex_err(sf_str_fmt(
                "Sprite '%s' auto.frame_size[1]: height %d does not fit in %d",
                name, fh, h
            ));

        int col = w / fw;
        int row = h / fh;

        solu_val exclude = solu_dobj_strget(a_obj, "exclude");
        if (exclude.tt == SOLU_TI64 && (exclude.i64 > col || exclude.i64 < 0))
            return sgb_spr_ex_err(sf_str_fmt( "Sprite '%s' auto.exclude: exclude must be 0<=e<=%d", col));
        int exc = exclude.tt == SOLU_TI64 ? (int)exclude.i64 : 0;

        solu_val _repeat = solu_dobj_strget(a_obj, "repeat");
        int repeat = 1 + (_repeat.tt == SOLU_TI64 ? max(0, (int)_repeat.i64) : 0);

        spr.frames = calloc((size_t)((col * row - exc) * repeat), sizeof(sgb_rect));
        spr.frame_c = (uint32_t)((col * row - exc) * repeat);
        solu_i64 o = 0;
        for (int y = 0; y < row; ++y) {
            for (int x = 0; x < col; ++x) {
                if (y == row - 1 && x >= col - exc) break;
                for (solu_i64 i = 0; i < repeat; ++i) {
                    spr.frames[o++] = (sgb_rect){
                        (uint32_t)(x * fw),
                        (uint32_t)(y * fh),
                        (uint32_t)fw,
                        (uint32_t)fh,
                        origin
                    };
                }
            }
        }
    }

    sgb_info("Loaded sprite '%s'.", name);
    spr.name = sf_str_cdup(name);
    return sgb_spr_ex_ok(spr);
}

sgb_snd_ex sgb_open_sound(solu_state *s, sf_str snd_dir, char *name) {
    char *fpath = sf_str_fmt("%s/%s", snd_dir.c_str, name).c_str;
    char *rpath = solu_findfile(s, fpath);
    free(fpath);
    if (!rpath)
        return sgb_snd_ex_err(sf_str_fmt("Failed to load sound '%s'", name));
    fpath = rpath;
    Mix_Chunk *snd = Mix_LoadWAV(fpath);
    free(fpath);
    if (!snd)
        return sgb_snd_ex_err(sf_str_fmt("Failed to load sound '%s'", name));
    return sgb_snd_ex_ok((sgb_sounddata){
        .tt = SGB_SOUND,
        .sound = snd,
        .name = sf_str_cdup(name),
    });
}

sgb_snd_ex sgb_open_music(solu_state *s, sf_str snd_dir, char *name) {
    char *fpath = sf_str_fmt("%s/%s", snd_dir.c_str, name).c_str;
    char *rpath = solu_findfile(s, fpath);
    free(fpath);
    if (!rpath)
        return sgb_snd_ex_err(sf_str_fmt("Failed to load music '%s'", name));
    fpath = rpath;


    solu_compile_ex comp_ex = solu_cfile(s, fpath);
    free(fpath);
    if (!comp_ex.is_ok)
        return sgb_snd_ex_err(sf_str_fmt(
            "Failed to compile music '%s': %s",
            name, solu_err_string(comp_ex.err.tt)
        ));

    solu_call_ex call_ex = solu_call(s, &comp_ex.ok, NULL, 0);
    solu_fproto_free(&comp_ex.ok);
    if (!call_ex.is_ok) {
        sf_str e = sf_str_fmt("Panic during music '%s': %s", name, call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt));
        if (call_ex.err.panic)
            free(call_ex.err.panic);
        return sgb_snd_ex_err(e);
    }
    if (!solu_isdtype(call_ex.ok, SOLU_DOBJ))
        return sgb_snd_ex_err(sf_str_fmt("Expected music '%s' to return obj, got %s", name, solu_typename(call_ex.ok).c_str));

    solu_val source = solu_dobj_strget(call_ex.ok.dyn, "source");
    if (!solu_isdtype(source, SOLU_DSTR))
        return sgb_snd_ex_err(sf_str_fmt("Expected music '%s' to contain source:str", name));

    sf_str join = sf_str_join(snd_dir, sf_lit("/"));
    sf_str j2 = sf_str_join(join, sf_ref(source.dyn));
    sf_str_free(join);
    join = j2;

    char *spath = solu_realpath(join.c_str);
    sf_str_free(join);
    if (!spath)
        return sgb_snd_ex_err(sf_str_fmt("Failed to find music '%s' source sound '%s'", name, source.dyn));

    Mix_Music *mus = Mix_LoadMUS(spath);
    free(spath);
    if (!mus)
        return sgb_snd_ex_err(sf_str_fmt("Failed to load music '%s'", name));

    solu_val volume = solu_dobj_strget(call_ex.ok.dyn, "volume");
    solu_val loop = solu_dobj_strget(call_ex.ok.dyn, "loop");

    return sgb_snd_ex_ok((sgb_sounddata){
        .tt = SGB_MUSIC,
        .music = mus,
        .name = sf_str_cdup(name),
        .default_volume = volume.tt == SOLU_TF64 ? volume.f64 : 1,
        .loop = loop.tt == SOLU_TBOOL && loop.boolean,
    });
}

solu_val sgb_manifest_load(solu_state *s) {
    solu_val out = SOLU_NIL;

    char *ppath = sgb_locate_manifest();
    if (!ppath)
        return solu_dnerr(s, "Failed to locate manifest.solu");

    solu_compile_ex comp_ex = solu_cfile(s, ppath);
    if (!comp_ex.is_ok) {
        if (sf_file_exists(sf_ref(ppath))) {
            free(ppath);
            return solu_dnerr(s, "Failed to open file");
        }
        FILE *f = fopen(ppath, "w");
        if (!f) {
            free(ppath);
            return solu_dnerr(s, "Failed to create file");
        }
        fwrite(SGB_DEFAULT_CONFIG, 1, sizeof(SGB_DEFAULT_CONFIG) - 1, f);
        fclose(f);
        comp_ex = solu_csrc(s, (char *)SGB_DEFAULT_CONFIG);
    }
    free(ppath);

    solu_call_ex call_ex = solu_call(s, &comp_ex.ok, NULL, 0);
        solu_fproto_free(&comp_ex.ok);
    if (!call_ex.is_ok) {
        sf_str e = sf_str_fmt("Panic during manifest.solu: %s", call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt));
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        if (call_ex.err.panic)
            free(call_ex.err.panic);
        return out;
    }
    if (!solu_isdtype(call_ex.ok, SOLU_DOBJ)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to return obj, got %s", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }

    if (!solu_isdtype(solu_dobj_strget(call_ex.ok.dyn, "title"), SOLU_DSTR)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain title:str", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }

    solu_val window = solu_dobj_strget(call_ex.ok.dyn, "window");
    if (!solu_isdtype(window, SOLU_DOBJ)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain window:obj", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }
    if (solu_dobj_strget(window.dyn, "width").tt != SOLU_TI64) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain window.width:i64", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }
    if (solu_dobj_strget(window.dyn, "height").tt != SOLU_TI64) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain window.height:i64", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }
    if (solu_dobj_strget(window.dyn, "scale").tt != SOLU_TI64) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain window.scale:i64", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }

    solu_val path = solu_dobj_strget(call_ex.ok.dyn, "path");
    if (!solu_isdtype(path, SOLU_DOBJ)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain path:obj", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }

    if (!solu_isdtype(solu_dobj_strget(path.dyn, "rooms"), SOLU_DSTR)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain path.rooms:str", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }
    if (!solu_isdtype(solu_dobj_strget(path.dyn, "objects"), SOLU_DSTR)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain path.objects:str", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }
    if (!solu_isdtype(solu_dobj_strget(path.dyn, "sprites"), SOLU_DSTR)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain path.sprites:str", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }
    if (!solu_isdtype(solu_dobj_strget(path.dyn, "sounds"), SOLU_DSTR)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain path.sounds:str", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }

    solu_dhold(call_ex.ok);
    return call_ex.ok;
}
