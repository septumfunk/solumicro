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

extern bool SMC_READONLY;

typedef enum {
    SMC_PLATFORM_DESKTOP,
    SMC_PLATFORM_VITA,
} smc_platform;

typedef struct {
    smc_platform id;
#ifdef __vita__
    uint32_t ctrl_held;
    uint32_t ctrl_pressed;
    uint32_t ctrl_released;
    smc_point ctrl_left, ctrl_right;
#else // Desktop
#endif
} smc_platformdata;

char *smc_platform_string(void);
sf_vec2 smc_platform_screensize(sf_vec2 resolution, float scale);

void smc_platform_init(smc_platformdata *platform);

void smc_log_format(char *fmt, ...);
#define smc_info(fmt, ...) (smc_log_format(TUI_INFO fmt TUI_CLEAR "\n", __VA_ARGS__))
#define smc_err(fmt, ...) (smc_log_format(TUI_ERR fmt TUI_CLEAR "\n", __VA_ARGS__))
#define smc_check(fmt) (smc_log_format(TUI_INFO fmt " %s:%d\n" TUI_CLEAR, __FILE__, __LINE__))

// Controller

typedef enum {
#ifdef __vita__
    SMC_CTRL_A      = SCE_CTRL_CROSS,
    SMC_CTRL_B      = SCE_CTRL_CIRCLE,
    SMC_CTRL_X      = SCE_CTRL_SQUARE,
    SMC_CTRL_Y      = SCE_CTRL_TRIANGLE,

    SMC_CTRL_L1     = SCE_CTRL_LTRIGGER,
    SMC_CTRL_R1     = SCE_CTRL_RTRIGGER,

    SMC_CTRL_SELECT = SCE_CTRL_SELECT,
    SMC_CTRL_START  = SCE_CTRL_START,

    SMC_CTRL_UP     = SCE_CTRL_UP,
    SMC_CTRL_DOWN   = SCE_CTRL_DOWN,
    SMC_CTRL_LEFT   = SCE_CTRL_LEFT,
    SMC_CTRL_RIGHT  = SCE_CTRL_RIGHT,
#else
    SMC_CTRL_A      = SDL_CONTROLLER_BUTTON_A,
    SMC_CTRL_B      = SDL_CONTROLLER_BUTTON_B,
    SMC_CTRL_X      = SDL_CONTROLLER_BUTTON_X,
    SMC_CTRL_Y      = SDL_CONTROLLER_BUTTON_Y,

    SMC_CTRL_L1     = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SMC_CTRL_R1     = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,

    SMC_CTRL_SELECT = SDL_CONTROLLER_BUTTON_BACK,
    SMC_CTRL_START  = SDL_CONTROLLER_BUTTON_START,

    SMC_CTRL_UP     = SDL_CONTROLLER_BUTTON_DPAD_UP,
    SMC_CTRL_DOWN   = SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SMC_CTRL_LEFT   = SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SMC_CTRL_RIGHT  = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
#endif
} smc_controller;

typedef enum {
    SMC_LEFT_STICK,
    SMC_RIGHT_STICK,
} smc_analog;

void smc_controller_poll(smc_platformdata *p);

bool smc_controller_held(smc_platformdata *p, smc_controller btn);
bool smc_controller_pressed(smc_platformdata *p, smc_controller btn);
bool smc_controller_released(smc_platformdata *p, smc_controller btn);
sf_vec2 smc_controller_axis(smc_platformdata *p, smc_analog stick);

char *smc_locate_manifest(void);

char *smc_make_dir(char *path);
char *smc_data_dir(char *path);

#endif