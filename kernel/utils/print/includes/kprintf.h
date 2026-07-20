//Yazin Tantawi
#ifndef KPRINTF_H
#define KPRINTF_H

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include "serial.h"
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
        case Ok:    return COLOR_GRAY;
        case Info:  return COLOR_GRAY;
        case Warn:  return COLOR_GRAY;
        case Fatal: return COLOR_GRAY;
        default:    return "";
    }
}

int kprintf(const char *format, ...);
int ktprintf(const char *format, ...);
int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int snprintf(char *str, size_t size, const char *format, ...);
void printcol(const char *color, const char *text);
void log_to_terminal(result_t status, const char *from, const char *file, int line, const char *fmt, ...);

#define log(status, fmt, ...) log_to_terminal(status, __func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
