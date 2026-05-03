#include "../api.h"
#include "solus/api.h"
#include <SDL2/SDL.h>

static int smc_mouse_btn(int btn) {
    switch (btn) {
        case 1: return SDL_BUTTON_LEFT;
        case 2: return SDL_BUTTON_MIDDLE;
        case 3: return SDL_BUTTON_RIGHT;
        default: return btn;
    }
}

solu_call_ex smc_key_held(solu_state *s) {
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);

    SDL_Scancode sc = (SDL_Scancode)key.i64;
    const uint8_t *state = SDL_GetKeyboardState(NULL);
    return solu_ok((solu_val){
        SOLU_TBOOL,
        .boolean = sc < SDL_NUM_SCANCODES && state[sc]
    });
}

solu_call_ex smc_key_pressed(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);

    SDL_Scancode sc = (SDL_Scancode)key.i64;
    return solu_ok((solu_val){
        SOLU_TBOOL,
        .boolean = sc < SDL_NUM_SCANCODES && g->keys_pressed[sc]
    });
}

solu_call_ex smc_key_released(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);

    SDL_Scancode sc = (SDL_Scancode)key.i64;
    return solu_ok((solu_val){
        SOLU_TBOOL,
        .boolean = sc < SDL_NUM_SCANCODES && g->keys_released[sc]
    });
}

solu_call_ex smc_key_string(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    return solu_ok(solu_dnstr(s, g->text_input));
}

solu_call_ex smc_mouse_held(solu_state *s) {
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);

    int btn = smc_mouse_btn((int)key.i64);
    uint32_t state = SDL_GetMouseState(NULL, NULL);
    return solu_ok((solu_val){
        SOLU_TBOOL,
        .boolean = btn > 0 && btn < 8 && (state & SDL_BUTTON(btn))
    });
}

solu_call_ex smc_mouse_pressed(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);

    int btn = smc_mouse_btn((int)key.i64);
    return solu_ok((solu_val){
        SOLU_TBOOL,
        .boolean = btn > 0 && btn < 8 && g->mouse_pressed[btn]
    });
}

solu_call_ex smc_mouse_released(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    solu_val key = solu_get(s, 0);
    if (key.tt != SOLU_TI64)
        return solu_err(s, "arg 'key' expected i64 got %s", solu_typename(key).c_str);

    int btn = smc_mouse_btn((int)key.i64);
    return solu_ok((solu_val){
        SOLU_TBOOL,
        .boolean = btn > 0 && btn < 8 && g->mouse_released[btn]
    });
}

solu_call_ex smc_mouse_wheel(solu_state *s) {
    smc_game *g = *(smc_game **)solu_capturec(s, 0).dyn;
    return solu_ok((solu_val){
        SOLU_TF64,
        .f64 = g->mouse_wheel
    });
}