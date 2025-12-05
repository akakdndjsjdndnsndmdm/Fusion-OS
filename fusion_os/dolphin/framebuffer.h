/*
 * framebuffer.h - framebuffer graphics for fusion os
 * 
 * vesa framebuffer support for graphical display with proper
 * memory allocation and graphics operations
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>

/* vesa mode information */
typedef struct {
    uint16_t mode_attributes;
    uint8_t window_a_attributes;
    uint8_t window_b_attributes;
    uint16_t window_granularity;
    uint16_t window_size;
    uint16_t window_a_segment;
    uint16_t window_b_segment;
    uint32_t window_position_function;
    uint16_t bytes_per_scan_line;
    uint16_t pixels_per_x_resolution;
    uint16_t pixels_per_y_resolution;
    uint8_t character_cell_width;
    uint8_t character_cell_height;
    uint8_t number_of_planes;
    uint8_t bits_per_pixel;
    uint8_t number_of_banks;
    uint8_t bank_size;
    uint8_t number_of_image_pages;
    uint8_t reserved_page;
    uint8_t red_mask_size;
    uint8_t red_field_position;
    uint8_t green_mask_size;
    uint8_t green_field_position;
    uint8_t blue_mask_size;
    uint8_t blue_field_position;
    uint8_t reserved_mask_size;
    uint8_t reserved_field_position;
    uint8_t direct_color_mode_info;
    uint32_t physical_address;
    uint32_t reserved[6];
    uint16_t bytes_per_scan_line_for_linear;
    uint16_t pixels_per_x_resolution_for_linear;
    uint16_t pixels_per_y_resolution_for_linear;
    uint32_t lfb_physical_address;
    uint8_t reserved2[3];
    uint8_t bits_per_pixel_for_linear;
    uint8_t number_of_image_pages_for_linear;
    uint8_t reserved3;
    uint32_t red_mask_size_for_linear;
    uint8_t red_field_position_for_linear;
    uint32_t green_mask_size_for_linear;
    uint8_t green_field_position_for_linear;
    uint32_t blue_mask_size_for_linear;
    uint8_t blue_field_position_for_linear;
    uint32_t reserved_mask_size_for_linear;
    uint8_t reserved4[6];
} vesa_mode_info_t;

/* framebuffer configuration */
typedef struct {
    void* lfb_address;
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    uint32_t bytes_per_line;
    uint32_t pitch;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t alpha_mask;
} framebuffer_config_t;

/* framebuffer initialization */
int framebuffer_init(void);
void framebuffer_set_mode(uint32_t width, uint32_t height, uint32_t bpp);
framebuffer_config_t* framebuffer_get_config(void);

/* basic graphics operations */
void framebuffer_clear(uint32_t color);
void framebuffer_draw_pixel(uint32_t x, uint32_t y, uint32_t color);
void framebuffer_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void framebuffer_draw_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);

/* framebuffer utility */
uint32_t framebuffer_make_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void framebuffer_get_color(uint32_t color, uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* alpha);

/* memory operations for framebuffer */
void* framebuffer_alloc_buffer(size_t size);
void framebuffer_free_buffer(void* buffer);
int framebuffer_copy_to_buffer(void* source, void* destination, size_t size);

#endif /* FRAMEBUFFER_H */