#ifndef FLANTERM_FB_H
#define FLANTERM_FB_H 1

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../flanterm.h"

#ifdef FLANTERM_IN_FLANTERM

#include "fb_private.h"

#endif

#define FLANTERM_FB_ROTATE_0 0
#define FLANTERM_FB_ROTATE_90 1
#define FLANTERM_FB_ROTATE_180 2
#define FLANTERM_FB_ROTATE_270 3

struct flanterm_context *flanterm_fb_init(
    
    void *(*_malloc)(size_t size),
    void (*_free)(void *ptr, size_t size),
    uint32_t *framebuffer, size_t width, size_t height, size_t pitch,
    uint8_t red_mask_size, uint8_t red_mask_shift,
    uint8_t green_mask_size, uint8_t green_mask_shift,
    uint8_t blue_mask_size, uint8_t blue_mask_shift,
    uint32_t *canvas, 
    uint32_t *ansi_colours, uint32_t *ansi_bright_colours, 
    uint32_t *default_bg, uint32_t *default_fg, 
    uint32_t *default_bg_bright, uint32_t *default_fg_bright, 
    
    void *font, size_t font_width, size_t font_height, size_t font_spacing,
    
    size_t font_scale_x, size_t font_scale_y,
    size_t margin,
    
    int rotation
);

void flanterm_fb_set_flush_callback(struct flanterm_context *ctx, void (*flush_callback)(volatile void *address, size_t length));

#ifdef __cplusplus
}
#endif

#endif