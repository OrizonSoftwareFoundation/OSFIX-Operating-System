/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

int init_serial(void);

int serial_received(void);

char read_serial(void);

void serial_write(const char *format, ...);

void serial_printf(const char *fmt, ...);

#endif
