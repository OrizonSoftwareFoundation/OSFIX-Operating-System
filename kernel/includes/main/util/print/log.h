/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#ifndef LOG_INFO_H
#define LOG_INFO_H

#include "serial.h"
#include "kernel/main/terminal/src/flanterm.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

typedef enum result_t {
    Ok,          

    Info,        

    Warn,        

    Fatal,       

    ResultCount  

} result_t;

extern const char* result_str[ResultCount];

#define COLOR_RED           "\033[31m"
#define COLOR_GREEN         "\033[32m"
#define COLOR_YELLOW        "\033[33m"
#define COLOR_BLUE          "\033[34m"
#define COLOR_MAGENTA       "\033[35m"
#define COLOR_CYAN          "\033[36m"
#define COLOR_WHITE         "\033[37m"
#define COLOR_GRAY          "\033[90m"
#define COLOR_LIGHTRED      "\033[91m"
#define COLOR_LIGHTGREEN    "\033[92m"
#define COLOR_LIGHTYELLOW   "\033[93m"
#define COLOR_LIGHTBLUE     "\033[94m"
#define COLOR_LIGHTMAGENTA  "\033[95m"
#define COLOR_LIGHTCYAN     "\033[96m"
#define COLOR_LIGHTGRAY     "\033[97m"
#define COLOR_BG_BLACK      "\033[40m"
#define COLOR_BG_RED        "\033[41m"
#define COLOR_BG_GREEN      "\033[42m"
#define COLOR_BG_YELLOW     "\033[43m"
#define COLOR_BOLD          "\033[1m"
#define COLOR_DIM           "\033[2m"
#define COLOR_UNDERLINE     "\033[4m"
#define COLOR_BLINK         "\033[5m"
#define COLOR_REVERSE       "\033[7m"
#define COLOR_RESET         "\033[0m"

static inline const char* get_status_color(result_t status) {
    switch (status) {
        case Ok:    return COLOR_GREEN;
        case Info:  return COLOR_BOLD;
        case Warn:  return COLOR_YELLOW;
        case Fatal: return COLOR_RED;
        default:    return "";
    }
}

void log_to_terminal(result_t status, const char *from, const char *file, int line, const char *fmt, ...);

#define LOG_OK(fmt, ...) log_to_terminal(Ok, __func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) log_to_terminal(Info, __func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) log_to_terminal(Warn, __func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) log_to_terminal(Fatal, __func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define SERIAL(status, from, fmt, ...) \
    serial_printf("%s in %s at line %d: " fmt "", \
                  #from, __FILE__, __LINE__, ##__VA_ARGS__)

void printcol(const char *color, const char *text);

#define LOG_BOTH(fmt, ...) \
    do { \
        kprintf(fmt, ##__VA_ARGS__); \
        serial_printf(fmt, ##__VA_ARGS__); \
    } while(0)

#endif
