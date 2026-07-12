# Utilities

This page documents common support code used across the kernel and modules.

## Logging

Logging is implemented in `kernel/main/util/print/log.c` with declarations in
`kernel/includes/main/util/print/log.h`.

Logs include:

- Timestamp based on `get_time_ms`.
- Result status.
- Function name.
- Source file and line.
- Formatted message text.

Known result statuses:

- `Ok`
- `Info`
- `Warn`
- `Fatal`

Terminal output goes through Flanterm when `global_flanterm` is available.
Serial logging is available separately through serial helpers.

## Terminal

The terminal uses Flanterm sources under `kernel/main/terminal/src`. The kernel
initializes a framebuffer backend in `kmain` using Limine's first framebuffer.

Text output uses CRLF translation for terminal display.

## Serial

Serial helpers live in `kernel/main/util/print/serial.c` and
`kernel/includes/main/util/print/serial.h`.

The serial interface provides:

- `init_serial`
- receive checks
- transmit readiness checks
- formatted serial output

Serial logging is important because it still works when framebuffer output is
unusable or hard to inspect.

## Formatted Printing

Formatted output lives in `kernel/main/util/print/kprintf.c`.

Provided interfaces include:

- `kprintf`
- `sprintf`
- `snprintf`
- `vsprintf`
- `vsnprintf`

Use these instead of host libc printing; the kernel is freestanding.

## Time

Time code lives in:

- `kernel/main/util/time/ktime.c`
- `kernel/main/util/time/tsc.c`
- `kernel/main/util/time/pit.c`

`boot_tsc` is recorded near the start of `kmain`.

The time layer can:

- Measure TSC over PIT.
- Estimate CPU clock speed.
- Return elapsed milliseconds or microseconds.
- Delay with PIT or TSC depending on the active delay mode.

Currently implemented delay paths:

- PIT delay.
- TSC delay.

Placeholder modes exist for PIC, APIC, IOAPIC, and HPET delay, but return
failure.

## Port I/O and CPU Helpers

`kernel/main/cpu/io.c` provides:

- `inb`, `outb`, and related port I/O helpers.
- Interrupt enable and disable.
- Halt helpers.
- CPUID wrapper.

Headers also define inline helpers for model-specific registers, CR registers,
and TSC reads.

## Strings

String and memory helpers live in `kernel/main/util/string.c`.

The project provides freestanding versions of common operations:

- `memcpy`
- `memset`
- `memmove`
- `memcmp`
- `strcmp`
- `strncmp`
- `strlen`
- `strcpy`
- `strncpy`
- `strlcpy`
- `strcat`
- `strncat`
- `strlcat`
- `strchr`
- `strrchr`
- `strpbrk`
- `strstr`
- `strtok`

## Math and Conversion

Math helpers live in `kernel/main/util/math.c`.
Conversion helpers live in `kernel/main/util/util.c`.

Available helpers include:

- Integer division and modulo support helpers used by the compiler.
- `roundf`.
- `itoa`.
- `atoi`.

These exist because the kernel does not link against a normal hosted C runtime.
