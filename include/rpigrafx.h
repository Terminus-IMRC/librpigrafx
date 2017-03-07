/*
 * Copyright (c) 2017 Sugizaki Yukimasa (ysugi@idein.jp)
 * All rights reserved.
 *
 * This software is licensed under a Modified (3-Clause) BSD License.
 * You should have received a copy of this license along with this
 * software. If not, contact the copyright holder above.
 */

#ifndef RPIGRAFX_H
#define RPIGRAFX_H

#include <bcm_host.h>
#include <stdint.h>


    typedef DISPMANX_ELEMENT_HANDLE_T RPIGRAFX_ELEMENT_T;

    typedef enum {
        RPIGRAFX_COLOR_MIN = 0,
        RPIGRAFX_COLOR_BLACK,
        RPIGRAFX_COLOR_RED,
        RPIGRAFX_COLOR_GREEN,
        RPIGRAFX_COLOR_ORANGE,
        RPIGRAFX_COLOR_BLUE,
        RPIGRAFX_COLOR_MAGENTA,
        RPIGRAFX_COLOR_CYAN,
        RPIGRAFX_COLOR_WHITE,
        RPIGRAFX_COLOR_TRANSPARENT,
        RPIGRAFX_COLOR_MAX
    } RPIGRAFX_COLOR_T;

    typedef enum {
        RPIGRAFX_FORMAT_MIN = 0,
        RPIGRAFX_FORMAT_RGBA32,
        RPIGRAFX_FORMAT_MAX
    } RPIGRAFX_FORMAT_T;

    /* main.c */
    void rpigrafx_init() __attribute__((constructor));
    void rpigrafx_finalize() __attribute__((destructor));

    /* dispmanx.c */
    void rpigrafx_get_screen_size(int *width, int *height);
    RPIGRAFX_ELEMENT_T rpigrafx_draw_box(const int x_start, const int y_start, const int x_end, const int y_end, const int border_width, const RPIGRAFX_COLOR_T color);
    RPIGRAFX_ELEMENT_T rpigrafx_render_image(void *image, const int x, const int y, const int width, const int height);
    RPIGRAFX_ELEMENT_T rpigrafx_render_image_scale(void *p, const int x, const int y, const int width, const int height, const int width_scaled, const int height_scaled);
    void rpigrafx_commit_drawings();
    void rpigrafx_remove_all_elements();

    /* mmal.c */
    void rpigrafx_set_camera_num(const int camera_num);
    void rpigrafx_set_frame_format(const RPIGRAFX_FORMAT_T format);
    void rpigrafx_get_frame_full_size(int *widthp, int *heightp);
    void rpigrafx_set_frame_size(const int width, const int height);
    void rpigrafx_ignite_capture();
    RPIGRAFX_ELEMENT_T rpigrafx_display_frame(const int x, const int y, const int width, const int height);
    void* rpigrafx_get_frame();

#endif /* RPIGRAFX_H */
