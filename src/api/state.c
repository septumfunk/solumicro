#include "../api.h"
#include "solus/api.h"
#include <SDL2/SDL_video.h>

solu_call_ex smc_quit(solu_state *s) {
    (*(smc_game **)solu_capturec(s, 0).dyn)->open = false;
    return solu_ok(SOLU_NIL);
}

solu_call_ex smc_set_room(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 1).dyn;
    solu_val val = solu_get(s, 0);
    if (!solu_isdtype(val, SOLU_DSTR))
        return solu_err(s, "setter 'room' expected str got %s", solu_typename(val).c_str);

    if (sf_str_eq(g->room, sf_ref(val.dyn)))
        return solu_ok(val);

    g->room = sf_str_cdup(val.dyn);
    solu_dobj_strset(solu_capturec(s, 0).dyn, "room", solu_dnstr(s, g->room.c_str));
    g->roomchange = true;

    return solu_ok(val);
}

solu_call_ex smc_set_title(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 1).dyn;
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

solu_call_ex smc_set_paused(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 1).dyn;
    solu_val val = solu_get(s, 0);
    if (val.tt != SOLU_TBOOL)
        return solu_err(s, "setter 'paused' expected bool got %s", solu_typename(val).c_str);

    g->paused = val.boolean;
    solu_dobj_strset(g->ginfo.dyn, "paused", val);
    smc_info("%s", val.boolean ? "Game paused." : "Game unpaused.");
    return solu_ok(val);
}

solu_val smc_object_new(smc_game *g, solu_i64 id, sf_str path) {
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
    if (smc_is_solc(rp)) {
        solu_load_ex load_ex = solu_loadfun(g->s, rp);
        if (!load_ex.is_ok) {
            free(rp);
            sf_str e = sf_str_fmt(
                "Failed to compile object '%s': %s",
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
            char *trace = solu_ctrace_print(rp, comp_ex.err, 15, 2, 1);
            free(rp);
            sf_str e = sf_str_fmt(
                "Failed to compile object '%s': %s",
                path.c_str, trace
            );
            free(trace);
            solu_val er = solu_dnerr(g->s, e.c_str);
            sf_str_free(e);
            return er;
        }
        fp = comp_ex.ok;
    }

    solu_call_ex call_ex = solu_call(g->s, &fp, NULL, 0);

    if (!call_ex.is_ok) {
        sf_str e;
        char *trace = solu_trace_print(call_ex.err.trace, 5, 2, 1);
        if (trace) {
            e = sf_str_fmt(
                TUI_ERR "Object %s:load() error: %s\n%s\n" TUI_CLEAR,
                path, call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt),
                trace
            );
            free(trace);
        } else e = sf_str_fmt(
            TUI_ERR "Object %s:load() error: %s\n" TUI_CLEAR,
            path, call_ex.err.panic ? call_ex.err.panic : solu_err_string(call_ex.err.tt)
        );
        free(rp);
        solu_fproto_free(&fp);
        out = solu_dnerr(g->s, e.c_str);
        sf_str_free(e);
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

solu_call_ex smc_load_object(solu_state *s) {
    solu_val type = solu_get(s, 0);
    solu_val fields = solu_get(s, 1);
    if (!solu_isdtype(type, SOLU_DSTR))
        return solu_err(s, "arg 'name' expected str got %s", solu_typename(type).c_str);

    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    solu_val obj = smc_object_new(g, g->id_c++, sf_ref(type.dyn));
    if (solu_isdtype(obj, SOLU_DERR))
        return solu_err(s, "Object [%u]:%s:load() error:\n-> %s\n", g->id_c - 1, (char *)type.dyn, (char *)obj.dyn);

    if (solu_isdtype(fields, SOLU_DOBJ))
        solu_dappend(obj, fields);
    smc_callmethod(g, obj.dyn, "start");

    return solu_ok(obj);
}

solu_call_ex smc_delete(solu_state *s) {
    solu_val self = solu_selfc(s);
    if (!solu_isdtype(self, SOLU_DOBJ))
        return solu_panic(s, "arg 'self' expected obj, found %s", solu_typename(self).c_str);
    solu_val id = solu_dobj_strget(self.dyn, "id");
    if (id.tt != SOLU_TI64)
        return solu_panic(s, "self.id expected i64, found %s", solu_typename(self).c_str);
    smc_game *g = *(smc_game **)solu_capturec(s, 1).dyn;
    solu_dobj *d = g->objects.dyn;
    if (id.i64 < 0 || id.i64 > d->array.count - 1 || id.i64 > UINT32_MAX)
        return solu_panic(s, "self.id is invalid");
    smc_callmethod(g, self.dyn, "cleanup");
    solu_valvec_set(&((solu_dobj *)g->objects.dyn)->array, (uint32_t)id.i64, SOLU_NIL);
    return solu_ok(SOLU_NIL);
}
