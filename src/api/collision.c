#include "../api.h"
#include <math.h>

static inline uint32_t *sgb_find_parts(sgb_game *g, sgb_frect fr, uint32_t *size) {
    sgb_collision *c = &g->collision_data;
    *size = 0;

    solu_i64 x0 = (solu_i64)floor(fr.x);
    solu_i64 y0 = (solu_i64)floor(fr.y);
    solu_i64 x1 = (solu_i64)ceil(fr.x + fr.width);
    solu_i64 y1 = (solu_i64)ceil(fr.y + fr.height);
    if (x0 < c->world.x) x0 = c->world.x;
    if (y0 < c->world.y) y0 = c->world.y;
    if (x1 > c->world.x + c->world.width) x1 = c->world.x + c->world.width;
    if (y1 > c->world.y + c->world.height) y1 = c->world.y + c->world.height;
    if (x1 <= x0 || y1 <= y0)
        return NULL;

    uint32_t cols = (uint32_t)((c->world.width + c->grid - 1) / c->grid);

    uint32_t min_x = (uint32_t)((x0 - c->world.x) / c->grid);
    uint32_t min_y = (uint32_t)((y0 - c->world.y) / c->grid);
    uint32_t max_x = (uint32_t)((x1 - 1 - c->world.x) / c->grid);
    uint32_t max_y = (uint32_t)((y1 - 1 - c->world.y) / c->grid);

    uint32_t w = max_x - min_x + 1;
    uint32_t h = max_y - min_y + 1;
    *size = w * h;

    uint32_t *parts = malloc(sizeof(uint32_t) * *size);
    if (!parts) {
        *size = 0;
        return NULL;
    }
    uint32_t n = 0;
    for (uint32_t y = min_y; y <= max_y; ++y)
        for (uint32_t x = min_x; x <= max_x; ++x)
            parts[n++] = y * cols + x;
    return parts;
}
static inline solu_call_ex sgb_clear_collider(sgb_game *g, solu_val collider) {
    sgb_collision *c = &g->collision_data;
    sgb_collider *col = collider.dyn;

    uint32_t pcount = 0;
    uint32_t *parts = sgb_find_parts(g, col->rect, &pcount);
    if (!parts || !pcount) return solu_err(g->s, "Malformed collider");
    for (uint32_t i = 0; i < pcount; ++i) {
        sgb_partition *p = c->partitions + parts[i];
        for (uint16_t i = 0; i < p->count; ++i) {
            if (p->data[i]->id == col->id) {
                sgb_partition_delete(p, i);
                break;
            }
        }
    }
    free(parts);
    return solu_ok(collider);
}
static inline solu_call_ex sgb_update_collider(sgb_game *g, solu_val collider) {
    sgb_collision *c = &g->collision_data;
    sgb_collider *col = collider.dyn;
    uint32_t pcount = 0;
    uint32_t *parts = sgb_find_parts(g, col->rect, &pcount);
    for (uint32_t i = 0; i < pcount; ++i) {
        sgb_partition *p = c->partitions + parts[i];
        sgb_partition_push(p, collider.dyn);
    }
    free(parts);
    return solu_ok(collider);
}

void sgb_collider_delete(void *_c) {
    sgb_collider *c = _c;
    sgb_clear_collider(c->g, (solu_val){SOLU_TDYN, .dyn=c});
}

static bool sgb_collider_active(sgb_collider *c) {
    solu_dalloc *da = (solu_dalloc *)c - 1;
    solu_val e = solu_dobj_strget(da->metadata[SOLU_META_EXTEND].dyn, "active");
    return (c->enabled = e.tt == SOLU_TBOOL ? e.boolean : false);
}

solu_call_ex sgb_collider_new(solu_state *s) {
    solu_val rect = solu_get(s, 0);
    if (!solu_arrptype(rect, SOLU_TF64, 4))
        return solu_err(s, "arg rect expected obj[4:f64], found %s", solu_typename(rect).c_str);
    solu_dobj *arr = rect.dyn;
    sgb_game *g = *(sgb_game **)solu_capturec(s, 0).dyn;
    sgb_frect fr = {arr->array.data[0].f64, arr->array.data[1].f64, arr->array.data[2].f64, arr->array.data[3].f64};

    solu_val collider = solu_dnusr(s,
        sizeof(sgb_collider),
        "collider",
        &(sgb_collider){g, g->id_c++, fr, false, true},
        sgb_collider_delete,
        NULL
    );
    solu_val extend = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(extend.dyn, "creator", g->ocall);
    solu_dobj_strset(extend.dyn, "active", SOLU_TRUE);
    solu_dobj_strset(extend.dyn, "move", solu_wrapmfun(s, sgb_collider_move, 1, &g->gptr, 1));
    solu_dobj_strset(extend.dyn, "resize", solu_wrapmfun(s, sgb_collider_resize, 1, &g->gptr, 1));
    solu_dobj_strset(extend.dyn, "check", solu_wrapmfun(s, sgb_collider_check, 0, &g->gptr, 1));
    solu_dobj_strset(extend.dyn, "check_all", solu_wrapmfun(s, sgb_collider_check_all, 0, &g->gptr, 1));
    solu_dobj_strset(extend.dyn, "check_type", solu_wrapmfun(s, sgb_collider_check_all, 0, &g->gptr, 1));
    solu_dobj_strset(extend.dyn, "draw", solu_wrapmfun(s, sgb_collider_draw, 0, &g->gptr, 1));
    solu_dheader(collider)->metadata[SOLU_META_EXTEND] = extend;
    return sgb_update_collider(g, collider);
}

solu_call_ex sgb_collider_draw(solu_state *s) {
    solu_val collider = solu_capturec(s, 0);
    if (!solu_isutype(collider, sf_lit("collider")))
        return solu_err(s, "arg collider expected collider, found %s", solu_typename(collider).c_str);

    sgb_game *g = *(sgb_game **)solu_capturec(s, 1).dyn;
    sgb_collider *c = collider.dyn;

    int yofs = g->gui ? 0 : (int)-g->camera.y;
    int xofs = g->gui ? 0 : (int)-g->camera.x;

    SDL_SetRenderDrawBlendMode(g->ren, SDL_BLENDMODE_BLEND);
    uint32_t pcount = 0;
    uint32_t *parts = sgb_find_parts(g, c->rect, &pcount);
    if (parts) {
        uint32_t cols = (uint32_t)(
            (g->collision_data.world.width + g->collision_data.grid - 1) /
            g->collision_data.grid
        );
        SDL_SetRenderDrawColor(g->ren, 120, 120, 255, 175);
        for (uint32_t i = 0; i < pcount; ++i) {
            int y = (int)(parts[i] / cols);
            int x = (int)(parts[i] % cols);
            SDL_Rect r = {
                (int)g->collision_data.world.x + x * (int)g->collision_data.grid + xofs,
                (int)g->collision_data.world.y + y * (int)g->collision_data.grid + yofs,
                (int)g->collision_data.grid,
                (int)g->collision_data.grid
            };
            SDL_RenderFillRect(g->ren, &r);
        }
        free(parts);
    }

    if (sgb_collider_active(c)) SDL_SetRenderDrawColor(g->ren, 255, 0, 0, 175);
    else SDL_SetRenderDrawColor(g->ren, 95, 95, 95, 175);

    SDL_Rect r = {
        (int)c->rect.x + xofs,
        (int)c->rect.y + yofs,
        (int)c->rect.width,
        (int)c->rect.height
    };
    SDL_RenderFillRect(g->ren, &r);

    return solu_ok(SOLU_NIL);
}

solu_call_ex sgb_collider_move(solu_state *s) {
    solu_val collider = solu_selfc(s);
    if (!solu_isutype(collider, sf_lit("collider")))
        return solu_err(s, "arg collider expected collider, found %s", solu_typename(collider).c_str);
    sgb_game *g = *(sgb_game **)solu_capturec(s, 1).dyn;
    solu_val rect = solu_get(s, 1);
    if (!solu_arrptype(rect, SOLU_TF64, 2))
        return solu_err(s, "arg rect expected obj[2:f64], found %s", solu_typename(rect).c_str);
    solu_dobj *arr = rect.dyn;

    sgb_clear_collider(g, collider);
    sgb_collider *c = collider.dyn;
    c->rect = (sgb_frect){
        arr->array.data[0].f64, arr->array.data[1].f64,
        c->rect.width, c->rect.height
    };
    return sgb_update_collider(g, collider);
}
solu_call_ex sgb_collider_resize(solu_state *s) {
    solu_val collider = solu_selfc(s);
    if (!solu_isutype(collider, sf_lit("collider")))
        return solu_err(s, "arg collider expected collider, found %s", solu_typename(collider).c_str);
    sgb_game *g = *(sgb_game **)solu_capturec(s, 1).dyn;
    solu_val rect = solu_get(s, 1);
    if (!solu_arrptype(rect, SOLU_TF64, 2))
        return solu_err(s, "arg rect expected obj[2:f64], found %s", solu_typename(rect).c_str);
    solu_dobj *arr = rect.dyn;

    sgb_clear_collider(g, collider);
    sgb_collider *c = collider.dyn;
    c->rect = (sgb_frect){
        c->rect.x, c->rect.y,
        arr->array.data[0].f64, arr->array.data[1].f64
    };
    return sgb_update_collider(g, collider);
}

solu_call_ex sgb_collider_check(solu_state *s) {
    solu_val collider = solu_selfc(s);
    if (!solu_isutype(collider, sf_lit("collider")))
        return solu_err(s, "arg collider expected collider, found %s", solu_typename(collider).c_str);
    sgb_game *g = *(sgb_game **)solu_capturec(s, 1).dyn;
    sgb_collision *c = &g->collision_data;
    sgb_collider *col = collider.dyn;
    sgb_frect a = col->rect;

    uint32_t pcount = 0;
    uint32_t *parts = sgb_find_parts(g, col->rect, &pcount);
    for (uint32_t i = 0; i < pcount; ++i) {
        sgb_partition *p = c->partitions + parts[i];
        for (uint16_t i = 0; i < p->count; ++i) {
            sgb_collider *c = *(p->data + i);
            sgb_frect b = c->rect;
            if (a.x <= b.x + b.width
            &&  a.x + a.width >= b.x
            &&  a.y <= b.y + b.height
            &&  a.y + a.height >= b.y
            &&  c->id != col->id
            &&  sgb_collider_active(c)) {
                free(parts);
                return solu_ok((solu_val){SOLU_TDYN, .dyn=*(p->data + i)});
            }
        }
    }
    free(parts);
    return solu_ok(SOLU_NIL);
}

solu_call_ex sgb_collider_check_all(solu_state *s) {
    solu_val collider = solu_selfc(s);
    if (!solu_isutype(collider, sf_lit("collider")))
        return solu_err(s, "arg collider expected collider, found %s", solu_typename(collider).c_str);
    sgb_game *g = *(sgb_game **)solu_capturec(s, 1).dyn;
    sgb_collision *c = &g->collision_data;
    sgb_collider *col = collider.dyn;
    sgb_frect a = col->rect;

    solu_val obj = solu_dnew(s, SOLU_DOBJ);
    solu_dobj *d = obj.dyn;
    uint32_t pcount = 0;
    uint32_t *parts = sgb_find_parts(g, col->rect, &pcount);
    for (uint32_t i = 0; i < pcount; ++i) {
        sgb_partition *p = c->partitions + parts[i];
        for (uint16_t i = 0; i < p->count; ++i) {
            sgb_collider *c = *(p->data + i);
            sgb_frect b = c->rect;
            if (a.x <= b.x + b.width
            &&  a.x + a.width >= b.x
            &&  a.y <= b.y + b.height
            &&  a.y + a.height >= b.y
            &&  c->id != col->id && !c->seen) {
                if (sgb_collider_active(c))
                    solu_valvec_push(&d->array, (solu_val){SOLU_TDYN, .dyn=*(p->data + i)});
                c->seen = true;
            }
        }
    }
    free(parts);
    for (uint32_t i = 0; i < d->array.count; ++i)
        ((sgb_collider *)d->array.data[i].dyn)->seen = false;
    return solu_ok(obj);
}

solu_call_ex sgb_collider_check_type(solu_state *s) {
    solu_val collider = solu_selfc(s);
    if (!solu_isutype(collider, sf_lit("collider")))
        return solu_err(s, "arg collider expected collider, found %s", solu_typename(collider).c_str);
    solu_val type = solu_get(s, 0);
    if (!solu_isdtype(type, SOLU_DSTR))
        return solu_err(s, "arg type expected str, found %s", solu_typename(collider).c_str);
    sgb_game *g = *(sgb_game **)solu_capturec(s, 1).dyn;
    sgb_collision *c = &g->collision_data;
    sgb_collider *col = collider.dyn;
    sgb_frect a = col->rect;

    uint32_t pcount = 0;
    uint32_t *parts = sgb_find_parts(g, col->rect, &pcount);
    for (uint32_t i = 0; i < pcount; ++i) {
        sgb_partition *p = c->partitions + parts[i];
        for (uint16_t i = 0; i < p->count; ++i) {
            sgb_collider *c = *(p->data + i);
            solu_dalloc *da = (solu_dalloc *)c - 1;
            sgb_frect b = c->rect;
            if (a.x <= b.x + b.width  &&
                a.x + a.width >= b.x  &&
                a.y <= b.y + b.height &&
                a.y + a.height >= b.y &&
                c->id != col->id && !c->seen) {
                solu_val creator = solu_dobj_strget(da->metadata[SOLU_META_EXTEND].dyn, "creator");
                if (sgb_collider_active(c)
                &&  solu_isdtype(creator, SOLU_DOBJ)
                &&  solu_streq(type, solu_dobj_strget(creator.dyn, "type"))) {
                    free(parts);
                    return solu_ok((solu_val){SOLU_TDYN, .dyn=c});
                }
            }
        }
    }
    free(parts);
    return solu_ok(SOLU_NIL);
}
