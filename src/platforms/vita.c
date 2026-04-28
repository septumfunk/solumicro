#ifndef VITA_H
#define VITA_H

#include <errno.h>
#include <solus/val.h>
#include <sf/math.h>
#include <sys/stat.h>
#include "platforms.h"
#include "solus/compat.h"

#define DEBUG_IP "10.0.0.224"

bool SGB_READONLY = true;

char *sgb_platform_string(void) { return "vita"; }

sf_vec2 sgb_platform_screensize(sf_vec2 res, float scale) {
    (void)res; (void)scale;
    return (sf_vec2){960, 544};
}

void sgb_platform_init(sgb_platformdata *p) {
    (void)p;
    debugNetInit(DEBUG_IP, 18194, ERROR);
    sgb_info("Initialized <debugnet>", NULL);
}

void sgb_log_format(char *fmt, ...) {
    va_list arglist;

    va_start(arglist, fmt);
    const size_t size =
        (size_t)vsnprintf(NULL, 0, fmt, arglist);
    va_end(arglist);

    char *_fmt = calloc(1, size + 1);
    va_start(arglist, fmt);
    vsnprintf(_fmt, size + 1, fmt, arglist);
    va_end(arglist);

    debugNetPrintf(ERROR, "%s", _fmt);
    free(_fmt);
}

// Controller

void sgb_controller_poll(sgb_platformdata *p) {
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceCtrlPeekBufferPositive(0, &pad, 1);

    uint32_t old = p->ctrl_held;
    uint32_t now = pad.buttons;

    p->ctrl_held = now;
    p->ctrl_pressed = now & ~old;
    p->ctrl_released = old & ~now;

    p->ctrl_left = (sgb_point){pad.lx, pad.ly};
    p->ctrl_right = (sgb_point){pad.rx, pad.ry};
}

float sgb_vita_axis(uint8_t v) {
    float out = ((float)v - 128.0f) / 127.0f;
    if (out < -1.0) out = -1.0;
    if (out >  1.0) out =  1.0;
    if (out > -0.08 && out < 0.08)
        out = 0.0;
    return out;
}

bool sgb_controller_held(sgb_platformdata *p, sgb_controller btn) {
    uint32_t b = (uint32_t)btn;
    return (p->ctrl_held & b) != 0;
}

bool sgb_controller_pressed(sgb_platformdata *p, sgb_controller btn) {
    uint32_t b = (uint32_t)btn;
    return (p->ctrl_pressed & b) != 0;
}

bool sgb_controller_released(sgb_platformdata *p, sgb_controller btn) {
    uint32_t b = (uint32_t)btn;
    return (p->ctrl_released & b) != 0;
}

sf_vec2 sgb_controller_axis(sgb_platformdata *p, sgb_analog stick) {
    float x = 0.0;
    float y = 0.0;
    if (stick == SGB_LEFT_STICK) {
        x = sgb_vita_axis((uint8_t)p->ctrl_left.x);
        y = sgb_vita_axis((uint8_t)p->ctrl_left.y);
    } else if (stick == SGB_RIGHT_STICK) {
        x = sgb_vita_axis((uint8_t)p->ctrl_right.x);
        y = sgb_vita_axis((uint8_t)p->ctrl_right.y);
    } else {
        return (sf_vec2){0, 0};
    }
    return (sf_vec2){x, y};
}

char *sgb_locate_manifest(void) {
    return strdup("app0:/manifest.solu");
}

char *sgb_make_dir(char *path) {
    char *p = sf_str_fmt("ux0:/data/" VITA_TITLEID "/%s", path).c_str;
    if (!sf_file_exists(sf_ref(p))) {
        int status = mkdir(p, 0700);
        if (status || !sf_file_exists(sf_ref(p)))
            sgb_err("Failed to create dir: %s", strerror(errno));
    }
    return p;
}

char *sgb_data_dir(char *path) {
    char *p = sf_str_fmt("app0:/%s", path).c_str;
    if (!sf_file_exists(sf_ref(p))) {
        sgb_err("Unable to create dir %s. app0 is readonly!", p);
        free(p);
        return NULL;
    }
    return p;
}

#endif // VITA_H
