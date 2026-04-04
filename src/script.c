#include "script.h"
#include "asset.h"
#include "game.h"
#include "log.h"
#include "sf/str.h"
#include "solus/bytecode.h"
#include "solus/val.h"
#include "solus/vm.h"
#include <math.h>
#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>

const char SGB_DEFAULT_ROOM[] = "{ \n\
    name = 'Room'\n\
    spawns = {\n\
        # 'foo',\n\
    }\n\
}";

solu_val sgb_object_new(sgb_game *g, solu_i64 id, sf_str path) {
    solu_val out = SOLU_NIL;
    char *rp = solu_findfile(g->obj_dir.c_str, path.c_str);
    if (!rp) {
        sf_str e = sf_str_fmt("Unable to locate %s\n", path.c_str);
        out = solu_dnerr(g->s, e.c_str);
        sf_str_free(e);
        return out;
    }

    solu_compile_ex comp_ex = solu_cfile(g->s, rp);
    if (!comp_ex.is_ok) {
        sf_str e = sf_str_fmt(
            TUI_ERR "Object %s:load() error:\n[" TUI_CLEAR "%s:%d:%d" TUI_ERR "]\n-> %s\n\n" TUI_CLEAR,
            path.c_str, rp,
            comp_ex.err.line, comp_ex.err.column,
            solu_err_string(comp_ex.err.tt)
        );
        free(rp);
        out = solu_dnerr(g->s, e.c_str);
        sf_str_free(e);
        return out;
    }

    solu_call_ex call_ex = solu_call(g->s, &comp_ex.ok, NULL, 0);
    if (!call_ex.is_ok) {
        sf_str e;
        if (g->s->ecall->dbg && g->s->ecall->file_name.len > 0)
            e = sf_str_fmt(
                TUI_ERR "Object %s:load() error:\n[" TUI_CLEAR "%s:%d:%d" TUI_ERR "]\n-> %s\n\n" TUI_CLEAR,
                path.c_str, rp,
                SOLU_DBG_LINE(g->s->ecall->dbg[call_ex.err.pc]), SOLU_DBG_COL(g->s->ecall->dbg[call_ex.err.pc]),
                call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt)
            );
        else e = sf_str_fmt(
            TUI_ERR "Object %s:load() error:\n[" TUI_CLEAR "%s" TUI_ERR "]\n-> %s\n\n" TUI_CLEAR,
            path.c_str, rp,
            call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt)
        );
        free(rp);
        solu_fproto_free(&comp_ex.ok);
        out = solu_dnerr(g->s, e.c_str);
        sf_str_free(e);
        if (call_ex.err.panic)
            free(call_ex.err.panic);
        return out;
    }
    free(rp);
    solu_fproto_free(&comp_ex.ok);
    if (!solu_isdtype(call_ex.ok, SOLU_DOBJ)) {
        sf_str e = sf_str_fmt("Expected gameobject to return obj, got %s\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(g->s, e.c_str);
        sf_str_free(e);
        return out;
    }
    out = call_ex.ok;

    solu_val idv = {SOLU_TI64, .i64=id};
    solu_dobj_strset(out.dyn, "id", idv);
    solu_dobj_strset(out.dyn, "depth", (solu_val){SOLU_TF64, .f64=0});
    solu_dobj_strset(out.dyn, "type", solu_dnstr(g->s, path.c_str));
    solu_dobj_set(g->s, g->objects.dyn, idv, out);

    // Member Fun
    solu_dobj_strset(out.dyn, "delete", solu_wrapmfun(g->s, sgb_delete, 1, &g->gptr, 1));

    return out;
}

void sgb_register(sgb_game *g) {
    solu_val load = solu_dnew(g->s, SOLU_DOBJ);
    solu_dobj_strset(load.dyn, "sprite", solu_wrapcfun(g->s, sgb_load_sprite, 1, &g->gptr, 1));
    solu_dobj_strset(load.dyn, "sound", solu_wrapcfun(g->s, sgb_load_sound, 1, &g->gptr, 1));
    solu_dobj_strset(load.dyn, "music", solu_wrapcfun(g->s, sgb_load_music, 1, &g->gptr, 1));
    solu_dobj_strset(load.dyn, "object", solu_wrapcfun(g->s, sgb_load_object, 2, &g->gptr, 1));

    solu_val draw = solu_dnew(g->s, SOLU_DOBJ);
    solu_dobj_strset(draw.dyn, "sprite", solu_wrapcfun(g->s, sgb_draw_sprite, 7, &g->gptr, 1));
    solu_dobj_strset(draw.dyn, "rect", solu_wrapcfun(g->s, sgb_draw_rect, 5, &g->gptr, 1));

    solu_val key = sgb_keys(g->s);
    solu_dobj_strset(key.dyn, "held", solu_wrapcfun(g->s, sgb_key_held, 1, NULL, 0));
    solu_dobj_strset(key.dyn, "pressed", solu_wrapcfun(g->s, sgb_key_pressed, 1, NULL, 0));
    solu_dobj_strset(key.dyn, "released", solu_wrapcfun(g->s, sgb_key_released, 1, NULL, 0));
    solu_dobj_strset(key.dyn, "string", solu_wrapcfun(g->s, sgb_key_string, 0, NULL, 0));

    solu_val mouse = sgb_mouse(g->s);
    solu_dobj_strset(mouse.dyn, "held", solu_wrapcfun(g->s, sgb_mouse_held, 1, NULL, 0));
    solu_dobj_strset(mouse.dyn, "pressed", solu_wrapcfun(g->s, sgb_mouse_pressed, 1, NULL, 0));
    solu_dobj_strset(mouse.dyn, "released", solu_wrapcfun(g->s, sgb_mouse_released, 1, NULL, 0));
    solu_dobj_strset(mouse.dyn, "wheel", solu_wrapcfun(g->s, sgb_mouse_wheel, 0, NULL, 0));

    g->sprite = solu_dnew(g->s, SOLU_DOBJ);
    solu_dhold(g->sprite);
    solu_dobj_strset(g->sprite.dyn, "draw", solu_wrapmfun(g->s, sgb_draw_sprite, 7, &g->gptr, 1));

    g->snd = solu_dnew(g->s, SOLU_DOBJ);
    solu_dhold(g->snd);
    solu_dobj_strset(g->snd.dyn, "play", solu_wrapmfun(g->s, sgb_snd_play, 0, &g->gptr, 1));
    solu_dobj_strset(g->snd.dyn, "is_playing", solu_wrapmfun(g->s, sgb_snd_is_playing, 0, &g->gptr, 1));
    solu_dobj_strset(g->snd.dyn, "stop", solu_wrapmfun(g->s, sgb_snd_stop, 0, &g->gptr, 1));

    // Globals
    solu_setg(g->s, "delete", solu_wrapcfun(g->s, sgb_delete, 1, &g->gptr, 1));

    solu_setg(g->s, "load", load);
    solu_setg(g->s, "draw", draw);
    solu_setg(g->s, "key", key);
    solu_setg(g->s, "mouse", mouse);
}

solu_call_ex sgb_quit(solu_state *s) {
    (*(sgb_game **)solu_capturec(s, 0).dyn)->open = false;
    CloseWindow();
    return solu_ok(SOLU_NIL);
}

solu_call_ex sgb_delete(solu_state *s) {
    solu_val self = solu_selfc(s);
    if (!solu_isdtype(self, SOLU_DOBJ))
        return solu_panic("arg 'self' expected obj, found %s", solu_typename(self).c_str);
    solu_val id = solu_dobj_strget(self.dyn, "id");
    if (id.tt != SOLU_TI64)
        return solu_panic("self.id expected i64, found %s", solu_typename(self).c_str);
    sgb_game *g = *(sgb_game **)solu_capturec(s, 1).dyn;
    solu_dobj *d = g->objects.dyn;
    if (id.i64 < 0 || id.i64 > d->array.count - 1 || id.i64 > UINT32_MAX)
        return solu_panic("self.id is invalid");
    sgb_callmethod(g, self.dyn, "cleanup");
    solu_valvec_set(&((solu_dobj *)g->objects.dyn)->array, (uint32_t)id.i64, SOLU_NIL);
    return solu_ok(SOLU_NIL);
}

solu_call_ex sgb_set(solu_state *s) {
    solu_val key = solu_get(s, 0);
    solu_val val = solu_get(s, 1);
    if (!solu_isdtype(key, SOLU_DSTR))
        return solu_panic("arg 'key' expected str got %s", solu_typename(key).c_str);

    solu_val cap = solu_capturec(s, 0);
    solu_dobj *set;
    solu_val f;
    if (solu_isdtype(cap, SOLU_DOBJ)) {
        solu_dobj *obj = cap.dyn;
        solu_dobj *m_obj = obj->meta.dyn;
        if (!obj || !m_obj) return solu_panic("Game Corrupt");
        set = solu_dobj_strget(m_obj, "set").dyn;
        if (!set) return solu_panic("Game Corrupt");
        f = solu_dobj_strget(set, key.dyn);
    } else {
        set = solu_dobj_strget(solu_uheader(cap)->metafuns[SOLU_META_EXTEND].dyn, "set").dyn;
        if (!set) return solu_panic("Game Corrupt");
        f = solu_dobj_strget(set, key.dyn);
        solu_fproto *fp = f.dyn;
        fp->upvals[0].value = cap;
    }

    if (!solu_isdtype(f, SOLU_DFUN))
        return solu_panic("key '%s' not found", key.dyn);
    return solu_call(s, f.dyn, (solu_val[]){val}, 1);
}

solu_call_ex sgb_noset(solu_state *_s) {
    (void)_s;
    return solu_panic("key is readonly");
}

solu_call_ex sgb_set_room(solu_state *s) {
    sgb_game *g = *(sgb_game **)solu_capturec(s, 1).dyn;
    solu_val val = solu_get(s, 0);
    if (!solu_isdtype(val, SOLU_DSTR))
        return solu_err(s, "setter 'room' expected str got %s", solu_typename(val).c_str);

    if (sf_str_eq(g->room, sf_ref(val.dyn)))
        return solu_ok(val);
    if (sgb_changeroom(g, val.dyn))
        return solu_err(s, "failed to change to room '%s'", val.dyn);
    solu_val v = solu_capturec(s, 0); (void)v;
    solu_dobj_strset(solu_capturec(s, 0).dyn, "room", solu_dnstr(s, g->room.c_str));
    g->roomchange = true;

    return solu_ok(val);
}

solu_call_ex sgb_set_title(solu_state *s) {
    sgb_game *g = *(sgb_game **)solu_capturec(s, 1).dyn;
    solu_val val = solu_get(s, 0);
    if (!solu_isdtype(val, SOLU_DSTR))
        return solu_err(s, "setter 'title' expected str got %s", solu_typename(val).c_str);

    if (g->title.c_str && sf_str_eq(g->title, sf_ref(val.dyn)))
        return solu_ok(val);
    SetWindowTitle(val.dyn);
    sf_str_free(g->title);
    g->title = sf_str_cdup(val.dyn);
    solu_dobj_strset(g->ginfo.dyn, "title", solu_dnstr(s, g->title.c_str));

    return solu_ok(val);
}

solu_call_ex sgb_set_paused(solu_state *s) {
    sgb_game *g = *(sgb_game **)solu_capturec(s, 1).dyn;
    solu_val val = solu_get(s, 0);
    if (val.tt != SOLU_TBOOL)
        return solu_err(s, "setter 'paused' expected bool got %s", solu_typename(val).c_str);

    g->paused = val.boolean;
    solu_dobj_strset(g->ginfo.dyn, "paused", val);
    sgb_info("%s", val.boolean ? "Game paused." : "Game unpaused.");
    return solu_ok(val);
}

static void sgb_spr_delete(void *_spr) {
    sgb_spritedata *spr = *(sgb_spritedata **)_spr;
    solu_valmap_delete(&((sgb_game *)spr->g)->spr_cache, spr->name);
    sgb_info("Unloaded sprite '%s'.", spr->name.c_str);
    sgb_spritedata_free(*spr);
}
static void sgb_sfx_delete(void *_sfx) {
    sgb_sounddata *sfx = *(sgb_sounddata **)_sfx;
    if (sfx->tt == SGB_MUSIC)
        solu_valmap_delete(&((sgb_game *)sfx->g)->mus_cache, sfx->name);
    sgb_info("Unloaded %s '%s'.", sfx->tt == SGB_MUSIC ? "music" : "sound", sfx->name.c_str);
}

static solu_call_ex sgb_gwrap(solu_state *s) {
    return solu_ok(solu_dobj_get(s, solu_capturec(s, 0).dyn, solu_get(s, 0)));
}

solu_call_ex sgb_load_sprite(solu_state *s) {
    solu_val name = solu_get(s, 0);
    if (!solu_isdtype(name, SOLU_DSTR))
        return solu_err(s, "arg 'name' expected str got %s", solu_typename(name).c_str);

    sgb_game *g = *(sgb_game **)solu_capturec(s, 0).dyn;
    solu_valmap_ex exists = solu_valmap_get(&g->spr_cache, sf_ref(name.dyn));
    if (exists.is_ok)
        return solu_ok(exists.ok);

    sgb_spr_ex ex = sgb_open_sprite(s, g->spr_dir, name.dyn);
    if (!ex.is_ok) {
        if (!ex.is_ok) {
            solu_call_ex res = solu_panic("%s", ex.err.c_str);
            sf_str_free(ex.err);
            return res;
        }
    }
    sgb_spritedata *spr = malloc(sizeof(sgb_spritedata));
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

    solu_val out = solu_dnusr(s, sizeof(sgb_spritedata *), "spr", &spr, sgb_spr_delete, NULL);
    solu_usrwrap *usr = solu_uheader(out);
    solu_dobj *infod = info.dyn;
    infod->metafuns[SOLU_META_EXTEND] = g->sprite;
    usr->metafuns[SOLU_META_EXTEND] = info;

    solu_valmap_set(&g->spr_cache, sf_str_cdup(name.dyn), out);
    return solu_ok(out);
}

solu_call_ex sgb_load_sound(solu_state *s) {
    solu_val name = solu_get(s, 0);
    if (!solu_isdtype(name, SOLU_DSTR))
        return solu_err(s, "arg 'name' expected str got %s", solu_typename(name).c_str);

    sgb_game *g = *(sgb_game **)solu_capturec(s, 0).dyn;
    sgb_snd_ex ex = sgb_open_sound(g->snd_dir, name.dyn);
    if (!ex.is_ok) {
        if (!ex.is_ok) {
            solu_call_ex res = solu_panic("%s", ex.err.c_str);
            sf_str_free(ex.err);
            return res;
        }
    }
    sgb_sounddata *sfx = malloc(sizeof(sgb_sounddata));
    *sfx = ex.ok;
    sfx->g = g;

    solu_val info = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(info.dyn, "name", solu_dnstr(s, sfx->name.c_str));
    solu_dobj_strset(info.dyn, "volume", (solu_val){SOLU_TF64, .f64=1.0});

    solu_val out = solu_dnusr(s, sizeof(sgb_spritedata *), "sfx", &sfx, sgb_sfx_delete, NULL);
    solu_usrwrap *usr = solu_uheader(out);
    solu_dobj *infod = info.dyn;
    infod->meta = solu_dnew(s, SOLU_DOBJ);
    infod->metafuns[SOLU_META_EXTEND] = g->snd;
    usr->metafuns[SOLU_META_EXTEND] = info;

    solu_val set = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(set.dyn, "volume", solu_wrapmfun(s, sgb_snd_volume, 1, NULL, 0));

    solu_dobj_strset(info.dyn, "set", set);
    usr->metafuns[SOLU_META_SET] = solu_wrapcfun(s, sgb_set, 2, (solu_val[]){out, g->gptr}, 2);
    return solu_ok(out);
}

solu_call_ex sgb_load_music(solu_state *s) {
    solu_val name = solu_get(s, 0);
    if (!solu_isdtype(name, SOLU_DSTR))
        return solu_err(s, "arg 'name' expected str got %s", solu_typename(name).c_str);

    sgb_game *g = *(sgb_game **)solu_capturec(s, 0).dyn;
    solu_valmap_ex exists = solu_valmap_get(&g->mus_cache, sf_ref(name.dyn));
    if (exists.is_ok && solu_isutype(exists.ok, sf_lit("mus")))
        return solu_ok(exists.ok);

    sgb_snd_ex ex = sgb_open_music(s, g->snd_dir, name.dyn);
    if (!ex.is_ok) {
        if (!ex.is_ok) {
            solu_call_ex res = solu_panic("%s", ex.err.c_str);
            sf_str_free(ex.err);
            return res;
        }
    }
    sgb_sounddata *sfx = malloc(sizeof(sgb_sounddata));
    *sfx = ex.ok;
    sfx->g = g;

    solu_val info = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(info.dyn, "name", solu_dnstr(s, sfx->name.c_str));
    solu_dobj_strset(info.dyn, "len", (solu_val){SOLU_TF64, .f64=GetMusicTimeLength(ex.ok.music)});
    solu_dobj_strset(info.dyn, "volume", (solu_val){SOLU_TF64, .f64=ex.ok.default_volume});
    solu_dobj_strset(info.dyn, "loop", (solu_val){SOLU_TBOOL, .boolean=true});
    sfx->music.looping = true;

    solu_val out = solu_dnusr(s, sizeof(sgb_spritedata *), "mus", &sfx, sgb_sfx_delete, NULL);
    solu_usrwrap *usr = solu_uheader(out);
    solu_dobj *infod = info.dyn;
    infod->metafuns[SOLU_META_EXTEND] = g->snd;
    usr->metafuns[SOLU_META_EXTEND] = info;

    solu_val set = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(set.dyn, "volume", solu_wrapmfun(s, sgb_snd_volume, 1, NULL, 0));
    solu_dobj_strset(set.dyn, "loop", solu_wrapmfun(s, sgb_snd_loop, 1, NULL, 0));

    solu_dobj_strset(info.dyn, "set", set);
    usr->metafuns[SOLU_META_SET] = solu_wrapcfun(s, sgb_set, 2, (solu_val[]){out, g->gptr}, 2);

    solu_valmap_set(&g->mus_cache, sf_str_cdup(name.dyn), out);
    return solu_ok(out);
}

solu_call_ex sgb_load_object(solu_state *s) {
    solu_val type = solu_get(s, 0);
    solu_val fields = solu_get(s, 1);
    if (!solu_isdtype(type, SOLU_DSTR))
        return solu_err(s, "arg 'name' expected str got %s", solu_typename(type).c_str);

    sgb_game *g = *(sgb_game **)solu_capturec(s, 0).dyn;
    solu_val obj = sgb_object_new(g, g->id_c++, sf_ref(type.dyn));
    if (solu_isdtype(obj, SOLU_DERR))
        return solu_err(s, "Object [%u]:%s:load() error:\n-> %s\n", g->id_c - 1, (char *)type.dyn, (char *)obj.dyn);

    if (solu_isdtype(fields, SOLU_DOBJ))
        solu_dappend(obj, fields);
    sgb_callmethod(g, obj.dyn, "start");

    return solu_ok(obj);
}

solu_call_ex sgb_draw_sprite(solu_state *s) {
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

    Color c = WHITE;
    solu_dobj *c_obj = color.dyn;
    if (solu_isdtype(color, SOLU_DOBJ)) {
        if (c_obj->array.count < 4 ||
            c_obj->array.data[0].tt != SOLU_TI64 ||
            c_obj->array.data[1].tt != SOLU_TI64 ||
            c_obj->array.data[2].tt != SOLU_TI64 ||
            c_obj->array.data[2].tt != SOLU_TI64)
            return solu_err(s, "arg 'color' expected obj[4:i64]");
        c = (Color){
            (uint8_t)min(max(c_obj->array.data[0].i64, 0), UINT8_MAX),
            (uint8_t)min(max(c_obj->array.data[1].i64, 0), UINT8_MAX),
            (uint8_t)min(max(c_obj->array.data[2].i64, 0), UINT8_MAX),
            (uint8_t)min(max(c_obj->array.data[3].i64, 0), UINT8_MAX),
        };
    }

    sgb_game *g = *(sgb_game **)solu_capturec(s, s->ccall->up_c - 1).dyn;
    if (!g->drawing)
        return solu_panic("Draw call outside of object:draw()");

    sgb_spritedata spr = **(sgb_spritedata **)sprite.dyn;

    if (frame.i64 > spr.frame_c - 1 || frame.i64 < 0)
        return solu_panic("Sprite '%s' does not contain frame %lld", spr.name.c_str, frame.i64);

    sgb_rect source = spr.frames[frame.i64];
    DrawTexturePro(
        spr.texture,
        (Rectangle){
            (float)source.x,
            (float)source.y,
            (float)source.width  * (xscale < 0 ? -1.0f : 1.0f),
            (float)source.height * (yscale < 0 ? -1.0f : 1.0f),
        },
        (Rectangle){
            g->gui ? (float)x.i64 : (float)(x.i64 - (solu_i64)g->camera.x),
            g->gui ? (float)y.i64 : (float)(y.i64 - (solu_i64)g->camera.y),
            (float)source.width * fabsf(xscale),
            (float)source.height * fabsf(yscale)
        },
        (Vector2){
            (float)source.origin.x * fabsf(xscale),
            (float)source.origin.y * fabsf(yscale)
        },
        (float)rot.f64,
        c
    );

    return solu_ok(SOLU_NIL);
}

solu_call_ex sgb_draw_rect(solu_state *s) {
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

    sgb_game *g = *(sgb_game **)solu_capturec(s, 0).dyn;
    if (!g->drawing)
        return solu_panic("Draw call outside of object:draw()");

    DrawRectangle(
        g->gui ? (int)x.i64 : (int)(x.i64 - (solu_i64)g->camera.x),
        g->gui ? (int)y.i64 : (int)(y.i64 - (solu_i64)g->camera.y),
        (int)w.i64, (int)h.i64, (Color){
        (uint8_t)obj->array.data[0].i64,
        (uint8_t)obj->array.data[1].i64,
        (uint8_t)obj->array.data[2].i64,
        (uint8_t)obj->array.data[3].i64,
    });

    return solu_ok(SOLU_NIL);
}

solu_call_ex sgb_snd_play(solu_state *s) {
    solu_val sfx = solu_selfc(s);
    if (!solu_isutype(sfx, sf_lit("sfx")) && !solu_isutype(sfx, sf_lit("mus")))
        return solu_panic("'self' expected sfx|mus got %s", solu_typename(sfx).c_str);
    if (!IsAudioDeviceReady())
        return solu_panic("Audio device is not ready!");
    sgb_sounddata *snd = *(sgb_sounddata **)sfx.dyn;

    solu_val volume = solu_dobj_strget(solu_uheader(sfx)->metafuns[SOLU_META_EXTEND].dyn, "volume");
    if (volume.tt == SOLU_TF64)
        snd->tt == SGB_MUSIC ? SetMusicVolume(snd->music, (float)volume.f64) :
        SetSoundVolume(snd->sound, (float)volume.f64);

    snd->tt == SGB_MUSIC ? PlayMusicStream(snd->music) : PlaySound(snd->sound);
    return solu_ok(SOLU_NIL);
}

solu_call_ex sgb_snd_is_playing(solu_state *s) {
    solu_val sfx = solu_selfc(s);
    if (!solu_isutype(sfx, sf_lit("sfx")) && !solu_isutype(sfx, sf_lit("mus")))
        return solu_panic("'self' expected sfx|mus got %s", solu_typename(sfx).c_str);
    sgb_sounddata *snd = *(sgb_sounddata **)sfx.dyn;
    return solu_ok((solu_val){ SOLU_TBOOL,
        .boolean = snd->tt == SGB_MUSIC ? IsMusicStreamPlaying(snd->music) : IsSoundPlaying(snd->sound)
    });
}

solu_call_ex sgb_snd_stop(solu_state *s) {
    solu_val sfx = solu_selfc(s);
    if (!solu_isutype(sfx, sf_lit("sfx")) && !solu_isutype(sfx, sf_lit("mus")))
        return solu_panic("'self' expected sfx|mus got %s", solu_typename(sfx).c_str);
    sgb_sounddata *snd = *(sgb_sounddata **)sfx.dyn;
    snd->tt == SGB_MUSIC ? StopMusicStream(snd->music) : StopSound(snd->sound);
    return solu_ok(SOLU_NIL);
}

solu_call_ex sgb_snd_volume(solu_state *s) {
    solu_val sfx = solu_selfc(s);
    if (!solu_isutype(sfx, sf_lit("sfx")) && !solu_isutype(sfx, sf_lit("mus")))
        return solu_panic("'self' expected sfx|mus got %s", solu_typename(sfx).c_str);
    solu_val volume = solu_get(s, 0);
    if (volume.tt != SOLU_TF64)
        return solu_panic("arg 'volume' expectected f64 got %s", solu_typename(volume).c_str);
    sgb_sounddata *snd = *(sgb_sounddata **)sfx.dyn;
    snd->tt == SGB_MUSIC ? SetMusicVolume(snd->music, (float)volume.f64) : SetSoundVolume(snd->sound, (float)volume.f64);
    solu_dobj_strset(solu_uheader(sfx)->metafuns[SOLU_META_EXTEND].dyn, "volume", volume);
    return solu_ok(SOLU_NIL);
}

solu_call_ex sgb_snd_loop(solu_state *s) {
    solu_val sfx = solu_selfc(s);
    if (!solu_isutype(sfx, sf_lit("sfx")) && !solu_isutype(sfx, sf_lit("mus")))
        return solu_panic("'self' expected sfx|mus got %s", solu_typename(sfx).c_str);
    solu_val loop = solu_get(s, 0);
    if (loop.tt != SOLU_TBOOL)
        return solu_panic("arg 'loop' expectected bool got %s", solu_typename(loop).c_str);
    sgb_sounddata *snd = *(sgb_sounddata **)sfx.dyn;
    snd->music.looping = loop.boolean;
    solu_dobj_strset(solu_uheader(sfx)->metafuns[SOLU_META_EXTEND].dyn, "loop", loop);
    return solu_ok(SOLU_NIL);
}

solu_call_ex sgb_key_held(solu_state *s) {
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);
    return solu_ok((solu_val){SOLU_TBOOL, .boolean=IsKeyDown((int)key.i64)});
}

solu_call_ex sgb_key_pressed(solu_state *s) {
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);
    return solu_ok((solu_val){SOLU_TBOOL, .boolean=IsKeyPressed((int)key.i64)});
}

solu_call_ex sgb_key_released(solu_state *s) {
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);
    return solu_ok((solu_val){SOLU_TBOOL, .boolean=IsKeyReleased((int)key.i64)});
}

solu_call_ex sgb_key_string(solu_state *s) {
    size_t len = 16, cur = 0;
    char *str = calloc(len, 1);
    for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed()) {
        if ((ch >= 32) && (ch <= 126)) {
            if (cur >= len - 1)
                str = realloc(str, len *= 2);
            str[cur++] = (char)ch;
            str[cur] = '\0';
        }
    }
    solu_val kstr = solu_dnstr(s, str);
    free(str);
    return solu_ok(kstr);
}

solu_call_ex sgb_mouse_wheel(solu_state *s) {
    (void)s;
    return solu_ok((solu_val){SOLU_TF64, .f64=GetMouseWheelMove()});
}

solu_call_ex sgb_mouse_held(solu_state *s) {
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);
    return solu_ok((solu_val){SOLU_TBOOL, .boolean=IsMouseButtonDown((int)key.i64)});
}
solu_call_ex sgb_mouse_pressed(solu_state *s) {
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);
    return solu_ok((solu_val){SOLU_TBOOL, .boolean=IsMouseButtonPressed((int)key.i64)});
}
solu_call_ex sgb_mouse_released(solu_state *s) {
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);
    return solu_ok((solu_val){SOLU_TBOOL, .boolean=IsMouseButtonReleased((int)key.i64)});
}

solu_val sgb_keys(solu_state *s) {
    solu_val keys = solu_dnew(s, SOLU_DOBJ);
    // Letters
    solu_dobj_strset(keys.dyn, "a", (solu_val){SOLU_TI64, .i64=KEY_A});
    solu_dobj_strset(keys.dyn, "b", (solu_val){SOLU_TI64, .i64=KEY_B});
    solu_dobj_strset(keys.dyn, "c", (solu_val){SOLU_TI64, .i64=KEY_C});
    solu_dobj_strset(keys.dyn, "d", (solu_val){SOLU_TI64, .i64=KEY_D});
    solu_dobj_strset(keys.dyn, "e", (solu_val){SOLU_TI64, .i64=KEY_E});
    solu_dobj_strset(keys.dyn, "f", (solu_val){SOLU_TI64, .i64=KEY_F});
    solu_dobj_strset(keys.dyn, "g", (solu_val){SOLU_TI64, .i64=KEY_G});
    solu_dobj_strset(keys.dyn, "h", (solu_val){SOLU_TI64, .i64=KEY_H});
    solu_dobj_strset(keys.dyn, "i", (solu_val){SOLU_TI64, .i64=KEY_I});
    solu_dobj_strset(keys.dyn, "j", (solu_val){SOLU_TI64, .i64=KEY_J});
    solu_dobj_strset(keys.dyn, "k", (solu_val){SOLU_TI64, .i64=KEY_K});
    solu_dobj_strset(keys.dyn, "l", (solu_val){SOLU_TI64, .i64=KEY_L});
    solu_dobj_strset(keys.dyn, "m", (solu_val){SOLU_TI64, .i64=KEY_M});
    solu_dobj_strset(keys.dyn, "n", (solu_val){SOLU_TI64, .i64=KEY_N});
    solu_dobj_strset(keys.dyn, "o", (solu_val){SOLU_TI64, .i64=KEY_O});
    solu_dobj_strset(keys.dyn, "p", (solu_val){SOLU_TI64, .i64=KEY_P});
    solu_dobj_strset(keys.dyn, "q", (solu_val){SOLU_TI64, .i64=KEY_Q});
    solu_dobj_strset(keys.dyn, "r", (solu_val){SOLU_TI64, .i64=KEY_R});
    solu_dobj_strset(keys.dyn, "s", (solu_val){SOLU_TI64, .i64=KEY_S});
    solu_dobj_strset(keys.dyn, "t", (solu_val){SOLU_TI64, .i64=KEY_T});
    solu_dobj_strset(keys.dyn, "u", (solu_val){SOLU_TI64, .i64=KEY_U});
    solu_dobj_strset(keys.dyn, "v", (solu_val){SOLU_TI64, .i64=KEY_V});
    solu_dobj_strset(keys.dyn, "w", (solu_val){SOLU_TI64, .i64=KEY_W});
    solu_dobj_strset(keys.dyn, "x", (solu_val){SOLU_TI64, .i64=KEY_X});
    solu_dobj_strset(keys.dyn, "y", (solu_val){SOLU_TI64, .i64=KEY_Y});
    solu_dobj_strset(keys.dyn, "z", (solu_val){SOLU_TI64, .i64=KEY_Z});

    // Numbers
    solu_dobj_strset(keys.dyn, "zero",  (solu_val){SOLU_TI64, .i64=KEY_ZERO});
    solu_dobj_strset(keys.dyn, "one",   (solu_val){SOLU_TI64, .i64=KEY_ONE});
    solu_dobj_strset(keys.dyn, "two",   (solu_val){SOLU_TI64, .i64=KEY_TWO});
    solu_dobj_strset(keys.dyn, "three", (solu_val){SOLU_TI64, .i64=KEY_THREE});
    solu_dobj_strset(keys.dyn, "four",  (solu_val){SOLU_TI64, .i64=KEY_FOUR});
    solu_dobj_strset(keys.dyn, "five",  (solu_val){SOLU_TI64, .i64=KEY_FIVE});
    solu_dobj_strset(keys.dyn, "six",   (solu_val){SOLU_TI64, .i64=KEY_SIX});
    solu_dobj_strset(keys.dyn, "seven", (solu_val){SOLU_TI64, .i64=KEY_SEVEN});
    solu_dobj_strset(keys.dyn, "eight", (solu_val){SOLU_TI64, .i64=KEY_EIGHT});
    solu_dobj_strset(keys.dyn, "nine",  (solu_val){SOLU_TI64, .i64=KEY_NINE});
    solu_dobj_strset(keys.dyn, "equal", (solu_val){SOLU_TI64, .i64=KEY_EQUAL});
    solu_dobj_strset(keys.dyn, "minus",  (solu_val){SOLU_TI64, .i64=KEY_MINUS});

    // Function keys
    solu_dobj_strset(keys.dyn, "tilde",  (solu_val){SOLU_TI64, .i64=KEY_GRAVE});
    solu_dobj_strset(keys.dyn, "f1",  (solu_val){SOLU_TI64, .i64=KEY_F1});
    solu_dobj_strset(keys.dyn, "f2",  (solu_val){SOLU_TI64, .i64=KEY_F2});
    solu_dobj_strset(keys.dyn, "f3",  (solu_val){SOLU_TI64, .i64=KEY_F3});
    solu_dobj_strset(keys.dyn, "f4",  (solu_val){SOLU_TI64, .i64=KEY_F4});
    solu_dobj_strset(keys.dyn, "f5",  (solu_val){SOLU_TI64, .i64=KEY_F5});
    solu_dobj_strset(keys.dyn, "f6",  (solu_val){SOLU_TI64, .i64=KEY_F6});
    solu_dobj_strset(keys.dyn, "f7",  (solu_val){SOLU_TI64, .i64=KEY_F7});
    solu_dobj_strset(keys.dyn, "f8",  (solu_val){SOLU_TI64, .i64=KEY_F8});
    solu_dobj_strset(keys.dyn, "f9",  (solu_val){SOLU_TI64, .i64=KEY_F9});
    solu_dobj_strset(keys.dyn, "f10", (solu_val){SOLU_TI64, .i64=KEY_F10});
    solu_dobj_strset(keys.dyn, "f11", (solu_val){SOLU_TI64, .i64=KEY_F11});
    solu_dobj_strset(keys.dyn, "f12", (solu_val){SOLU_TI64, .i64=KEY_F12});

    // Controls
    solu_dobj_strset(keys.dyn, "space",        (solu_val){SOLU_TI64, .i64=KEY_SPACE});
    solu_dobj_strset(keys.dyn, "enter",        (solu_val){SOLU_TI64, .i64=KEY_ENTER});
    solu_dobj_strset(keys.dyn, "tab",          (solu_val){SOLU_TI64, .i64=KEY_TAB});
    solu_dobj_strset(keys.dyn, "backspace",    (solu_val){SOLU_TI64, .i64=KEY_BACKSPACE});
    solu_dobj_strset(keys.dyn, "escape",       (solu_val){SOLU_TI64, .i64=KEY_ESCAPE});
    solu_dobj_strset(keys.dyn, "insert",       (solu_val){SOLU_TI64, .i64=KEY_INSERT});
    solu_dobj_strset(keys.dyn, "delete",       (solu_val){SOLU_TI64, .i64=KEY_DELETE});
    solu_dobj_strset(keys.dyn, "home",         (solu_val){SOLU_TI64, .i64=KEY_HOME});
    solu_dobj_strset(keys.dyn, "end",          (solu_val){SOLU_TI64, .i64=KEY_END});
    solu_dobj_strset(keys.dyn, "page_up",      (solu_val){SOLU_TI64, .i64=KEY_PAGE_UP});
    solu_dobj_strset(keys.dyn, "page_down",    (solu_val){SOLU_TI64, .i64=KEY_PAGE_DOWN});

    // Arrows
    solu_dobj_strset(keys.dyn, "right", (solu_val){SOLU_TI64, .i64=KEY_RIGHT});
    solu_dobj_strset(keys.dyn, "left",  (solu_val){SOLU_TI64, .i64=KEY_LEFT});
    solu_dobj_strset(keys.dyn, "down",  (solu_val){SOLU_TI64, .i64=KEY_DOWN});
    solu_dobj_strset(keys.dyn, "up",    (solu_val){SOLU_TI64, .i64=KEY_UP});

    // Modifiers
    solu_dobj_strset(keys.dyn, "left_shift",   (solu_val){SOLU_TI64, .i64=KEY_LEFT_SHIFT});
    solu_dobj_strset(keys.dyn, "right_shift",  (solu_val){SOLU_TI64, .i64=KEY_RIGHT_SHIFT});
    solu_dobj_strset(keys.dyn, "left_control", (solu_val){SOLU_TI64, .i64=KEY_LEFT_CONTROL});
    solu_dobj_strset(keys.dyn, "right_control",(solu_val){SOLU_TI64, .i64=KEY_RIGHT_CONTROL});
    solu_dobj_strset(keys.dyn, "left_alt",     (solu_val){SOLU_TI64, .i64=KEY_LEFT_ALT});
    solu_dobj_strset(keys.dyn, "right_alt",    (solu_val){SOLU_TI64, .i64=KEY_RIGHT_ALT});
    solu_dobj_strset(keys.dyn, "left_super",   (solu_val){SOLU_TI64, .i64=KEY_LEFT_SUPER});
    solu_dobj_strset(keys.dyn, "right_super",  (solu_val){SOLU_TI64, .i64=KEY_RIGHT_SUPER});
    solu_dobj_strset(keys.dyn, "caps_lock",    (solu_val){SOLU_TI64, .i64=KEY_CAPS_LOCK});
    solu_dobj_strset(keys.dyn, "scroll_lock",  (solu_val){SOLU_TI64, .i64=KEY_SCROLL_LOCK});
    solu_dobj_strset(keys.dyn, "num_lock",     (solu_val){SOLU_TI64, .i64=KEY_NUM_LOCK});
    solu_dobj_strset(keys.dyn, "print_screen", (solu_val){SOLU_TI64, .i64=KEY_PRINT_SCREEN});
    solu_dobj_strset(keys.dyn, "pause",        (solu_val){SOLU_TI64, .i64=KEY_PAUSE});

    // Keypad
    solu_dobj_strset(keys.dyn, "kp_0", (solu_val){SOLU_TI64, .i64=KEY_KP_0});
    solu_dobj_strset(keys.dyn, "kp_1", (solu_val){SOLU_TI64, .i64=KEY_KP_1});
    solu_dobj_strset(keys.dyn, "kp_2", (solu_val){SOLU_TI64, .i64=KEY_KP_2});
    solu_dobj_strset(keys.dyn, "kp_3", (solu_val){SOLU_TI64, .i64=KEY_KP_3});
    solu_dobj_strset(keys.dyn, "kp_4", (solu_val){SOLU_TI64, .i64=KEY_KP_4});
    solu_dobj_strset(keys.dyn, "kp_5", (solu_val){SOLU_TI64, .i64=KEY_KP_5});
    solu_dobj_strset(keys.dyn, "kp_6", (solu_val){SOLU_TI64, .i64=KEY_KP_6});
    solu_dobj_strset(keys.dyn, "kp_7", (solu_val){SOLU_TI64, .i64=KEY_KP_7});
    solu_dobj_strset(keys.dyn, "kp_8", (solu_val){SOLU_TI64, .i64=KEY_KP_8});
    solu_dobj_strset(keys.dyn, "kp_9", (solu_val){SOLU_TI64, .i64=KEY_KP_9});
    solu_dobj_strset(keys.dyn, "kp_decimal",  (solu_val){SOLU_TI64, .i64=KEY_KP_DECIMAL});
    solu_dobj_strset(keys.dyn, "kp_divide",   (solu_val){SOLU_TI64, .i64=KEY_KP_DIVIDE});
    solu_dobj_strset(keys.dyn, "kp_multiply", (solu_val){SOLU_TI64, .i64=KEY_KP_MULTIPLY});
    solu_dobj_strset(keys.dyn, "kp_subtract", (solu_val){SOLU_TI64, .i64=KEY_KP_SUBTRACT});
    solu_dobj_strset(keys.dyn, "kp_add",      (solu_val){SOLU_TI64, .i64=KEY_KP_ADD});
    solu_dobj_strset(keys.dyn, "kp_enter",    (solu_val){SOLU_TI64, .i64=KEY_KP_ENTER});
    solu_dobj_strset(keys.dyn, "kp_equal",    (solu_val){SOLU_TI64, .i64=KEY_KP_EQUAL});

    return keys;
}

solu_val sgb_mouse(solu_state *s) {
    solu_val mouse = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(mouse.dyn, "left", (solu_val){SOLU_TI64, .i64=MOUSE_BUTTON_LEFT});
    solu_dobj_strset(mouse.dyn, "middle", (solu_val){SOLU_TI64, .i64=MOUSE_BUTTON_MIDDLE});
    solu_dobj_strset(mouse.dyn, "right", (solu_val){SOLU_TI64, .i64=MOUSE_BUTTON_RIGHT});
    solu_dobj_strset(mouse.dyn, "forward", (solu_val){SOLU_TI64, .i64=MOUSE_BUTTON_FORWARD});
    solu_dobj_strset(mouse.dyn, "back", (solu_val){SOLU_TI64, .i64=MOUSE_BUTTON_BACK});
    return mouse;
}
