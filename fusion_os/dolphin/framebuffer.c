/*
 * framebuffer.c - vesa framebuffer implementation for fusion os
 * 
 * implements vesa vbe mode setting and basic graphics operations
 * for framebuffer display on x86-64 systems
 */

#include "framebuffer.h"
#include "../common/logger.h"
#include "../common/string.h"
#include <stdlib.h>

/* global framebuffer configuration */
static framebuffer_config_t current_config;
static int framebuffer_initialized = 0;

/* vesa vbe function calls */
#define VESA_VBE_FUNCTION_SET_MODE     0x4f02
#define VESA_VBE_FUNCTION_GET_MODE_INFO 0x4f01

/* vesa mode attributes */
#define VESA_MODE_SUPPORTED         0x01
#define VESA_MODE_TTY_SUPPORTED     0x04
#define VESA_MODE_LINEAR_FB_SUPPORTED 0x80

/*
 * vesa vbe bios call wrapper
 */
static uint16_t vesa_vbe_call(uint16_t function, uint16_t mode, void* es_di) {
    uint16_t result;
    __asm__ volatile (
        "movw %1, %%ax\n\t"
        "movw %2, %%bx\n\t"
        "movq %3, %%rdi\n\t"
        "int $0x10\n\t"
        "movw %%ax, %0"
        : "=r" (result)
        : "r" (function), "r" (mode), "r" (es_di)
        : "ax", "bx", "rdi"
    );
    return result;
}

/*
 * set vesa vbe mode with linear framebuffer
 */
static int set_vesa_mode(uint32_t mode) {
    /* check if function is supported */
    if (vesa_vbe_call(0x4f00, 0, (void*)0) != 0x004f) {
        LOG_ERROR("framebuffer", "vesa vbe not supported");
        return -1;
    }
    
    /* set mode with linear framebuffer flag */
    uint16_t result = vesa_vbe_call(VESA_VBE_FUNCTION_SET_MODE, mode | 0x4000, (void*)0);
    
    if (result != 0x004f) {
        LOG_ERROR("framebuffer", "failed to set vesa mode 0x%x", mode);
        return -1;
    }
    
    LOG_INFO("framebuffer", "vesa mode 0x%x set successfully", mode);
    return 0;
}

/*
 * get vesa mode information
 */
static int get_vesa_mode_info(uint32_t mode, vesa_mode_info_t* info) {
    /* allocate temporary buffer for mode info */
    void* buffer = (void*)0x5000; /* use memory below 1MB for bios access */
    
    memset(buffer, 0, sizeof(vesa_mode_info_t));
    
    uint16_t result = vesa_vbe_call(VESA_VBE_FUNCTION_GET_MODE_INFO, mode, buffer);
    
    if (result != 0x004f) {
        LOG_ERROR("framebuffer", "failed to get mode info for 0x%x", mode);
        return -1;
    }
    
    memcpy(info, buffer, sizeof(vesa_mode_info_t));
    return 0;
}

/*
 * find best vesa mode for given resolution
 */
static uint32_t find_best_mode(uint32_t width, uint32_t height, uint32_t bpp) {
    /* common vesa modes */
    struct {
        uint32_t width;
        uint32_t height;
        uint32_t bpp;
        uint32_t mode;
    } modes[] = {
        {800, 600, 32, 0x0105},
        {1024, 768, 32, 0x0107},
        {1280, 720, 32, 0x0110},
        {1280, 1024, 32, 0x0108},
        {1366, 768, 32, 0x0111},
        {1440, 900, 32, 0x0112},
        {1600, 900, 32, 0x0113},
        {1920, 1080, 32, 0x0114},
        {0, 0, 0, 0} /* end marker */
    };
    
    for (int i = 0; modes[i].mode != 0; i++) {
        if (modes[i].width == width && modes[i].height == height && modes[i].bpp == bpp) {
            return modes[i].mode;
        }
    }
    
    /* fall back to 1024x768x32 */
    LOG_WARNING("framebuffer", "requested mode not found, falling back to 1024x768x32");
    return 0x0107;
}

/*
 * initialize framebuffer with vesa vbe
 */
int framebuffer_init(void) {
    if (framebuffer_initialized) {
        return 0;
    }
    
    LOG_INFO("framebuffer", "initializing vesa framebuffer");
    
    /* try to set 1024x768x32 mode */
    uint32_t mode = find_best_mode(1024, 768, 32);
    
    if (set_vesa_mode(mode) != 0) {
        LOG_ERROR("framebuffer", "failed to initialize vesa framebuffer");
        return -1;
    }
    
    /* get mode information */
    vesa_mode_info_t mode_info;
    if (get_vesa_mode_info(mode, &mode_info) != 0) {
        return -1;
    }
    
    /* configure framebuffer structure */
    current_config.lfb_address = (void*)mode_info.lfb_physical_address;
    current_config.width = mode_info.pixels_per_x_resolution;
    current_config.height = mode_info.pixels_per_y_resolution;
    current_config.bits_per_pixel = mode_info.bits_per_pixel;
    current_config.bytes_per_line = mode_info.bytes_per_scan_line_for_linear;
    current_config.pitch = mode_info.bytes_per_scan_line_for_linear;
    
    /* configure color masks */
    current_config.red_mask = ((1 << mode_info.red_mask_size_for_linear) - 1) << mode_info.red_field_position_for_linear;
    current_config.green_mask = ((1 << mode_info.green_mask_size_for_linear) - 1) << mode_info.green_field_position_for_linear;
    current_config.blue_mask = ((1 << mode_info.blue_mask_size_for_linear) - 1) << mode_info.blue_field_position_for_linear;
    current_config.alpha_mask = ((1 << mode_info.reserved_mask_size_for_linear) - 1) << mode_info.reserved_field_position;
    
    /* validate configuration */
    if (current_config.lfb_address == NULL || current_config.width == 0 || current_config.height == 0) {
        LOG_ERROR("framebuffer", "invalid framebuffer configuration");
        return -1;
    }
    
    LOG_INFO("framebuffer", "framebuffer initialized: %dx%d at %dbpp, lfb at %p", 
             current_config.width, current_config.height, 
             current_config.bits_per_pixel, current_config.lfb_address);
    
    /* clear framebuffer */
    framebuffer_clear(0x00000000);
    
    framebuffer_initialized = 1;
    return 0;
}

/*
 * set specific vesa mode
 */
void framebuffer_set_mode(uint32_t width, uint32_t height, uint32_t bpp) {
    if (!framebuffer_initialized) {
        framebuffer_init();
    }
    
    uint32_t mode = find_best_mode(width, height, bpp);
    
    if (set_vesa_mode(mode) == 0) {
        /* update configuration */
        current_config.width = width;
        current_config.height = height;
        current_config.bits_per_pixel = bpp;
        
        LOG_INFO("framebuffer", "mode changed to %dx%d at %dbpp", width, height, bpp);
    }
}

/*
 * get current framebuffer configuration
 */
framebuffer_config_t* framebuffer_get_config(void) {
    return &current_config;
}

/*
 * clear framebuffer with specified color
 */
void framebuffer_clear(uint32_t color) {
    if (!framebuffer_initialized || current_config.lfb_address == NULL) {
        return;
    }
    
    /* clear entire framebuffer */
    size_t total_size = current_config.pitch * current_config.height;
    memset(current_config.lfb_address, 0, total_size);
    
    /* fill with color if not black */
    if (color != 0) {
        for (uint32_t y = 0; y < current_config.height; y++) {
            for (uint32_t x = 0; x < current_config.width; x++) {
                framebuffer_draw_pixel(x, y, color);
            }
        }
    }
}

/*
 * draw single pixel at coordinates
 */
void framebuffer_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!framebuffer_initialized || x >= current_config.width || y >= current_config.height) {
        return;
    }
    
    /* calculate pixel offset */
    uint32_t offset = (y * current_config.pitch) + (x * (current_config.bits_per_pixel / 8));
    uint8_t* pixel_addr = (uint8_t*)current_config.lfb_address + offset;
    
    /* write pixel based on color depth */
    if (current_config.bits_per_pixel == 32) {
        *(uint32_t*)pixel_addr = color;
    } else if (current_config.bits_per_pixel == 24) {
        *(uint16_t*)pixel_addr = (color >> 8) & 0xffff;
        *(uint8_t*)(pixel_addr + 2) = (color >> 24) & 0xff;
    } else if (current_config.bits_per_pixel == 16) {
        *(uint16_t*)pixel_addr = color & 0xffff;
    }
}

/*
 * draw filled rectangle
 */
void framebuffer_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t dy = 0; dy < height; dy++) {
        for (uint32_t dx = 0; dx < width; dx++) {
            framebuffer_draw_pixel(x + dx, y + dy, color);
        }
    }
}

/*
 * draw line using simple bresenham algorithm
 */
void framebuffer_draw_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color) {
    int32_t dx = abs((int32_t)x2 - (int32_t)x1);
    int32_t sx = x1 < x2 ? 1 : -1;
    int32_t dy = -abs((int32_t)y2 - (int32_t)y1);
    int32_t sy = y1 < y2 ? 1 : -1;
    int32_t err = dx + dy;
    
    while (1) {
        framebuffer_draw_pixel((uint32_t)x1, (uint32_t)y1, color);
        
        if (x1 == x2 && y1 == y2) {
            break;
        }
        
        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/*
 * create color from rgba components
 */
uint32_t framebuffer_make_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
    uint32_t color = 0;
    color |= (uint32_t)alpha << 24;
    color |= (uint32_t)red << 16;
    color |= (uint32_t)green << 8;
    color |= (uint32_t)blue;
    return color;
}

/*
 * extract rgba components from color
 */
void framebuffer_get_color(uint32_t color, uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* alpha) {
    if (red) *red = (color >> 16) & 0xff;
    if (green) *green = (color >> 8) & 0xff;
    if (blue) *blue = color & 0xff;
    if (alpha) *alpha = (color >> 24) & 0xff;
}

/*
 * allocate buffer for framebuffer operations
 */
void* framebuffer_alloc_buffer(size_t size) {
    return malloc(size);
}

/*
 * free allocated buffer
 */
void framebuffer_free_buffer(void* buffer) {
    free(buffer);
}

/*
 * copy data to framebuffer buffer
 */
int framebuffer_copy_to_buffer(void* source, void* destination, size_t size) {
    if (source == NULL || destination == NULL) {
        return -1;
    }
    
    memcpy(destination, source, size);
    return 0;
}