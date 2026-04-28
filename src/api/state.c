#include "../api.h"
#include "solus/vm.h"
#include <SDL2/SDL_video.h>

solu_call_ex sgb_quit(solu_state *s) {
    (*(sgb_game **)solu_capturec(s, 0).dyn)->open = false;
    return solu_ok(SOLU_NIL);
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
    SDL_SetWindowTitle(g->win, val.dyn);
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

solu_val sgb_object_new(sgb_game *g, solu_i64 id, sf_str path) {
    solu_val out = SOLU_NIL;
    char *rp = sf_str_fmt("%s/%s", g->obj_dir.c_str, path.c_str).c_str;
    char *rpath = solu_findfile(g->s, rp);
    free(rp);
    if (!rpath) {
        sf_str e = sf_str_fmt("Unable to locate object %s\n", path.c_str);
        out = solu_dnerr(g->s, e.c_str);
        sf_str_free(e);
        return out;
    }
    rp = rpath;

    solu_fproto fp;
    if (sgb_is_solc(rp)) {
        solu_load_ex load_ex = solu_loadfun(g->s, rp);
        if (!load_ex.is_ok) {
            free(rp);
            sf_str e = sf_str_fmt(
                "Failed to compile sprite '%s': %s",
                path.c_str, solu_err_string(load_ex.err)
            );
            solu_val er = solu_dnerr(g->s, e.c_str);
            sf_str_free(e);
            return er;
        }
        fp = load_ex.ok;
    } else {
        solu_compile_ex comp_ex = solu_cfile(g->s, rp);
        if (!comp_ex.is_ok) {
            free(rp);
            sf_str e = sf_str_fmt(
                "Failed to compile sprite '%s': %s",
                path.c_str, solu_err_string(comp_ex.err.tt)
            );
            solu_val er = solu_dnerr(g->s, e.c_str);
            sf_str_free(e);
            return er;
        }
        fp = comp_ex.ok;
    }

    solu_call_ex call_ex = solu_call(g->s, &fp, NULL, 0);

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
        solu_fproto_free(&fp);
        out = solu_dnerr(g->s, e.c_str);
        sf_str_free(e);
        if (call_ex.err.panic)
            free(call_ex.err.panic);
        return out;
    }
    free(rp);

    solu_fproto_free(&fp);
    if (!solu_isdtype(call_ex.ok, SOLU_DOBJ)) {
        sf_str e = sf_str_fmt("Expected gameobject to return obj, got %s\n", solu_typename(call_ex.ok).c_str);
        out = solu_dnerr(g->s, e.c_str);
        sf_str_free(e);
        return out;
    }
    out = call_ex.ok;

    solu_val idv = {SOLU_TI64, .i64=id};
    solu_dobj_strset(out.dyn, "id", idv);
    if (solu_dobj_strget(out.dyn, "depth").tt != SOLU_TF64)
        solu_dobj_strset(out.dyn, "depth", (solu_val){SOLU_TF64, .f64=0});
    solu_dobj_strset(out.dyn, "type", solu_dnstr(g->s, path.c_str));
    solu_dobj_set(g->s, g->objects.dyn, idv, out);
    solu_dheader(out)->metadata[SOLU_META_EXTEND] = g->obj;

    return out;
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

solu_call_ex sgb_delete(solu_state *s) {
    solu_val self = solu_selfc(s);
    if (!solu_isdtype(self, SOLU_DOBJ))
        return solu_panic(s, "arg 'self' expected obj, found %s", solu_typename(self).c_str);
    solu_val id = solu_dobj_strget(self.dyn, "id");
    if (id.tt != SOLU_TI64)
        return solu_panic(s, "self.id expected i64, found %s", solu_typename(self).c_str);
    sgb_game *g = *(sgb_game **)solu_capturec(s, 1).dyn;
    solu_dobj *d = g->objects.dyn;
    if (id.i64 < 0 || id.i64 > d->array.count - 1 || id.i64 > UINT32_MAX)
        return solu_panic(s, "self.id is invalid");
    sgb_callmethod(g, self.dyn, "cleanup");
    solu_valvec_set(&((solu_dobj *)g->objects.dyn)->array, (uint32_t)id.i64, SOLU_NIL);
    return solu_ok(SOLU_NIL);
}
