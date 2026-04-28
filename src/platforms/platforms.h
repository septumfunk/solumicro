#ifndef PLATFORM_H
#define PLATFORM_H

#include <sf/math.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include "../asset.h"

#ifdef __vita__
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/ctrl.h>
#include <debugnet.h>
#endif

#ifndef _WIN32
    #define TUI_CLEAR  "\x1b[0m"
    #define TUI_ERR    "\x1b[1;31m"  // bright red
    #define TUI_INFO   "\x1b[0;90m"  // gray
#else
    #define TUI_CLEAR  ""
    #define TUI_ERR    ""
    #define TUI_INFO   ""
#endif

extern bool SGB_READONLY;

typedef enum {
    SGB_PLATFORM_DESKTOP,
    SGB_PLATFORM_VITA,
} sgb_platform;

typedef struct {
    sgb_platform id;
#ifdef __vita__
    uint32_t ctrl_held;
    uint32_t ctrl_pressed;
    uint32_t ctrl_released;
    sgb_point ctrl_left, ctrl_right;
#else // Desktop
#endif
} sgb_platformdata;

char *sgb_platform_string(void);
sf_vec2 sgb_platform_screensize(sf_vec2 resolution, float scale);

void sgb_platform_init(sgb_platformdata *platform);

void sgb_log_format(char *fmt, ...);
#define sgb_info(fmt, ...) (sgb_log_format(TUI_INFO fmt TUI_CLEAR "\n", __VA_ARGS__))
#define sgb_err(fmt, ...) (sgb_log_format(TUI_ERR fmt TUI_CLEAR "\n", __VA_ARGS__))
#define sgb_check(fmt) (sgb_log_format(TUI_INFO fmt " %s:%d\n" TUI_CLEAR, __FILE__, __LINE__))

// Controller

typedef enum {
#ifdef __vita__
    SGB_CTRL_A      = SCE_CTRL_CROSS,
    SGB_CTRL_B      = SCE_CTRL_CIRCLE,
    SGB_CTRL_X      = SCE_CTRL_SQUARE,
    SGB_CTRL_Y      = SCE_CTRL_TRIANGLE,

    SGB_CTRL_L1     = SCE_CTRL_LTRIGGER,
    SGB_CTRL_R1     = SCE_CTRL_RTRIGGER,

    SGB_CTRL_SELECT = SCE_CTRL_SELECT,
    SGB_CTRL_START  = SCE_CTRL_START,

    SGB_CTRL_UP     = SCE_CTRL_UP,
    SGB_CTRL_DOWN   = SCE_CTRL_DOWN,
    SGB_CTRL_LEFT   = SCE_CTRL_LEFT,
    SGB_CTRL_RIGHT  = SCE_CTRL_RIGHT,
#else
    SGB_CTRL_A      = SDL_CONTROLLER_BUTTON_A,
    SGB_CTRL_B      = SDL_CONTROLLER_BUTTON_B,
    SGB_CTRL_X      = SDL_CONTROLLER_BUTTON_X,
    SGB_CTRL_Y      = SDL_CONTROLLER_BUTTON_Y,

    SGB_CTRL_L1     = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SGB_CTRL_R1     = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,

    SGB_CTRL_SELECT = SDL_CONTROLLER_BUTTON_BACK,
    SGB_CTRL_START  = SDL_CONTROLLER_BUTTON_START,

    SGB_CTRL_UP     = SDL_CONTROLLER_BUTTON_DPAD_UP,
    SGB_CTRL_DOWN   = SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SGB_CTRL_LEFT   = SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SGB_CTRL_RIGHT  = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
#endif
} sgb_controller;

typedef enum {
    SGB_LEFT_STICK,
    SGB_RIGHT_STICK,
} sgb_analog;

void sgb_controller_poll(sgb_platformdata *p);

bool sgb_controller_held(sgb_platformdata *p, sgb_controller btn);
bool sgb_controller_pressed(sgb_platformdata *p, sgb_controller btn);
bool sgb_controller_released(sgb_platformdata *p, sgb_controller btn);
sf_vec2 sgb_controller_axis(sgb_platformdata *p, sgb_analog stick);

char *sgb_locate_manifest(void);

char *sgb_make_dir(char *path);
char *sgb_data_dir(char *path);

#endif