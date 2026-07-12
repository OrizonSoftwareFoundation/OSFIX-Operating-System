/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#include <log.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <util.h>
#include <time/ktime.h>

static char* fmt_uint(char *buf, unsigned long val, int base) {
    static const char digits[] = "0123456789abcdef";
    char tmp[20];
    int i = 0;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return buf; }

    while (val) { tmp[i++] = digits[val % base]; val /= base; }
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}

extern struct flanterm_context *global_flanterm;

const char* result_str[ResultCount] = {
    [Ok]    = "[  OK  ]",
    [Info]  = "[ INFO ]",
    [Warn]  = "[ WARN ]",
    [Fatal] = "[FAILED]",
};

static inline char* strcpy_advance(char *dest, const char *src) {
    while (*src) *dest++ = *src++;
    return dest;
}

static char* write_timestamp(char *ptr) {
    uint32_t ms   = get_time_ms();
    uint32_t secs = ms / 1000;
    uint32_t frac = ms % 1000;

    *ptr++ = '['; *ptr++ = ' ';

    char sec_str[12];
    itoa(secs, sec_str, 10);
    size_t sec_len = 0;
    while (sec_str[sec_len]) sec_len++;
    for (size_t i = sec_len; i < 4; i++) *ptr++ = ' ';
    ptr = strcpy_advance(ptr, sec_str);

    *ptr++ = '.';

    char frac_str[12];
    itoa(frac, frac_str, 10);
    size_t frac_len = 0;
    while (frac_str[frac_len]) frac_len++;
    for (size_t i = frac_len; i < 3; i++) *ptr++ = '0';
    ptr = strcpy_advance(ptr, frac_str);

    *ptr++ = ' '; *ptr++ = ']'; *ptr++ = ' ';
    return ptr;
}

static void flanterm_write_translated(struct flanterm_context *ctx, const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            flanterm_write(ctx, "\r\n", 2);
        } else {
            flanterm_write(ctx, &buf[i], 1);
        }
    }
}

void log_to_terminal(result_t status, const char *from, const char *file, int line, const char *fmt, ...) {
    if (!global_flanterm) return;
    if (!from) from = "<unknown>";
    if (!file) file = "<unknown>";
    if (!fmt)  return;

    char message[1024];
    char *ptr     = message;
    char *msg_end = message + sizeof(message) - 1;

#define SAFE_ADVANCE(call) do { ptr = (call); if (ptr >= msg_end) goto done; } while(0)
#define SAFE_CHAR(c)       do { if (ptr < msg_end) *ptr++ = (c); else goto done; } while(0)

    ptr = write_timestamp(ptr);

    const char *color = get_status_color(status);
    SAFE_ADVANCE(strcpy_advance(ptr, color));
    SAFE_ADVANCE(strcpy_advance(ptr, result_str[status]));
    if (status != Info)
        SAFE_ADVANCE(strcpy_advance(ptr, COLOR_RESET));

    SAFE_ADVANCE(strcpy_advance(ptr, COLOR_DIM));
    SAFE_CHAR(' ');
    SAFE_CHAR('[');
    SAFE_ADVANCE(strcpy_advance(ptr, from));
    SAFE_ADVANCE(strcpy_advance(ptr, " in "));

    const char *last_slash = file;
    const char *f = file;
    while (*f) {
        if (*f == '/' || *f == '\\') last_slash = f + 1;
        f++;
    }
    SAFE_ADVANCE(strcpy_advance(ptr, last_slash));
    SAFE_CHAR(':');

    char line_str[16];
    itoa(line, line_str, 10);
    SAFE_ADVANCE(strcpy_advance(ptr, line_str));
    SAFE_CHAR(']');
    SAFE_ADVANCE(strcpy_advance(ptr, COLOR_RESET));
    SAFE_CHAR(' ');

    va_list args;
    va_start(args, fmt);
    while (*fmt && ptr < msg_end) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd' || *fmt == 'i') {
                int val = va_arg(args, int);
                char num[16];
                itoa(val, num, 10);
                SAFE_ADVANCE(strcpy_advance(ptr, num));
            } else if (*fmt == 'u') {
                unsigned int val = va_arg(args, unsigned int);
                char num[16];
                fmt_uint(num, (unsigned long)val, 10);
                SAFE_ADVANCE(strcpy_advance(ptr, num));
            } else if (*fmt == 's') {
                char *str = va_arg(args, char *);
                if (str) SAFE_ADVANCE(strcpy_advance(ptr, str));
            } else if (*fmt == 'p' || *fmt == 'x') {

                unsigned long val = va_arg(args, unsigned long);
                SAFE_CHAR('0');
                SAFE_CHAR('x');
                char hex[20];
                fmt_uint(hex, val, 16);
                SAFE_ADVANCE(strcpy_advance(ptr, hex));
            } else if (*fmt == '%') {
                SAFE_CHAR('%');
            } else {

                SAFE_CHAR('%');
                SAFE_CHAR(*fmt);
            }
        } else {
            SAFE_CHAR(*fmt);
        }
        if (*fmt) fmt++;
    }
    va_end(args);

done:
    *ptr = '\0';
    flanterm_write_translated(global_flanterm, message, ptr - message);

#undef SAFE_ADVANCE
#undef SAFE_CHAR
}

void printcol(const char *color, const char *text) {
    if (global_flanterm) {
        flanterm_write(global_flanterm, color, strlen(color));
        flanterm_write_translated(global_flanterm, text, strlen(text));
        flanterm_write(global_flanterm, COLOR_RESET, strlen(COLOR_RESET));
    }
}