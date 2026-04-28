#ifndef DESKTOP_H
#define DESKTOP_H

#include <errno.h>
#include <sf/math.h>
#include "platforms.h"
#include "solus/compat.h"

bool SGB_READONLY = false;

char *sgb_platform_string(void) { return "vita"; }

sf_vec2 sgb_platform_screensize(sf_vec2 res, float scale) {
    return (sf_vec2){res.x * scale, res.y * scale};
}

void sgb_platform_init(sgb_platformdata *p) {
    (void)p;
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

    printf("%s", _fmt);
    free(_fmt);
}

// Controller

void sgb_controller_poll(sgb_platformdata *p) { (void)p; }

bool sgb_controller_held(sgb_platformdata *p, sgb_controller btn) {
    (void)p; (void)btn;
    return false;
}

bool sgb_controller_pressed(sgb_platformdata *p, sgb_controller btn) {
    (void)p; (void)btn;
    return false;
}

bool sgb_controller_released(sgb_platformdata *p, sgb_controller btn) {
    (void)p; (void)btn;
    return false;
}

sf_vec2 sgb_controller_axis(sgb_platformdata *p, sgb_analog stick) {
    (void)p; (void)stick;
    return (sf_vec2){0, 0};
}

char *sgb_locate_manifest(void) {
    return strdup("manifest.solu"); // Automatically located
}

#if defined(_WIN32) || defined(_WIN64)
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #define MKDIR(path) mkdir(path, 0700)
#endif
char *sgb_make_dir(char *path) {
    char *p = solu_realpath(path);
    if (!p) {
        int status = MKDIR(path);
        if (status || !(p = solu_realpath(path)))
            sgb_err("Failed to create objects dir: %s", strerror(errno));
    }
    return p;
}

// The same on desktop because it can create files locally
char *sgb_data_dir(char *path) {
    char *p = solu_realpath(path);
    if (!p) {
        int status = MKDIR(path);
        if (status || !(p = solu_realpath(path)))
            sgb_err("Failed to create objects dir: %s", strerror(errno));
    }
    return p;
}

#endif // DESKTOP_H
