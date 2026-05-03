#include "../api.h"
#include <math.h>

static void smc_spr_delete(void *_spr) {
    smc_spritedata *spr = *(smc_spritedata **)_spr;
    solu_valmap_delete(&((smc_game *)spr->g)->spr_cache, spr->name);
    smc_info("Unloaded sprite '%s'.", spr->name.c_str);
    smc_spritedata_free(*spr);
    free(spr);
}

solu_call_ex smc_load_sprite(solu_state *s) {
    solu_val name = solu_get(s, 0);
    if (!solu_isdtype(name, SOLU_DSTR))
        return solu_err(s, "arg 'name' expected str got %s", solu_typename(name).c_str);

    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    solu_valmap_ex exists = solu_valmap_get(&g->spr_cache, sf_ref(name.dyn));
    if (exists.is_ok)
        return solu_ok(exists.ok);

    smc_spr_ex ex = smc_open_sprite(g->ren, s, g->spr_dir, name.dyn);
    if (!ex.is_ok) {
        if (!ex.is_ok) {
            solu_call_ex res = solu_panic(s, "%s", ex.err.c_str);
            sf_str_free(ex.err);
            return res;
        }
    }
    smc_spritedata *spr = malloc(sizeof(smc_spritedata));
    *spr = ex.ok;
    spr->g = g;

    solu_val info = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(info.dyn, "name", solu_dnstr(s, spr->name.c_str));
    solu_dobj_strset(info.dyn, "width", (solu_val){SOLU_TI64, .i64=spr->size.width});
    solu_dobj_strset(info.dyn, "height", (solu_val){SOLU_TI64, .i64=spr->size.height});
    solu_val frames = solu_dnew(s, SOLU_DOBJ);
    for (uint32_t i = 0; i < spr->frame_c; ++i) {
        solu_val f = solu_dnew(s, SOLU_DOBJ);
        solu_dobj_strset(f.dyn, "x", (solu_val){SOLU_TI64, .i64=spr->frames[i].x});
        solu_dobj_strset(f.dyn, "y", (solu_val){SOLU_TI64, .i64=spr->frames[i].y});
        solu_dobj_strset(f.dyn, "width", (solu_val){SOLU_TI64, .i64=spr->frames[i].width});
        solu_dobj_strset(f.dyn, "height", (solu_val){SOLU_TI64, .i64=spr->frames[i].height});
        solu_valvec_push(&((solu_dobj *)frames.dyn)->array, f);
    }
    solu_dobj_strset(info.dyn, "frames", frames);

    solu_val out = solu_dnusr(s, sizeof(smc_spritedata *), "spr", &spr, smc_spr_delete, NULL);
    solu_dalloc *usr = solu_dheader(out);
    solu_dalloc *infod = solu_dheader(info);
    infod->metadata[SOLU_META_EXTEND] = g->sprite;
    usr->metadata[SOLU_META_EXTEND] = info;

    solu_valmap_set(&g->spr_cache, sf_str_cdup(name.dyn), out);
    return solu_ok(out);
}

solu_call_ex smc_draw_sprite(solu_state *s) {
    solu_val sprite = solu_selfc(s);
    solu_val x = solu_get(s, 1);
    solu_val y = solu_get(s, 2);
    solu_val frame = solu_get(s, 3);
    solu_val rot = solu_get(s, 4);
    solu_val scale = solu_get(s, 5);
    solu_val color = solu_get(s, 6);

    if (!solu_isutype(sprite, sf_lit("spr")))
        return solu_err(s, "arg 'sprite' expected spr got %s", solu_typename(sprite).c_str);
    if (x.tt != SOLU_TI64) {
        if (x.tt == SOLU_TF64) x = (solu_val){SOLU_TI64, .i64=(solu_i64)x.f64};
        else return solu_err(s, "arg 'x' expected i64|f64 got %s", solu_typename(x).c_str);
    }
    if (y.tt != SOLU_TI64){
        if (y.tt == SOLU_TF64) y = (solu_val){SOLU_TI64, .i64=(solu_i64)y.f64};
        else return solu_err(s, "arg 'y' expected i64|f64 got %s", solu_typename(y).c_str);
    }
    if (frame.tt != SOLU_TI64){
        if (frame.tt == SOLU_TF64) frame = (solu_val){SOLU_TI64, .i64=(solu_i64)frame.f64};
        else return solu_err(s, "arg 'frame' expected i64|f64 got %s", solu_typename(frame).c_str);
    }
    if (rot.tt != SOLU_TF64)
        rot = (solu_val){SOLU_TF64, .f64=rot.tt == SOLU_TI64 ? (float)rot.i64 : 0};

    float xscale = 1, yscale = 1;
    solu_dobj *s_obj = scale.dyn;
    if (solu_isdtype(scale, SOLU_DOBJ)) {
        if (s_obj->array.count < 2 || s_obj->array.data[0].tt != SOLU_TF64 || s_obj->array.data[0].tt != SOLU_TF64)
            return solu_err(s, "arg 'scale' expected obj[2:f64]");
        xscale = (float)s_obj->array.data[0].f64;
        yscale = (float)s_obj->array.data[1].f64;
    }

    SDL_Color c = (SDL_Color){255, 255, 255, 255};
    solu_dobj *c_obj = color.dyn;
    if (solu_isdtype(color, SOLU_DOBJ)) {
        if (c_obj->array.count < 4 ||
            c_obj->array.data[0].tt != SOLU_TI64 ||
            c_obj->array.data[1].tt != SOLU_TI64 ||
            c_obj->array.data[2].tt != SOLU_TI64 ||
            c_obj->array.data[2].tt != SOLU_TI64)
            return solu_err(s, "arg 'color' expected obj[4:i64]");
        c = (SDL_Color){
            (uint8_t)min(max(c_obj->array.data[0].i64, 0), UINT8_MAX),
            (uint8_t)min(max(c_obj->array.data[1].i64, 0), UINT8_MAX),
            (uint8_t)min(max(c_obj->array.data[2].i64, 0), UINT8_MAX),
            (uint8_t)min(max(c_obj->array.data[3].i64, 0), UINT8_MAX),
        };
    }

    smc_game *g = *(smc_game **)solu_capturec(s, s->ccall->up_c - 1).dyn;
    if (!g->drawing)
        return solu_panic(s, "Draw call outside of object:draw()");

    smc_spritedata spr = **(smc_spritedata **)sprite.dyn;
    if (frame.i64 > spr.frame_c - 1 || frame.i64 < 0)
        return solu_panic(s, "Sprite '%s' does not contain frame %lld", spr.name.c_str, frame.i64);

    smc_rect source = spr.frames[frame.i64];
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (xscale < 0) flip |= SDL_FLIP_HORIZONTAL;
    if (yscale < 0) flip |= SDL_FLIP_VERTICAL;

    SDL_SetTextureColorMod(spr.texture, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(spr.texture, c.a);
    SDL_SetTextureBlendMode(spr.texture, SDL_BLENDMODE_BLEND);
    SDL_RenderCopyExF(
        g->ren,
        spr.texture,
        &(SDL_Rect){
            (int)source.x,
            (int)source.y,
            (int)source.width,
            (int)source.height
        },
        &(SDL_FRect){
            g->gui ? (float)x.i64 : (float)x.i64 - g->camera.x,
            g->gui ? (float)y.i64 : (float)y.i64 - g->camera.y,
            (float)source.width  * fabsf(xscale),
            (float)source.height * fabsf(yscale)
        },
        (double)rot.f64,
        &(SDL_FPoint){
            (float)source.origin.x * fabsf(xscale),
            (float)source.origin.y * fabsf(yscale)
        },
        flip
    );
    return solu_ok(SOLU_NIL);
}

solu_call_ex smc_draw_rect(solu_state *s) {
    solu_val x = solu_get(s, 0);
    solu_val y = solu_get(s, 1);
    solu_val w = solu_get(s, 2);
    solu_val h = solu_get(s, 3);
    solu_val c = solu_get(s, 4);

    if (x.tt != SOLU_TI64) {
        if (x.tt == SOLU_TF64) x = (solu_val){SOLU_TI64, .i64=(solu_i64)x.f64};
        else return solu_err(s, "arg 'x' expected i64|f64 got %s", solu_typename(x).c_str);
    }
    if (y.tt != SOLU_TI64){
        if (y.tt == SOLU_TF64) y = (solu_val){SOLU_TI64, .i64=(solu_i64)y.f64};
        else return solu_err(s, "arg 'y' expected i64|f64 got %s", solu_typename(y).c_str);
    }
    if (w.tt != SOLU_TI64) {
        if (w.tt == SOLU_TF64) w = (solu_val){SOLU_TI64, .i64=(solu_i64)w.f64};
        else return solu_err(s, "arg 'w' expected i64|f64 got %s", solu_typename(w).c_str);
    }
    if (h.tt != SOLU_TI64){
        if (h.tt == SOLU_TF64) h = (solu_val){SOLU_TI64, .i64=(solu_i64)h.f64};
        else return solu_err(s, "arg 'h' expected i64|f64 got %s", solu_typename(h).c_str);
    }
    if (!solu_isdtype(c, SOLU_DOBJ))
        return solu_err(s, "arg 'c' expected obj (color) got %s", solu_typename(c).c_str);
    solu_dobj *obj = c.dyn;
    if (obj->array.count < 4 ||
        obj->array.data[0].tt != SOLU_TI64 ||
        obj->array.data[1].tt != SOLU_TI64 ||
        obj->array.data[2].tt != SOLU_TI64 ||
        obj->array.data[3].tt != SOLU_TI64)
        return solu_err(s, "arg 'c' expected obj[4:i64]");

    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    if (!g->drawing)
        return solu_panic(s, "Draw call outside of object:draw()");

    SDL_SetRenderDrawBlendMode(g->ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(
        g->ren,
        (uint8_t)obj->array.data[0].i64,
        (uint8_t)obj->array.data[1].i64,
        (uint8_t)obj->array.data[2].i64,
        (uint8_t)obj->array.data[3].i64
    );
    SDL_RenderFillRect(g->ren, &(SDL_Rect){
        g->gui ? (int)x.i64 : (int)(x.i64 - (solu_i64)g->camera.x),
        g->gui ? (int)y.i64 : (int)(y.i64 - (solu_i64)g->camera.y),
        (int)w.i64,
        (int)h.i64
    });
    return solu_ok(SOLU_NIL);
}
