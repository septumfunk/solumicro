#include "script.h"
#include "asset.h"
#include "game.h"
#include "sf/containers/buffer.h"
#include "sf/str.h"
#include "solus/bytecode.h"
#include "solus/val.h"
#include "solus/vm.h"
#include <math.h>
#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>

const char SGB_DEFAULT_CONFIG[] = "{ \n\
    title = 'solumicro'\n\
    resolution = {\n\
        width = 160\n\
        height = 144\n\
        scale = 3\n\
    }\n\
    path = {\n\
        objects = 'scripts'\n\
        rooms = 'rooms'\n\
        sprites = 'sprites'\n\
    }\n\
}";

const char SGB_DEFAULT_ROOM[] = "{ \n\
    name = 'Room'\n\
    spawns = {\n\
        # 'foo',\n\
    }\n\
}";

solu_val sgb_manifest_load(solu_state *s) {
    solu_val out = SOLU_NIL;

    solu_compile_ex comp_ex = solu_cfile(s, "manifest.solu");
    if (!comp_ex.is_ok) {
        if (sf_file_exists(sf_lit("manifest.solu")))
            return SOLU_NIL;
        FILE *f = fopen("manifest.solu", "w");
        if (!f) return SOLU_NIL;
        fwrite(SGB_DEFAULT_CONFIG, 1, sizeof(SGB_DEFAULT_CONFIG) - 1, f);
        fclose(f);
        comp_ex = solu_csrc(s, (char *)SGB_DEFAULT_CONFIG);
    }

    solu_call_ex call_ex = solu_call(s, &comp_ex.ok, NULL, 0);
    if (!call_ex.is_ok) {
        sf_str e = sf_str_fmt("Panic during manifest.solu: %s\n", call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt));
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        if (call_ex.err.panic)
            free(call_ex.err.panic);
        return out;
    }
    if (!solu_isdtype(call_ex.ok, SOLU_DOBJ)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to return obj, got %s\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }

    if (!solu_isdtype(solu_dobj_strget(call_ex.ok.dyn, "title"), SOLU_DSTR)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain title:str\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }

    solu_val resolution = solu_dobj_strget(call_ex.ok.dyn, "resolution");
    if (!solu_isdtype(resolution, SOLU_DOBJ)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain resolution:obj\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }
    if (solu_dobj_strget(resolution.dyn, "width").tt != SOLU_TI64) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain resolution.width:i64\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }
    if (solu_dobj_strget(resolution.dyn, "height").tt != SOLU_TI64) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain resolution.height:i64\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }
    if (solu_dobj_strget(resolution.dyn, "scale").tt != SOLU_TI64) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain resolution.scale:i64\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }

    solu_val path = solu_dobj_strget(call_ex.ok.dyn, "path");
    if (!solu_isdtype(path, SOLU_DOBJ)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain path:obj\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }

    if (!solu_isdtype(solu_dobj_strget(path.dyn, "rooms"), SOLU_DSTR)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain path.rooms:str\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }
    if (!solu_isdtype(solu_dobj_strget(path.dyn, "objects"), SOLU_DSTR)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain path.objects:str\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }
    if (!solu_isdtype(solu_dobj_strget(path.dyn, "sprites"), SOLU_DSTR)) {
        sf_str e = sf_str_fmt("Expected manifest.solu to contain path.sprites:str\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(s, e.c_str);
        sf_str_free(e);
        return out;
    }

    solu_dhold(call_ex.ok);
    return call_ex.ok;
}

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
        sf_str e = sf_str_fmt("Unable to load %s: %s\n", path.c_str, solu_err_string(comp_ex.err.tt));
        out = solu_dnerr(g->s, e.c_str);
        sf_str_free(e);
        return out;
    }

    solu_call_ex call_ex = solu_call(g->s, &comp_ex.ok, NULL, 0);
    if (!call_ex.is_ok) {
        uint16_t line = SOLU_DBG_LINE(comp_ex.ok.dbg[call_ex.err.pc]);
        sf_str e = sf_str_fmt("Panic while loading gameobject:%u: %s\n", line, call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt));
        out = solu_dnerr(g->s, e.c_str);
        sf_str_free(e);
        if (call_ex.err.panic)
            free(call_ex.err.panic);
        return out;
    }
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

    return out;
}

void sgb_register(sgb_game *g) {
    solu_val load = solu_dnew(g->s, SOLU_DOBJ);
    solu_dobj_strset(load.dyn, "sprite", solu_wrapcfun(g->s, sgb_load_sprite, 1, &g->gptr, 1));
    solu_dobj_strset(load.dyn, "object", solu_wrapcfun(g->s, sgb_load_object, 2, &g->gptr, 1));

    solu_val draw = solu_dnew(g->s, SOLU_DOBJ);
    solu_dobj_strset(draw.dyn, "sprite", solu_wrapcfun(g->s, sgb_draw_sprite, 7, &g->gptr, 1));
    solu_dobj_strset(draw.dyn, "rect", solu_wrapcfun(g->s, sgb_draw_rect, 5, &g->gptr, 1));

    solu_val input = solu_dnew(g->s, SOLU_DOBJ);
    solu_dobj_strset(input.dyn, "key_down", solu_wrapcfun(g->s, sgb_key_down, 1, NULL, 0));
    solu_dobj_strset(input.dyn, "key_pressed", solu_wrapcfun(g->s, sgb_key_pressed, 1, NULL, 0));
    solu_dobj_strset(input.dyn, "key_released", solu_wrapcfun(g->s, sgb_key_released, 1, NULL, 0));

     solu_setg(g->s, "load", load);
    solu_setg(g->s, "draw", draw);
    solu_setg(g->s, "input", input);
    solu_setg(g->s, "key", sgb_keys(g->s));
}

solu_call_ex sgb_quit(solu_state *s) {
    (*(sgb_game **)solu_capturec(s, 0).dyn)->open = false;
    CloseWindow();
    return solu_ok(SOLU_NIL);
}

solu_call_ex sgb_set(solu_state *s) {
    solu_val key = solu_get(s, 0);
    solu_val val = solu_get(s, 1);
    if (!solu_isdtype(key, SOLU_DSTR))
        return solu_panic("arg 'key' expected str got %s", solu_typename(key).c_str);

    solu_dobj *obj = solu_capturec(s, 0).dyn;
    solu_dobj *m_obj = obj->meta.dyn;
    if (!obj || !m_obj) return solu_panic("Game Corrupt");
    solu_dobj *set = solu_dobj_strget(m_obj, "set").dyn;
    if (!set) return solu_panic("Game Corrupt");

    solu_val f = solu_dobj_strget(set, key.dyn);
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

static void sgb_spr_delete(void *_spr) {
    sgb_spritedata *spr = _spr;
    solu_valmap_delete(&((sgb_game *)spr->g)->sprites, spr->name);
    sgb_spritedata_free(*spr);
}

static solu_call_ex sgb_gwrap(solu_state *s) {
    return solu_ok(solu_dobj_get(s, solu_capturec(s, 0).dyn, solu_get(s, 0)));
}

solu_call_ex sgb_load_sprite(solu_state *s) {
    solu_val name = solu_get(s, 0);
    if (!solu_isdtype(name, SOLU_DSTR))
        return solu_err(s, "arg 'name' expected str got %s", solu_typename(name).c_str);

    sgb_game *g = *(sgb_game **)solu_capturec(s, 0).dyn;
    solu_valmap_ex exists = solu_valmap_get(&g->sprites, sf_ref(name.dyn));
    if (exists.is_ok && solu_isutype(exists.ok, sf_lit("spr")))
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
    solu_dobj_strset(info.dyn, "frames", (solu_val){SOLU_TI64, .i64=spr->frame_c});

    solu_val out = solu_dnusr(s, sizeof(sgb_spritedata *), "spr", &spr, sgb_spr_delete, NULL);
    solu_usrwrap *usr = solu_uheader(out);
    usr->metafuns[SOLU_META_SET] = solu_wrapcfun(s, sgb_noset, 2, NULL, 0);
    usr->metafuns[SOLU_META_GET] = solu_wrapcfun(s, sgb_gwrap, 1, &info, 1);

    solu_valmap_set(&g->sprites, sf_str_cdup(name.dyn), out);
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

    sgb_callmethod(g, obj.dyn, "start");
    if (solu_isdtype(fields, SOLU_DOBJ))
        solu_dappend(obj, fields);

    return solu_ok(obj);
}

solu_call_ex sgb_draw_sprite(solu_state *s) {
    solu_val sprite = solu_get(s, 0);
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

    sgb_game *g = *(sgb_game **)solu_capturec(s, 0).dyn;
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
            (float)x.i64,
            (float)y.i64,
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

solu_call_ex sgb_key_down(solu_state *s) {
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

    // Numbers (top row)
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
