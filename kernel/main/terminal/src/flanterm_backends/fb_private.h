#ifndef FLANTERM_FB_PRIVATE_H
#define FLANTERM_FB_PRIVATE_H 1

#ifndef FLANTERM_IN_FLANTERM
#error "Do not use fb_private.h. Use interfaces defined in fb.h only."
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLANTERM_FB_FONT_GLYPHS 256

struct flanterm_fb_char {
    uint8_t c; 

    bool fg_default; 

    bool bg_default; 

    
    uint32_t fg; 

    uint32_t bg; 

};

struct flanterm_fb_queue_item {
    size_t x, y; 

    struct flanterm_fb_char c; 

};

struct flanterm_fb_context {
    struct flanterm_context term; 

    void (*plot_char)(struct flanterm_context *ctx, struct flanterm_fb_char *c, size_t x, size_t y); 

    void (*flush_callback)(volatile void *address, size_t length); 

    size_t font_width; 

    size_t font_height; 

    size_t glyph_width; 

    size_t glyph_height; 

    size_t font_scale_x; 

    size_t font_scale_y; 

    size_t offset_x, offset_y; 

    volatile uint32_t *framebuffer; 

    size_t pitch; 

    size_t width; 

    size_t height; 

    size_t phys_height; 

    size_t bpp; 

    uint8_t red_mask_size, red_mask_shift; 

    uint8_t green_mask_size, green_mask_shift; 

    uint8_t blue_mask_size, blue_mask_shift; 

    int rotation; 

    size_t font_bits_size; 

    uint8_t *font_bits; 

    size_t font_bool_size; 

    bool *font_bool; 

    uint32_t ansi_colours[8]; 

    uint32_t ansi_bright_colours[8]; 

    uint32_t default_fg, default_bg; 

    uint32_t default_fg_bright, default_bg_bright; 

    size_t canvas_size; 

    uint32_t *canvas; 

    size_t grid_size; 

    size_t queue_size; 

    size_t map_size; 

    struct flanterm_fb_char *grid; 

    struct flanterm_fb_queue_item *queue; 

    size_t queue_i; 

    struct flanterm_fb_queue_item **map; 

    uint32_t text_fg; 

    uint32_t text_bg; 

    bool text_fg_default; 

    bool text_bg_default; 

    size_t cursor_x; 

    size_t cursor_y; 

    uint32_t saved_state_text_fg; 

    uint32_t saved_state_text_bg; 

    bool saved_state_text_fg_default; 

    bool saved_state_text_bg_default; 

    size_t saved_state_cursor_x; 

    size_t saved_state_cursor_y; 

    size_t old_cursor_x; 

    size_t old_cursor_y; 

};

#ifdef __cplusplus
}
#endif

#endif
