/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin Tantawi
 @license: EUPL 1.2
 */


#include <io.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#define PORT 0x3F8

int init_serial() {
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x80);
    outb(PORT + 0, 0x03);
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x03);
    outb(PORT + 2, 0xC7);
    outb(PORT + 4, 0x0B);
    outb(PORT + 4, 0x1E);
    outb(PORT + 0, 0xAE);

    if (inb(PORT + 0) != 0xAE)
        return 1;

    outb(PORT + 4, 0x0F);
    return 0;
}

int serial_received() {
    return inb(PORT + 5) & 1;
}

char read_serial() {
    while (serial_received() == 0);
    return inb(PORT);
}

static void serial_wait_tx(void) {
    while (!(inb(PORT + 5) & 0x20)) {}
}

void putc(char c) {
    serial_wait_tx();
    outb(PORT, (uint8_t)c);
}

void serial_write(const char *msg) {
    if (!msg) return;
    while (*msg) {
        if (*msg == '\n') putc('\r');
        putc(*msg++);
    }
}

static void write_uint_dec(uint64_t val) {
    char buf[20];
    int i = 0;
    if (val == 0) { putc('0'); return; }
    while (val) { buf[i++] = '0' + (val % 10); val /= 10; }
    while (i > 0) putc(buf[--i]);
}

static void write_uint_hex(uint64_t val, int uppercase) {
    const char *hex = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[16];
    int i = 0;
    if (val == 0) { putc('0'); return; }
    while (val) { buf[i++] = hex[val % 16]; val /= 16; }
    while (i > 0) putc(buf[--i]);
}

void serial_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            if (*p == '\n') putc('\r');
            putc(*p);
            continue;
        }

        p++;
        if (!*p) break;

        
        int is_long = 0, is_longlong = 0;
        if (*p == 'l') {
            p++;
            if (*p == 'l') { is_longlong = 1; p++; }
            else              is_long = 1;
        } else if (*p == 'z') {
            is_longlong = (sizeof(size_t) == 8);
            is_long     = (sizeof(size_t) == 4);
            p++;
        }

        switch (*p) {
            case 'd':
            case 'i': {
                int64_t val = is_longlong ? (int64_t)va_arg(args, long long)
                            : is_long     ? (int64_t)va_arg(args, long)
                                          : (int64_t)va_arg(args, int);
                if (val < 0) { putc('-'); val = -val; }
                write_uint_dec((uint64_t)val);
                break;
            }
            case 'u': {
                uint64_t val = is_longlong ? (uint64_t)va_arg(args, unsigned long long)
                             : is_long     ? (uint64_t)va_arg(args, unsigned long)
                                           : (uint64_t)va_arg(args, unsigned int);
                write_uint_dec(val);
                break;
            }
            case 'x':
            case 'X': {
                uint64_t val = is_longlong ? (uint64_t)va_arg(args, unsigned long long)
                             : is_long     ? (uint64_t)va_arg(args, unsigned long)
                                           : (uint64_t)va_arg(args, unsigned int);
                serial_write("0x");
                write_uint_hex(val, *p == 'X');
                break;
            }
            case 'p': {
                serial_write("0x");
                write_uint_hex((uint64_t)va_arg(args, void *), 0);
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                serial_write(s ? s : "(null)");
                break;
            }
            case 'c':
                putc((char)va_arg(args, int));
                break;
            case '%':
                putc('%');
                break;
            default:
                putc('%');
                putc(*p);
                break;
        }
    }

    va_end(args);
}
