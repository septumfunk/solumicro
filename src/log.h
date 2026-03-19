#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
    #define TUI_CLEAR  "\x1b[0m"
    #define TUI_ERR    "\x1b[1;31m"  // bright red
    #define TUI_INFO   "\x1b[0;90m"  // gray
#else
    #define TUI_CLEAR  ""
    #define TUI_ERR    ""
    #define TUI_INFO   ""
#endif

void sgb_log_format(FILE *res, char *fmt, ...);
#define sgb_info(fmt, ...) (sgb_log_format(stdout, TUI_INFO fmt TUI_CLEAR "\n", __VA_ARGS__))
#define sgb_err(fmt, ...) (sgb_log_format(stderr, TUI_ERR fmt TUI_CLEAR "\n", __VA_ARGS__))

#endif // LOG_H
