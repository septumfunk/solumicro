#include "../api.h"

static void smc_sfx_delete(void *_sfx) {
    smc_sounddata *sfx = *(smc_sounddata **)_sfx;
    if (!sfx) return;

    if (sfx->tt == SMC_MUSIC) {
        Mix_HaltMusic();
        if (sfx->g)
            solu_valmap_delete(&((smc_game *)sfx->g)->mus_cache, sfx->name);
        if (sfx->music)
            Mix_FreeMusic(sfx->music);
    } else {
        if (sfx->channel >= 0)
            Mix_HaltChannel(sfx->channel);
        if (sfx->sound)
            Mix_FreeChunk(sfx->sound);
    }
    smc_info(
        "Unloaded %s '%s'.",
        sfx->tt == SMC_MUSIC ? "music" : "sound",
        sfx->name.c_str
    );
    sf_str_free(sfx->name);
    free(sfx);
}

static solu_call_ex smc_gwrap(solu_state *s) {
    return solu_ok(solu_dobj_get(s, solu_capturec(s, 0).dyn, solu_get(s, 0)));
}

solu_call_ex smc_load_sound(solu_state *s) {
    solu_val name = solu_get(s, 0);
    if (!solu_isdtype(name, SOLU_DSTR))
        return solu_err(s, "arg 'name' expected str got %s", solu_typename(name).c_str);

    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    smc_snd_ex ex = smc_open_sound(s, g->snd_dir, name.dyn);
    if (!ex.is_ok) {
        solu_call_ex res = solu_panic(s, "%s", ex.err.c_str);
        sf_str_free(ex.err);
        return res;
    }
    smc_sounddata *sfx = malloc(sizeof(smc_sounddata));
    *sfx = ex.ok;
    sfx->g = g;

    solu_val info = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(info.dyn, "name", solu_dnstr(s, sfx->name.c_str));
    solu_dobj_strset(info.dyn, "volume", (solu_val){SOLU_TF64, .f64=1.0});

    solu_val out = solu_dnusr(s, sizeof(smc_sounddata *), "sfx", &sfx, smc_sfx_delete, NULL);
    solu_dalloc *usr = solu_dheader(out);
    solu_dalloc *infod = solu_dheader(info);
    infod->meta = solu_dnew(s, SOLU_DOBJ);
    infod->metadata[SOLU_META_EXTEND] = g->snd;
    usr->metadata[SOLU_META_EXTEND] = info;

    solu_val set = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(set.dyn, "volume", solu_wrapmfun(s, smc_snd_volume, 1, NULL, 0));

    solu_dobj_strset(info.dyn, "set", set);
    usr->metadata[SOLU_META_SET] = solu_wrapcfun(s, smc_set, 3, (solu_val[]){out, g->gptr}, 2);
    return solu_ok(out);
}

solu_call_ex smc_load_music(solu_state *s) {
    solu_val name = solu_get(s, 0);
    if (!solu_isdtype(name, SOLU_DSTR))
        return solu_err(s, "arg 'name' expected str got %s", solu_typename(name).c_str);

    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    solu_valmap_ex exists = solu_valmap_get(&g->mus_cache, sf_ref(name.dyn));
    if (exists.is_ok && solu_isutype(exists.ok, sf_lit("mus")))
        return solu_ok(exists.ok);

    smc_snd_ex ex = smc_open_music(s, g->snd_dir, name.dyn);
    if (!ex.is_ok) {
        if (!ex.is_ok) {
            solu_call_ex res = solu_panic(s, "%s", ex.err.c_str);
            sf_str_free(ex.err);
            return res;
        }
    }
    smc_sounddata *sfx = malloc(sizeof(smc_sounddata));
    *sfx = ex.ok;
    sfx->g = g;

    solu_val info = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(info.dyn, "name", solu_dnstr(s, sfx->name.c_str));
    double len = Mix_MusicDuration(ex.ok.music);
    len = len < 0 ? 0 : len;
    solu_dobj_strset(info.dyn, "len", (solu_val){SOLU_TF64, .f64 = len});
    solu_dobj_strset(info.dyn, "volume", (solu_val){SOLU_TF64, .f64=ex.ok.default_volume});
    solu_dobj_strset(info.dyn, "loop", (solu_val){SOLU_TBOOL, .boolean=true});
    sfx->loop = true;

    solu_val out = solu_dnusr(s, sizeof(smc_spritedata *), "mus", &sfx, smc_sfx_delete, NULL);
    solu_dalloc *usr = solu_dheader(out);
    solu_dalloc *infod = solu_dheader(info);
    infod->metadata[SOLU_META_EXTEND] = g->snd;
    usr->metadata[SOLU_META_EXTEND] = info;

    solu_val set = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(set.dyn, "volume", solu_wrapmfun(s, smc_snd_volume, 1, NULL, 0));
    solu_dobj_strset(set.dyn, "loop", solu_wrapmfun(s, smc_snd_loop, 1, NULL, 0));

    solu_dobj_strset(info.dyn, "set", set);
    usr->metadata[SOLU_META_SET] = solu_wrapcfun(s, smc_set, 3, (solu_val[]){out, g->gptr}, 2);

    solu_valmap_set(&g->mus_cache, sf_str_cdup(name.dyn), out);
    return solu_ok(out);
}

solu_call_ex smc_snd_play(solu_state *s) {
    solu_val sfx = solu_selfc(s);
    if (!solu_isutype(sfx, sf_lit("sfx")) && !solu_isutype(sfx, sf_lit("mus")))
        return solu_panic(s, "'self' expected sfx|mus got %s", solu_typename(sfx).c_str);
    if (!Mix_QuerySpec(NULL, NULL, NULL))
        return solu_panic(s, "Audio device is not ready!");
    smc_sounddata *snd = *(smc_sounddata **)sfx.dyn;

    solu_val volume = solu_dobj_strget(solu_dheader(sfx)->metadata[SOLU_META_EXTEND].dyn, "volume");
    double vol = volume.tt == SOLU_TF64 ? volume.f64 : snd->default_volume;
    if (vol < 0) vol = 0;
    if (vol > 1) vol = 1;

    if (snd->tt == SMC_MUSIC) {
        Mix_VolumeMusic((int)(vol * MIX_MAX_VOLUME));
        if (Mix_PlayMusic(snd->music, snd->loop ? -1 : 1) < 0)
            return solu_panic(s, "Failed to play music: %s", Mix_GetError());
    } else {
        Mix_VolumeChunk(snd->sound, (int)(vol * MIX_MAX_VOLUME));
        snd->channel = Mix_PlayChannel(-1, snd->sound, 0);
        if (snd->channel < 0)
            return solu_panic(s, "Failed to play sound: %s", Mix_GetError());
    }
    return solu_ok(SOLU_NIL);
}

solu_call_ex smc_snd_is_playing(solu_state *s) {
    solu_val sfx = solu_selfc(s);
    if (!solu_isutype(sfx, sf_lit("sfx")) && !solu_isutype(sfx, sf_lit("mus")))
        return solu_panic(s, "'self' expected sfx|mus got %s", solu_typename(sfx).c_str);

    smc_sounddata *snd = *(smc_sounddata **)sfx.dyn;
    bool playing;
    if (snd->tt == SMC_MUSIC) {
        playing = Mix_PlayingMusic() != 0;
    } else {
        playing = snd->channel >= 0 && Mix_Playing(snd->channel) != 0;
        if (!playing)
            snd->channel = -1;
    }
    return solu_ok((solu_val){
        SOLU_TBOOL,
        .boolean = playing
    });
}

solu_call_ex smc_snd_stop(solu_state *s) {
    solu_val sfx = solu_selfc(s);
    if (!solu_isutype(sfx, sf_lit("sfx")) && !solu_isutype(sfx, sf_lit("mus")))
        return solu_panic(s, "'self' expected sfx|mus got %s", solu_typename(sfx).c_str);
    smc_sounddata *snd = *(smc_sounddata **)sfx.dyn;
    if (snd->tt == SMC_MUSIC)
        Mix_HaltMusic();
    else if (snd->channel >= 0) {
        Mix_HaltChannel(snd->channel);
        snd->channel = -1;
    }
    return solu_ok(SOLU_NIL);
}

static bool smc_num01(solu_val v, double *out) {
    if (v.tt == SOLU_TF64) {
        *out = v.f64;
        return true;
    }
    if (v.tt == SOLU_TI64) {
        *out = (double)v.i64;
        return true;
    }
    return false;
}

solu_call_ex smc_snd_volume(solu_state *s) {
    solu_val sfx = solu_selfc(s);
    if (!solu_isutype(sfx, sf_lit("sfx")) && !solu_isutype(sfx, sf_lit("mus")))
        return solu_panic(s, "'self' expected sfx|mus got %s", solu_typename(sfx).c_str);

    solu_val volume = solu_get(s, 0);
    double v;
    if (!smc_num01(volume, &v))
        return solu_panic(s, "arg 'volume' expected i64|f64 got %s", solu_typename(volume).c_str);

    if (v < 0) v = 0;
    if (v > 1) v = 1;
    int iv = (int)(v * MIX_MAX_VOLUME);

    smc_sounddata *snd = *(smc_sounddata **)sfx.dyn;
    if (snd->tt == SMC_MUSIC) {
        Mix_VolumeMusic(iv);
    } else {
        Mix_VolumeChunk(snd->sound, iv);
        if (snd->channel >= 0)
            Mix_Volume(snd->channel, iv);
    }

    solu_dobj_strset(solu_dheader(sfx)->metadata[SOLU_META_EXTEND].dyn, "volume", (solu_val){SOLU_TF64, .f64 = v});
    return solu_ok(SOLU_NIL);
}

solu_call_ex smc_snd_loop(solu_state *s) {
    solu_val sfx = solu_selfc(s);
    if (!solu_isutype(sfx, sf_lit("sfx")) && !solu_isutype(sfx, sf_lit("mus")))
        return solu_panic(s, "'self' expected sfx|mus got %s", solu_typename(sfx).c_str);
    solu_val loop = solu_get(s, 0);
    if (loop.tt != SOLU_TBOOL)
        return solu_panic(s, "arg 'loop' expectected bool got %s", solu_typename(loop).c_str);
    smc_sounddata *snd = *(smc_sounddata **)sfx.dyn;
    snd->loop = loop.boolean;
    solu_dobj_strset(solu_dheader(sfx)->metadata[SOLU_META_EXTEND].dyn, "loop", loop);
    return solu_ok(SOLU_NIL);
}
