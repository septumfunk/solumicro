#ifndef CONFIG_H
#define CONFIG_H

#include "game.h"

extern const char SGB_DEFAULT_CONFIG[];
extern const char SGB_DEFAULT_ROOM[];

typedef struct {
    Texture2D texture;
    bool drawn;
} sgb_sprite;

solu_val sgb_manifest_load(solu_state *state);
solu_val sgb_object_new(sgb_game *game, solu_i64 id, sf_str path);
void sgb_register(sgb_game *game);

solu_call_ex sgb_quit(solu_state *state);

solu_call_ex sgb_set(solu_state *state);
solu_call_ex sgb_noset(solu_state *state);

solu_call_ex sgb_set_room(solu_state *state);
solu_call_ex sgb_set_title(solu_state *state);

solu_call_ex sgb_draw_sprite(solu_state *state);
solu_call_ex sgb_draw_rect(solu_state *state);

solu_call_ex sgb_key_down(solu_state *state);
solu_call_ex sgb_key_pressed(solu_state *state);
solu_call_ex sgb_key_released(solu_state *state);

solu_val sgb_keys(solu_state *state);

#endif // CONFIG_H
