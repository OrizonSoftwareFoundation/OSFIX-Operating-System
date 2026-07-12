#ifndef FLANTERM_H
#define FLANTERM_H 1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLANTERM_CB_DEC 10

#define FLANTERM_CB_BELL 20

#define FLANTERM_CB_PRIVATE_ID 30

#define FLANTERM_CB_STATUS_REPORT 40

#define FLANTERM_CB_POS_REPORT 50

#define FLANTERM_CB_KBD_LEDS 60

#define FLANTERM_CB_MODE 70

#define FLANTERM_CB_LINUX 80

#define FLANTERM_CB_OSC 90

#ifdef FLANTERM_IN_FLANTERM

#include "flanterm_private.h"

#else

struct flanterm_context;

#endif

void flanterm_write(struct flanterm_context *ctx, const char *buf, size_t count);

void flanterm_flush(struct flanterm_context *ctx);

void flanterm_full_refresh(struct flanterm_context *ctx);

void flanterm_deinit(struct flanterm_context *ctx, void (*_free)(void *ptr, size_t size));

void flanterm_get_dimensions(struct flanterm_context *ctx, size_t *cols, size_t *rows);

void flanterm_set_autoflush(struct flanterm_context *ctx, bool state);

void flanterm_set_callback(struct flanterm_context *ctx, void (*callback)(struct flanterm_context *, uint64_t, uint64_t, uint64_t, uint64_t));

void flanterm_get_cursor_pos(struct flanterm_context *ctx, size_t *x, size_t *y);

void flanterm_set_cursor_pos(struct flanterm_context *ctx, size_t x, size_t y);

void flanterm_set_text_fg(struct flanterm_context *ctx, size_t colour, bool bright);

void flanterm_set_text_bg(struct flanterm_context *ctx, size_t colour, bool bright);

void flanterm_reset_text_fg(struct flanterm_context *ctx);

void flanterm_reset_text_bg(struct flanterm_context *ctx);

void flanterm_clear(struct flanterm_context *ctx, bool move);

#ifdef __cplusplus
}
#endif

#endif
