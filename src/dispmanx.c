/*
 * Copyright (c) 2017 Sugizaki Yukimasa (ysugi@idein.jp)
 * All rights reserved.
 *
 * This software is licensed under a Modified (3-Clause) BSD License.
 * You should have received a copy of this license along with this
 * software. If not, contact the copyright holder above.
 */

#include <bcm_host.h>
#include <stdio.h>
#include <stdlib.h>
#include "rpigrafx.h"
#include "local/init_finalize.h"
#include "local/error.h"


/* List of graphic elements. */
static RPIGRAFX_ELEMENT_T *elements = NULL;
static int elements_len = 0;
static int elements_next_idx = 0;

/* Temporary memory for image which is to be passed to vc_dispmanx_resource_write_data(). */
static void *image = NULL;
static int image_size = 0;

/* Screen resolution returned by vc_dispmanx_display_get_info(). */
static int screen_width = -1, screen_height = -1;

static DISPMANX_DISPLAY_HANDLE_T display;
static DISPMANX_UPDATE_HANDLE_T update;
/* Opacity level 0 is not visible. */
static VC_DISPMANX_ALPHA_T alpha = {
    .flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE,
    // .flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,
    128, 0
};

#define _check(x) \
    do { \
        int ret = (x); \
        if (ret) \
            error_and_exit("Assertation failed: 0x%08x\n", ret); \
    } while (0)

static void register_element(const RPIGRAFX_ELEMENT_T element)
{
    if (elements_next_idx >= elements_len) {
        elements_len += 100;
        elements = realloc(elements, elements_len * sizeof(*elements));
        if (elements == NULL) {
            error_and_exit("Failed to realloc %d bytes of memory\n", elements_len * sizeof(*elements));
            exit(EXIT_FAILURE);
        }
    }
    elements[elements_next_idx++] = element;
}

static void remove_all_elements()
{
    int i;
    for (i = 0; i < elements_next_idx; i ++)
        _check(vc_dispmanx_element_remove(update, elements[i]));
    elements_next_idx = 0;
}

static void* use_image(const int size)
{
    if (size > image_size) {
        image_size = size;
        image = realloc(image, image_size);
        if (image == NULL) {
            error_and_exit("Failed to realloc %d bytes of memory\n", image_size);
            exit(EXIT_FAILURE);
        }
    }
    return image;
}

static void choose_color(void *valp, const RPIGRAFX_COLOR_T color, const RPIGRAFX_FORMAT_T format)
{
    const uint32_t palette_rgba32[RPIGRAFX_COLOR_MAX - 1] = {
        0xff000000,
        0xff0000ff,
        0xff00ff00,
        0xff00ffff,
        0xffff0000,
        0xffff00ff,
        0xffffff00,
        0xffffffff,
        0x00000000
    };

    if (format != RPIGRAFX_FORMAT_RGBA32)
        error_and_exit("format must be RGBA32 for now\n");
    * (uint32_t*) valp = palette_rgba32[color - 1];
}


void local_rpigrafx_dispmanx_init()
{
    DISPMANX_MODEINFO_T info;

    if (called.dispmanx != 0) {
        called.dispmanx ++;
        return;
    }

    bcm_host_init();

    /*
     * 0 means the dispmanx impl. uses VC_DISPLAY environment value
     * if it is set.
     */
    display = vc_dispmanx_display_open(0);
    if (display == 0)
        error_and_exit("vc_dispmanx_display_open: 0x%08x\n", display);

    _check(vc_dispmanx_display_get_info(display, &info));
    screen_width = info.width;
    screen_height = info.height;

    update = vc_dispmanx_update_start(0);
    if (update == DISPMANX_NO_HANDLE)
        error_and_exit("vc_dispmanx_update_start");

    called.dispmanx ++;
}

void local_rpigrafx_dispmanx_finalize()
{
    if (called.dispmanx != 1) {
        called.dispmanx --;
        return;
    }

    free(image);
    image = NULL;
    image_size = 0;

    free(elements);
    elements = NULL;
    elements_len = 0;
    elements_next_idx = 0;

    /* No way to cancel update? */
    rpigrafx_remove_all_elements();
    _check(vc_dispmanx_update_submit_sync(update));
    update = DISPMANX_NO_HANDLE;

    screen_width = -1;
    screen_height = -1;

    _check(vc_dispmanx_display_close(display));

    bcm_host_deinit();

    called.dispmanx --;
}


void rpigrafx_get_screen_size(int *width, int *height)
{
    *width = screen_width;
    *height = screen_height;
}

RPIGRAFX_ELEMENT_T rpigrafx_draw_box(const int x_start, const int y_start, const int width, const int height, const int border, const RPIGRAFX_COLOR_T color)
{
    int x, y;
    const int border_width = border <= width / 2 ? border : width / 2;
    const int border_height = border <= height / 2 ? border : height / 2;
    uint32_t *p = use_image(width * height * sizeof(*p));
    uint32_t val_color, val_transp;

    choose_color(&val_color, color, RPIGRAFX_FORMAT_RGBA32);
    choose_color(&val_transp, RPIGRAFX_COLOR_TRANSPARENT, RPIGRAFX_FORMAT_RGBA32);

    for (y = 0; y < border_height; y ++)
        for (x = 0; x < width; x ++)
            p[y * width + x] = val_color;
    for (y = border_height; y < height - border_height; y ++) {
        for (x = 0; x < border_width; x ++)
            p[y * width + x] = val_color;
        for (x = border_width; x < width - border_width; x ++)
            p[y * width + x] = val_transp;
        for (x = width - border_width; x < width; x ++)
            p[y * width + x] = val_color;
    }
    for (y = height - border_height; y < height; y ++)
        for (x = 0; x < width; x ++)
            p[y * width + x] = val_color;

    return rpigrafx_render_image(p, x_start, y_start, width, height);
}

/* Render an image of RGBA32. */
RPIGRAFX_ELEMENT_T rpigrafx_render_image(void *p, const int x, const int y, const int width, const int height)
{
    return rpigrafx_render_image_scale(p, x, y, width, height, width, height);
}

/* Render an image of RGBA32 with scaling. */
RPIGRAFX_ELEMENT_T rpigrafx_render_image_scale(void *p, const int x, const int y, const int width, const int height, const int width_scaled, const int height_scaled)
{
    VC_RECT_T rect, src_rect, dst_rect;
    DISPMANX_ELEMENT_HANDLE_T element;
    DISPMANX_RESOURCE_HANDLE_T resource;
    /*
     * This is set by vc_dispmanx_resource_create but it's always 0 (NULL) for now.
     * We simply ignore this value.
     */
    unsigned vc_image_ptr;

    /* xxx: 512x512 seems to be the maximum. */
    resource = vc_dispmanx_resource_create(
            VC_IMAGE_RGBA32,
            //screen_width, screen_height,
            //512, 512,
            width, height,
            &vc_image_ptr);
    if (resource == 0)
        error_and_exit("vc_dispmanx_resource_create: %d\n", resource);

    /* vcdispmanx_resource_write_data() does not see rect.x. */
    _check(vc_dispmanx_rect_set(&rect, 0, 0, width, height));
    _check(vc_dispmanx_resource_write_data(
            resource,
            VC_IMAGE_RGBA32,
            ALIGN_UP(width * 4, 32),
            p,
            &rect));

    /* Weird shifting trick from hello_pi/hello_dispmanx. */
    _check(vc_dispmanx_rect_set(&src_rect, 0, 0, width << 16, height << 16));
    _check(vc_dispmanx_rect_set(&dst_rect, x, y, width_scaled, height_scaled));
    /* raspistill's layer is 2. https://github.com/raspberrypi/userland/blob/master/host_applications/linux/apps/raspicam/RaspiPreview.h */
    element = vc_dispmanx_element_add(
            update, display,
            5, /* TODO: Need not to fix layer. */
            &dst_rect,
            resource,
            &src_rect,
            DISPMANX_PROTECTION_NONE,
            &alpha, NULL, VC_IMAGE_ROT0);
    if (element == 0)
        error_and_exit("vc_dispmanx_rect_set: %d\n", element);
    _check(vc_dispmanx_resource_delete(resource));
    register_element(element);
    return element;
}

void rpigrafx_commit_drawings()
{
    _check(vc_dispmanx_update_submit_sync(update));
    update = vc_dispmanx_update_start(0);
    if (update == DISPMANX_NO_HANDLE)
        error_and_exit("vc_dispmanx_update_start");
}

void rpigrafx_remove_all_elements()
{
    remove_all_elements();
}
