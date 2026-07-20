//Yazin T.

#include <flanterm.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <kprintf.h>
#include <util.h>
#include <string.h>
#include <ktime.h>
#include <serial.h>
const char* result_str[ResultCount] = {
    [Ok]    = "[  OK  ]",
    [Info]  = "[ INFO ]",
    [Warn]  = "[ WARN ]",
    [Fatal] = "[FAILED]",
};

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

static inline char* strcpy_advance(char *dest, const char *src) {
    while (*src) *dest++ = *src++;
    return dest;
}


extern struct flanterm_context *global_flanterm;

#define FLAG_ZERO_PAD   0x01 

#define FLAG_LEFT_JUST  0x02 

#define FLAG_SIGN_SPACE 0x04 

#define FLAG_SIGN_PLUS  0x08 

#define FLAG_ALT_FORM   0x10 

static const char *digits = "0123456789abcdef";

static const char *digits_upper = "0123456789ABCDEF";

static void print_char(char **out, size_t *remaining, char c) {
    if (!out || !*out || *remaining <= 1) return;
    **out = c;
    (*out)++;
    (*remaining)--;
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

static void print_string(char **out, size_t *remaining, const char *s) {
    if (!s) s = "(null)";
    while (*s && *remaining > 1) {
        print_char(out, remaining, *s++);
    }
}

void printcol(const char *color, const char *text) {
    if (global_flanterm) {
        flanterm_write(global_flanterm, color, strlen(color));
        flanterm_write_translated(global_flanterm, text, strlen(text));
        flanterm_write(global_flanterm, COLOR_RESET, strlen(COLOR_RESET));
    }
}

static char* write_timestamp(char *ptr) {
    uint32_t us    = get_time_us();
    uint32_t secs  = us / 1000000;
    uint32_t frac  = us % 1000000;

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
    for (size_t i = frac_len; i < 6; i++) *ptr++ = '0';
    ptr = strcpy_advance(ptr, frac_str);

    *ptr++ = ' '; *ptr++ = ']'; *ptr++ = ' ';
    return ptr;
}


static void print_unsigned(char **out, size_t *remaining, unsigned long long n, int base, int width, int flags) {
    char buffer[65];
    int i = 0;
    
    if (n == 0) {
        buffer[i++] = '0';
    } else {
        while (n > 0) {
            buffer[i++] = digits[n % base];
            n /= base;
        }
    }
    
    int len = i;
    if (!(flags & FLAG_LEFT_JUST)) {
        while (len < width && *remaining > 1) {
            char pad = (flags & FLAG_ZERO_PAD) ? '0' : ' ';
            print_char(out, remaining, pad);
            len++;
        }
    }
    
    while (i > 0 && *remaining > 1) {
        print_char(out, remaining, buffer[--i]);
    }
    
    if (flags & FLAG_LEFT_JUST) {
        while (len < width && *remaining > 1) {
            print_char(out, remaining, ' ');
            len++;
        }
    }
}

static void print_int(char **out, size_t *remaining, long long n, int base, int width, int flags) {
    if (n < 0) {
        print_char(out, remaining, '-');
        n = -n;
        width--;
    } else if (flags & FLAG_SIGN_PLUS) {
        print_char(out, remaining, '+');
        width--;
    } else if (flags & FLAG_SIGN_SPACE) {
        print_char(out, remaining, ' ');
        width--;
    }
    
    print_unsigned(out, remaining, (unsigned long long)n, base, width, flags);
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    char *out = str;
    size_t remaining = size;
    
    if (!str || size == 0) {

        out = NULL;
        remaining = (size_t)-1;
    }
    
    for (const char *f = format; *f; f++) {
        if (*f != '%') {
            print_char(&out, &remaining, *f);
            continue;
        }
        
        f++;
        
        int flags = 0;
        bool parsing_flags = true;
        while (parsing_flags) {
            switch (*f) {
                case '0': flags |= FLAG_ZERO_PAD; f++; break;
                case '-': flags |= FLAG_LEFT_JUST; f++; break;
                case ' ': flags |= FLAG_SIGN_SPACE; f++; break;
                case '+': flags |= FLAG_SIGN_PLUS; f++; break;
                case '#': flags |= FLAG_ALT_FORM; f++; break;
                default: parsing_flags = false; break;
            }
        }
        
        int width = 0;
        if (*f == '*') {
            width = va_arg(ap, int);
            f++;
        } else {
            while (*f >= '0' && *f <= '9') {
                width = width * 10 + (*f - '0');
                f++;
            }
        }
        
        int precision = -1;
        if (*f == '.') {
            f++;
            if (*f == '*') {
                precision = va_arg(ap, int);
                f++;
            } else {
                precision = 0;
                while (*f >= '0' && *f <= '9') {
                    precision = precision * 10 + (*f - '0');
                    f++;
                }
            }
        }
        
        enum {
            LENGTH_NONE,
            LENGTH_SHORT,
            LENGTH_LONG,
            LENGTH_LONG_LONG
        } length = LENGTH_NONE;
        
        if (*f == 'h') {
            length = LENGTH_SHORT;
            f++;
        } else if (*f == 'l') {
            length = LENGTH_LONG;
            f++;
            if (*f == 'l') {
                length = LENGTH_LONG_LONG;
                f++;
            }
        }
        
        switch (*f) {
            case 'c': {
                char c = (char)va_arg(ap, int);
                print_char(&out, &remaining, c);
                break;
            }
            
            case 's': {
                const char *s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                print_string(&out, &remaining, s);
                break;
            }
            
            case 'd':
            case 'i': {
                long long n;
                if (length == LENGTH_LONG_LONG)
                    n = va_arg(ap, long long);
                else if (length == LENGTH_LONG)
                    n = va_arg(ap, long);
                else
                    n = va_arg(ap, int);
                print_int(&out, &remaining, n, 10, width, flags);
                break;
            }
            
            case 'u': {
                unsigned long long n;
                if (length == LENGTH_LONG_LONG)
                    n = va_arg(ap, unsigned long long);
                else if (length == LENGTH_LONG)
                    n = va_arg(ap, unsigned long);
                else
                    n = va_arg(ap, unsigned int);
                print_unsigned(&out, &remaining, n, 10, width, flags);
                break;
            }
            
            case 'x': {
                unsigned long long n;
                if (length == LENGTH_LONG_LONG)
                    n = va_arg(ap, unsigned long long);
                else if (length == LENGTH_LONG)
                    n = va_arg(ap, unsigned long);
                else
                    n = va_arg(ap, unsigned int);
                if (flags & FLAG_ALT_FORM && n != 0) {
                    print_string(&out, &remaining, "0x");
                }
                print_unsigned(&out, &remaining, n, 16, width, flags);
                break;
            }
            
            case 'X': {
                unsigned long long n;
                if (length == LENGTH_LONG_LONG)
                    n = va_arg(ap, unsigned long long);
                else if (length == LENGTH_LONG)
                    n = va_arg(ap, unsigned long);
                else
                    n = va_arg(ap, unsigned int);
                if (flags & FLAG_ALT_FORM && n != 0) {
                    print_string(&out, &remaining, "0X");
                }

                const char *old_digits = digits;
                digits = digits_upper;
                print_unsigned(&out, &remaining, n, 16, width, flags);
                digits = old_digits;
                break;
            }
            
            case 'p': {
                void *p = va_arg(ap, void*);
                print_string(&out, &remaining, "0x");
                print_unsigned(&out, &remaining, (unsigned long)p, 16, width, flags);
                break;
            }
            
            case '%':
                print_char(&out, &remaining, '%');
                break;
                
            default:
                print_char(&out, &remaining, '%');
                print_char(&out, &remaining, *f);
                break;
        }
    }
    
    if (str && size > 0) {
        if (remaining > 0) {
            *out = '\0';
        } else {
            str[size - 1] = '\0';
        }
    }
    

    return (int)(out - str);
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    int result;
    
    va_start(ap, format);
    result = vsnprintf(str, size, format, ap);
    va_end(ap);
    
    return result;
}


static void write_both(const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            flanterm_write(global_flanterm, "\r\n", 2);
        } else {
            flanterm_write(global_flanterm, &buf[i], 1);
        }
        if (buf[i] == '\n') putc('\r');
        putc(buf[i]);
    }
}

int kprintf(const char *format, ...) {
    if (!global_flanterm) return -1;

    va_list args;
    va_start(args, format);

    char c;
    char buf[64];
    char *p = NULL;
    int pad = 0;
    char pad_char = ' ';
    int chars_written = 0;

    write_both(COLOR_DIM, strlen(COLOR_DIM));

    while ((c = *format++) != 0) {
        pad = 0;
        pad_char = ' ';

        if (c != '%') {
            if (c == '\n') {
                write_both("\n", 1);
                chars_written += 2;
            } else {
                write_both(&c, 1);
                chars_written++;
            }
            continue;
        }

        if (*format == '0') {
            pad_char = '0';
            format++;
        }

        while (*format >= '0' && *format <= '9') {
            pad = pad * 10 + (*format - '0');
            format++;
        }

        int is_long = 0;
        int is_longlong = 0;
        if (*format == 'l') {
            format++;
            if (*format == 'l') {
                is_longlong = 1;
                format++;
            } else {
                is_long = 1;
            }
        } else if (*format == 'L') {
            is_longlong = 1;
            format++;
        }

        c = *format++;
        p = NULL;

        switch (c) {
            case 'd':
            case 'u':
            case 'x':
            case 'o': {
                if (is_longlong) {
                    uint64_t val = va_arg(args, uint64_t);
                    if (c == 'x') {
                        write_both("0x", 2);
                        chars_written += 2;
                    }
                    itoa(val, buf, (c == 'x') ? 16 : (c == 'o') ? 8 : 10);
                    p = buf;
                } else if (is_long) {
                    long val = va_arg(args, long);
                    if (c == 'x') {
                        write_both("0x", 2);
                        chars_written += 2;
                    }
                    itoa(val, buf, (c == 'x') ? 16 : (c == 'o') ? 8 : 10);
                    p = buf;
                } else {
                    int val = va_arg(args, int);
                    if (c == 'x') {
                        write_both("0x", 2);
                        chars_written += 2;
                    }
                    itoa(val, buf, (c == 'x') ? 16 : (c == 'o') ? 8 : 10);
                    p = buf;
                }
                break;
            }

            case 'f': {
                double fval = va_arg(args, double);
                int int_part = (int)fval;
                double frac = fval - int_part;
                itoa(int_part, buf, 10);

                char *temp = buf;
                while (*temp) {
                    write_both(temp, 1);
                    temp++;
                    chars_written++;
                }

                write_both(".", 1);
                chars_written++;
                frac *= 1000000;
                itoa((int)frac, buf, 10);
                temp = buf;
                while (*temp) {
                    write_both(temp, 1);
                    temp++;
                    chars_written++;
                }
                continue;
            }

            case 's':
                p = va_arg(args, char *);
                if (!p) p = "(null)";
                break;

            case 'c': {
                char ch = (char)va_arg(args, int);
                if (ch == '\n') {
                    write_both("\n", 1);
                    chars_written += 2;
                } else {
                    write_both(&ch, 1);
                    chars_written++;
                }
                continue;
            }

            case 'p':
                write_both("0x", 2);
                chars_written += 2;
                itoa((uintptr_t)va_arg(args, void *), buf, 16);
                p = buf;
                break;

            case 'C': {
                const char *color = va_arg(args, const char *);
                if (!color) color = COLOR_DIM;
                write_both(color, strlen(color));
                continue;
            }

            case 'm': {
                char *mem = va_arg(args, char *);
                if (!mem) mem = "(null)";
                int len = pad ? pad : 16;
                write_both(mem, len);
                chars_written += len;
                continue;
            }

            default: {
                write_both("%", 1);
                write_both(&c, 1);
                chars_written += 2;
                continue;
            }
        }

        if (p) {
            int len = 0;
            char *p2 = p;
            while (*p2++) len++;

            for (int i = len; i < pad; i++) {
                write_both(&pad_char, 1);
                chars_written++;
            }

            write_both(p, len);
            chars_written += len;
        }
    }

    write_both(COLOR_RESET, strlen(COLOR_RESET));

    va_end(args);
    return chars_written;
}

int ktprintf(const char *format, ...) {
    if (!global_flanterm) return -1;

    va_list args;
    va_start(args, format);

    char c;
    char buf[64];
    char *p = NULL;
    int pad = 0;
    char pad_char = ' ';
    int chars_written = 0;

    char ts_buf[32];
    char *ts_end = write_timestamp(ts_buf);
    size_t ts_len = (size_t)(ts_end - ts_buf);
    write_both(ts_buf, ts_len);
    chars_written += (int)ts_len;

    write_both(COLOR_DIM, strlen(COLOR_DIM));

    while ((c = *format++) != 0) {
        pad = 0;
        pad_char = ' ';

        if (c != '%') {
            if (c == '\n') {
                write_both("\n", 1);
                chars_written += 2;
            } else {
                write_both(&c, 1);
                chars_written++;
            }
            continue;
        }

        if (*format == '0') {
            pad_char = '0';
            format++;
        }

        while (*format >= '0' && *format <= '9') {
            pad = pad * 10 + (*format - '0');
            format++;
        }

        int is_long = 0;
        int is_longlong = 0;
        if (*format == 'l') {
            format++;
            if (*format == 'l') {
                is_longlong = 1;
                format++;
            } else {
                is_long = 1;
            }
        } else if (*format == 'L') {
            is_longlong = 1;
            format++;
        }

        c = *format++;
        p = NULL;

        switch (c) {
            case 'd':
            case 'u':
            case 'x':
            case 'o': {
                if (is_longlong) {
                    uint64_t val = va_arg(args, uint64_t);
                    if (c == 'x') {
                        write_both("0x", 2);
                        chars_written += 2;
                    }
                    itoa(val, buf, (c == 'x') ? 16 : (c == 'o') ? 8 : 10);
                    p = buf;
                } else if (is_long) {
                    long val = va_arg(args, long);
                    if (c == 'x') {
                        write_both("0x", 2);
                        chars_written += 2;
                    }
                    itoa(val, buf, (c == 'x') ? 16 : (c == 'o') ? 8 : 10);
                    p = buf;
                } else {
                    int val = va_arg(args, int);
                    if (c == 'x') {
                        write_both("0x", 2);
                        chars_written += 2;
                    }
                    itoa(val, buf, (c == 'x') ? 16 : (c == 'o') ? 8 : 10);
                    p = buf;
                }
                break;
            }

            case 'f': {
                double fval = va_arg(args, double);
                int int_part = (int)fval;
                double frac = fval - int_part;
                itoa(int_part, buf, 10);

                char *temp = buf;
                while (*temp) {
                    write_both(temp, 1);
                    temp++;
                    chars_written++;
                }

                write_both(".", 1);
                chars_written++;
                frac *= 1000000;
                itoa((int)frac, buf, 10);
                temp = buf;
                while (*temp) {
                    write_both(temp, 1);
                    temp++;
                    chars_written++;
                }
                continue;
            }

            case 's':
                p = va_arg(args, char *);
                if (!p) p = "(null)";
                break;

            case 'c': {
                char ch = (char)va_arg(args, int);
                if (ch == '\n') {
                    write_both("\n", 1);
                    chars_written += 2;
                } else {
                    write_both(&ch, 1);
                    chars_written++;
                }
                continue;
            }

            case 'p':
                write_both("0x", 2);
                chars_written += 2;
                itoa((uintptr_t)va_arg(args, void *), buf, 16);
                p = buf;
                break;

            case 'C': {
                const char *color = va_arg(args, const char *);
                if (!color) color = COLOR_DIM;
                write_both(color, strlen(color));
                continue;
            }

            case 'm': {
                char *mem = va_arg(args, char *);
                if (!mem) mem = "(null)";
                int len = pad ? pad : 16;
                write_both(mem, len);
                chars_written += len;
                continue;
            }

            default: {
                write_both("%", 1);
                write_both(&c, 1);
                chars_written += 2;
                continue;
            }
        }

        if (p) {
            int len = 0;
            char *p2 = p;
            while (*p2++) len++;

            for (int i = len; i < pad; i++) {
                write_both(&pad_char, 1);
                chars_written++;
            }

            write_both(p, len);
            chars_written += len;
        }
    }

    write_both(COLOR_RESET, strlen(COLOR_RESET));

    va_end(args);
    return chars_written;
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

    SAFE_ADVANCE(strcpy_advance(ptr, COLOR_GRAY));
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
    serial_write(message);

#undef SAFE_ADVANCE
#undef SAFE_CHAR
}
