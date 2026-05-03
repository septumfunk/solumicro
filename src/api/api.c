#include "../api.h"

const char SMC_DEFAULT_ROOM[] = "{ \n\
    name = 'Room'\n\
\n\
    # Collision\n\
    size = {1024, 1024}\n\
    grid = 8\n\
\n\
    spawns = {\n\
        # 'foo',\n\
    }\n\
}";


void smc_register(smc_game *g) {
    solu_val load = solu_dnew(g->s, SOLU_DOBJ);
    solu_dobj_strset(load.dyn, "sprite", solu_wrapcfun(g->s, smc_load_sprite, 1, &g->gptr, 1));
    solu_dobj_strset(load.dyn, "sound", solu_wrapcfun(g->s, smc_load_sound, 1, &g->gptr, 1));
    solu_dobj_strset(load.dyn, "music", solu_wrapcfun(g->s, smc_load_music, 1, &g->gptr, 1));
    solu_dobj_strset(load.dyn, "object", solu_wrapcfun(g->s, smc_load_object, 2, &g->gptr, 1));

    solu_val draw = solu_dnew(g->s, SOLU_DOBJ);
    solu_dobj_strset(draw.dyn, "sprite", solu_wrapcfun(g->s, smc_draw_sprite, 7, &g->gptr, 1));
    solu_dobj_strset(draw.dyn, "rect", solu_wrapcfun(g->s, smc_draw_rect, 5, &g->gptr, 1));

    solu_val key = smc_keys(g->s);
    solu_dobj_strset(key.dyn, "held", solu_wrapcfun(g->s, smc_key_held, 1, &g->gptr, 1));
    solu_dobj_strset(key.dyn, "pressed", solu_wrapcfun(g->s, smc_key_pressed, 1, &g->gptr, 1));
    solu_dobj_strset(key.dyn, "released", solu_wrapcfun(g->s, smc_key_released, 1, &g->gptr, 1));
    solu_dobj_strset(key.dyn, "string", solu_wrapcfun(g->s, smc_key_string, 0, &g->gptr, 1));

    solu_val mouse = smc_mouse(g->s);
    solu_dobj_strset(mouse.dyn, "held", solu_wrapcfun(g->s, smc_mouse_held, 1, &g->gptr, 1));
    solu_dobj_strset(mouse.dyn, "pressed", solu_wrapcfun(g->s, smc_mouse_pressed, 1, &g->gptr, 1));
    solu_dobj_strset(mouse.dyn, "released", solu_wrapcfun(g->s, smc_mouse_released, 1, &g->gptr, 1));
    solu_dobj_strset(mouse.dyn, "wheel", solu_wrapcfun(g->s, smc_mouse_wheel, 0, &g->gptr, 1));

    solu_val ctrl = smc_ctrl(g->s);
    solu_dobj_strset(ctrl.dyn, "held", solu_wrapcfun(g->s, smc_ctrl_held, 1, &g->gptr, 1));
    solu_dobj_strset(ctrl.dyn, "pressed", solu_wrapcfun(g->s, smc_ctrl_pressed, 1, &g->gptr, 1));
    solu_dobj_strset(ctrl.dyn, "released", solu_wrapcfun(g->s, smc_ctrl_released, 1, &g->gptr, 1));
    solu_dobj_strset(ctrl.dyn, "axis", solu_wrapcfun(g->s, smc_ctrl_axis, 1, &g->gptr, 1));

    solu_val collider = solu_dnew(g->s, SOLU_DOBJ);
    solu_dobj_strset(collider.dyn, "new", solu_wrapcfun(g->s, smc_collider_new, 1, &g->gptr, 1));

    g->sprite = solu_dnew(g->s, SOLU_DOBJ);
    solu_dhold(g->sprite);
    solu_dobj_strset(g->sprite.dyn, "draw", solu_wrapmfun(g->s, smc_draw_sprite, 7, &g->gptr, 1));

    g->snd = solu_dnew(g->s, SOLU_DOBJ);
    solu_dhold(g->snd);
    solu_dobj_strset(g->snd.dyn, "play", solu_wrapmfun(g->s, smc_snd_play, 0, &g->gptr, 1));
    solu_dobj_strset(g->snd.dyn, "is_playing", solu_wrapmfun(g->s, smc_snd_is_playing, 0, &g->gptr, 1));
    solu_dobj_strset(g->snd.dyn, "stop", solu_wrapmfun(g->s, smc_snd_stop, 0, &g->gptr, 1));

    g->obj = solu_dnew(g->s, SOLU_DOBJ);
    solu_dhold(g->obj);
    solu_dobj_strset(g->obj.dyn, "delete", solu_wrapmfun(g->s, smc_delete, 1, &g->gptr, 1));

    // Globals
    solu_setg(g->s, "delete", solu_wrapmfun(g->s, smc_delete, 1, &g->gptr, 1));

    solu_setg(g->s, "load", load);
    solu_setg(g->s, "draw", draw);
    solu_setg(g->s, "key", key);
    solu_setg(g->s, "mouse", mouse);
    solu_setg(g->s, "ctrl", ctrl);
    solu_setg(g->s, "collider", collider);
}

#include <SDL2/SDL.h>

solu_val smc_keys(solu_state *s) {
    solu_val keys = solu_dnew(s, SOLU_DOBJ);

    // Letters
    solu_dobj_strset(keys.dyn, "a", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_A});
    solu_dobj_strset(keys.dyn, "b", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_B});
    solu_dobj_strset(keys.dyn, "c", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_C});
    solu_dobj_strset(keys.dyn, "d", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_D});
    solu_dobj_strset(keys.dyn, "e", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_E});
    solu_dobj_strset(keys.dyn, "f", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F});
    solu_dobj_strset(keys.dyn, "g", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_G});
    solu_dobj_strset(keys.dyn, "h", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_H});
    solu_dobj_strset(keys.dyn, "i", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_I});
    solu_dobj_strset(keys.dyn, "j", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_J});
    solu_dobj_strset(keys.dyn, "k", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_K});
    solu_dobj_strset(keys.dyn, "l", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_L});
    solu_dobj_strset(keys.dyn, "m", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_M});
    solu_dobj_strset(keys.dyn, "n", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_N});
    solu_dobj_strset(keys.dyn, "o", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_O});
    solu_dobj_strset(keys.dyn, "p", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_P});
    solu_dobj_strset(keys.dyn, "q", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_Q});
    solu_dobj_strset(keys.dyn, "r", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_R});
    solu_dobj_strset(keys.dyn, "s", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_S});
    solu_dobj_strset(keys.dyn, "t", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_T});
    solu_dobj_strset(keys.dyn, "u", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_U});
    solu_dobj_strset(keys.dyn, "v", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_V});
    solu_dobj_strset(keys.dyn, "w", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_W});
    solu_dobj_strset(keys.dyn, "x", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_X});
    solu_dobj_strset(keys.dyn, "y", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_Y});
    solu_dobj_strset(keys.dyn, "z", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_Z});

    // Numbers
    solu_dobj_strset(keys.dyn, "zero",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_0});
    solu_dobj_strset(keys.dyn, "one",   (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_1});
    solu_dobj_strset(keys.dyn, "two",   (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_2});
    solu_dobj_strset(keys.dyn, "three", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_3});
    solu_dobj_strset(keys.dyn, "four",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_4});
    solu_dobj_strset(keys.dyn, "five",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_5});
    solu_dobj_strset(keys.dyn, "six",   (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_6});
    solu_dobj_strset(keys.dyn, "seven", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_7});
    solu_dobj_strset(keys.dyn, "eight", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_8});
    solu_dobj_strset(keys.dyn, "nine",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_9});
    solu_dobj_strset(keys.dyn, "equal", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_EQUALS});
    solu_dobj_strset(keys.dyn, "minus", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_MINUS});

    // Function keys
    solu_dobj_strset(keys.dyn, "tilde", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_GRAVE});
    solu_dobj_strset(keys.dyn, "f1",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F1});
    solu_dobj_strset(keys.dyn, "f2",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F2});
    solu_dobj_strset(keys.dyn, "f3",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F3});
    solu_dobj_strset(keys.dyn, "f4",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F4});
    solu_dobj_strset(keys.dyn, "f5",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F5});
    solu_dobj_strset(keys.dyn, "f6",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F6});
    solu_dobj_strset(keys.dyn, "f7",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F7});
    solu_dobj_strset(keys.dyn, "f8",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F8});
    solu_dobj_strset(keys.dyn, "f9",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F9});
    solu_dobj_strset(keys.dyn, "f10", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F10});
    solu_dobj_strset(keys.dyn, "f11", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F11});
    solu_dobj_strset(keys.dyn, "f12", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_F12});

    // Controls
    solu_dobj_strset(keys.dyn, "space",     (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_SPACE});
    solu_dobj_strset(keys.dyn, "enter",     (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_RETURN});
    solu_dobj_strset(keys.dyn, "tab",       (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_TAB});
    solu_dobj_strset(keys.dyn, "backspace", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_BACKSPACE});
    solu_dobj_strset(keys.dyn, "escape",    (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_ESCAPE});
    solu_dobj_strset(keys.dyn, "insert",    (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_INSERT});
    solu_dobj_strset(keys.dyn, "delete",    (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_DELETE});
    solu_dobj_strset(keys.dyn, "home",      (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_HOME});
    solu_dobj_strset(keys.dyn, "end",       (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_END});
    solu_dobj_strset(keys.dyn, "page_up",   (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_PAGEUP});
    solu_dobj_strset(keys.dyn, "page_down", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_PAGEDOWN});

    // Arrows
    solu_dobj_strset(keys.dyn, "right", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_RIGHT});
    solu_dobj_strset(keys.dyn, "left",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_LEFT});
    solu_dobj_strset(keys.dyn, "down",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_DOWN});
    solu_dobj_strset(keys.dyn, "up",    (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_UP});

    // Modifiers
    solu_dobj_strset(keys.dyn, "left_shift",   (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_LSHIFT});
    solu_dobj_strset(keys.dyn, "right_shift",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_RSHIFT});
    solu_dobj_strset(keys.dyn, "left_control", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_LCTRL});
    solu_dobj_strset(keys.dyn, "right_control",(solu_val){SOLU_TI64, .i64=SDL_SCANCODE_RCTRL});
    solu_dobj_strset(keys.dyn, "left_alt",     (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_LALT});
    solu_dobj_strset(keys.dyn, "right_alt",    (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_RALT});
    solu_dobj_strset(keys.dyn, "left_super",   (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_LGUI});
    solu_dobj_strset(keys.dyn, "right_super",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_RGUI});
    solu_dobj_strset(keys.dyn, "caps_lock",    (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_CAPSLOCK});
    solu_dobj_strset(keys.dyn, "scroll_lock",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_SCROLLLOCK});
    solu_dobj_strset(keys.dyn, "num_lock",     (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_NUMLOCKCLEAR});
    solu_dobj_strset(keys.dyn, "print_screen", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_PRINTSCREEN});
    solu_dobj_strset(keys.dyn, "pause",        (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_PAUSE});

    // Keypad
    solu_dobj_strset(keys.dyn, "kp_0", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_0});
    solu_dobj_strset(keys.dyn, "kp_1", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_1});
    solu_dobj_strset(keys.dyn, "kp_2", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_2});
    solu_dobj_strset(keys.dyn, "kp_3", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_3});
    solu_dobj_strset(keys.dyn, "kp_4", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_4});
    solu_dobj_strset(keys.dyn, "kp_5", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_5});
    solu_dobj_strset(keys.dyn, "kp_6", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_6});
    solu_dobj_strset(keys.dyn, "kp_7", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_7});
    solu_dobj_strset(keys.dyn, "kp_8", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_8});
    solu_dobj_strset(keys.dyn, "kp_9", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_9});
    solu_dobj_strset(keys.dyn, "kp_decimal",  (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_PERIOD});
    solu_dobj_strset(keys.dyn, "kp_divide",   (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_DIVIDE});
    solu_dobj_strset(keys.dyn, "kp_multiply", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_MULTIPLY});
    solu_dobj_strset(keys.dyn, "kp_subtract", (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_MINUS});
    solu_dobj_strset(keys.dyn, "kp_add",      (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_PLUS});
    solu_dobj_strset(keys.dyn, "kp_enter",    (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_ENTER});
    solu_dobj_strset(keys.dyn, "kp_equal",    (solu_val){SOLU_TI64, .i64=SDL_SCANCODE_KP_EQUALS});

    return keys;
}

#include <SDL2/SDL.h>

solu_val smc_mouse(solu_state *s) {
    solu_val mouse = solu_dnew(s, SOLU_DOBJ);
    solu_dobj_strset(mouse.dyn, "left",   (solu_val){SOLU_TI64, .i64=SDL_BUTTON_LEFT});
    solu_dobj_strset(mouse.dyn, "middle", (solu_val){SOLU_TI64, .i64=SDL_BUTTON_MIDDLE});
    solu_dobj_strset(mouse.dyn, "right",  (solu_val){SOLU_TI64, .i64=SDL_BUTTON_RIGHT});
    solu_dobj_strset(mouse.dyn, "forward",(solu_val){SOLU_TI64, .i64=SDL_BUTTON_X2});
    solu_dobj_strset(mouse.dyn, "back",   (solu_val){SOLU_TI64, .i64=SDL_BUTTON_X1});
    return mouse;
}

solu_call_ex smc_set(solu_state *s) {
    solu_val key = solu_get(s, 1);
    solu_val val = solu_get(s, 2);
    if (!solu_isdtype(key, SOLU_DSTR))
        return solu_panic(s, "arg 'key' expected str got %s", solu_typename(key).c_str);

    solu_val cap = solu_capturec(s, 0);
    solu_dobj *set;
    solu_val f;
    if (solu_isdtype(cap, SOLU_DOBJ)) {
        solu_dobj *obj = cap.dyn;
        solu_dobj *m_obj = solu_dheader(cap)->meta.dyn;
        if (!obj || !m_obj) return solu_panic(s, "Game Corrupt");
        set = solu_dobj_strget(m_obj, "set").dyn;
        if (!set) return solu_panic(s, "Game Corrupt");
        f = solu_dobj_strget(set, key.dyn);
    } else {
        set = solu_dobj_strget(solu_dheader(cap)->metadata[SOLU_META_EXTEND].dyn, "set").dyn;
        if (!set) return solu_panic(s, "Game Corrupt");
        f = solu_dobj_strget(set, key.dyn);
        solu_fproto *fp = f.dyn;
        fp->upvals[0].value = cap;
    }

    if (!solu_isdtype(f, SOLU_DFUN))
        return solu_panic(s, "key '%s' not found", key.dyn);
    return solu_call(s, f.dyn, (solu_val[]){val}, 1);
}

solu_call_ex smc_noset(solu_state *s) {
    return solu_panic(s, "key is readonly");
}
