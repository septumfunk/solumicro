#ifndef API_H
#define API_H

#include "game.h"
#include <solus/api.h>

extern const char SMC_DEFAULT_CONFIG[];
extern const char SMC_DEFAULT_ROOM[];

typedef struct {
    SDL_Texture *texture;
    bool drawn;
} smc_sprite;

void smc_register(smc_game *game);
solu_val smc_keys(solu_state *state);
solu_val smc_mouse(solu_state *state);

// Setter helpers
solu_call_ex smc_set(solu_state *state);
solu_call_ex smc_noset(solu_state *state);

// State
solu_call_ex smc_quit(solu_state *state);
solu_call_ex smc_set_room(solu_state *state);
solu_call_ex smc_set_title(solu_state *state);
solu_call_ex smc_set_paused(solu_state *state);

solu_val smc_object_new(smc_game *game, solu_i64 id, sf_str path);
solu_call_ex smc_load_object(solu_state *state);
solu_call_ex smc_delete(solu_state *state);

// Graphics
solu_call_ex smc_load_sprite(solu_state *state);
solu_call_ex smc_draw_sprite(solu_state *state);
solu_call_ex smc_draw_rect(solu_state *state);

// Sound
solu_call_ex smc_load_sound(solu_state *state);
solu_call_ex smc_load_music(solu_state *state);
solu_call_ex smc_snd_play(solu_state *state);
solu_call_ex smc_snd_is_playing(solu_state *state);
solu_call_ex smc_snd_stop(solu_state *state);
solu_call_ex smc_snd_volume(solu_state *state);
solu_call_ex smc_snd_loop(solu_state *state);

// Kb/Mouse
solu_call_ex smc_key_held(solu_state *state);
solu_call_ex smc_key_pressed(solu_state *state);
solu_call_ex smc_key_released(solu_state *state);
solu_call_ex smc_key_string(solu_state *state);

solu_call_ex smc_mouse_held(solu_state *state);
solu_call_ex smc_mouse_pressed(solu_state *state);
solu_call_ex smc_mouse_released(solu_state *state);
solu_call_ex smc_mouse_wheel(solu_state *state);

// Controller
solu_call_ex smc_ctrl_held(solu_state *state);
solu_call_ex smc_ctrl_pressed(solu_state *state);
solu_call_ex smc_ctrl_released(solu_state *state);
solu_call_ex smc_ctrl_axis(solu_state *state);

solu_val smc_ctrl(solu_state *state);

// Collision
solu_call_ex smc_vec2(solu_state *state);
solu_call_ex smc_collider_new(solu_state *state);
solu_call_ex smc_collider_move(solu_state *state);
solu_call_ex smc_collider_resize(solu_state *state);
solu_call_ex smc_collider_check(solu_state *state);
solu_call_ex smc_collider_check_all(solu_state *state);
solu_call_ex smc_collider_check_type(solu_state *state);
solu_call_ex smc_collider_draw(solu_state *state);

#endif // API_H
