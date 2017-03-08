/*
 * Copyright (c) 2017 Sugizaki Yukimasa (ysugi@idein.jp)
 * All rights reserved.
 *
 * This software is licensed under a Modified (3-Clause) BSD License.
 * You should have received a copy of this license along with this
 * software. If not, contact the copyright holder above.
 */

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_logging.h>
#include <interface/mmal/util/mmal_connection.h>
#include <interface/mmal/util/mmal_util_params.h>
#include <interface/mmal/util/mmal_component_wrapper.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <stdio.h>
#include "rpigrafx.h"
#include "local/init_finalize.h"
#include "local/error.h"


static MMAL_WRAPPER_T *cpw_camera = NULL;
static MMAL_WRAPPER_T *cpw_null = NULL, *cpw_resize = NULL;
static MMAL_CONNECTION_T *connection_camera_resize = NULL, *connection_preview_null = NULL;

static int num_cameras = 0;
static MMAL_PARAMETER_CAMERA_INFO_CAMERA_T camera_info_cameras[MMAL_PARAMETER_CAMERA_INFO_MAX_CAMERAS];

static MMAL_BUFFER_HEADER_T *header_frame_full = NULL, *header_frame = NULL;
static void *frame = NULL, *frame_full = NULL;
static int frame_full_width = 0, frame_full_height = 0;
static int frame_width = 0, frame_height = 0;
static RPIGRAFX_FORMAT_T frame_encoding = RPIGRAFX_FORMAT_RGBA32;

static _Bool is_capture_ignited = 0, is_frame_full_ready = 0, is_frame_ready = 0;
static _Bool is_no_resize = 1;


#define _check(x) \
    do { \
        int ret = (x); \
        if (ret != MMAL_SUCCESS) \
            error_and_exit("MMAL assertation failed: 0x%08x\n", ret); \
    } while (0)


static void config_port(MMAL_PORT_T *port, const MMAL_FOURCC_T encoding, const int width, const int height)
{
    port->format->encoding = encoding;
    port->format->es->video.width  = VCOS_ALIGN_UP(width,  32);
    port->format->es->video.height = VCOS_ALIGN_UP(height, 16);
    port->format->es->video.crop.x = 0;
    port->format->es->video.crop.y = 0;
    port->format->es->video.crop.width  = width;
    port->format->es->video.crop.height = height;
    _check(mmal_port_format_commit(port));
}

static void config_camera_output(const MMAL_FOURCC_T encoding, const int width, const int height)
{
    config_port(cpw_camera->output[2], encoding, width, height);
}

static void config_resize_input(const MMAL_FOURCC_T encoding, const int width, const int height)
{
    config_port(cpw_resize->input[0], encoding, width, height);
}

static void config_resize_output(const MMAL_FOURCC_T encoding, const int width, const int height)
{
    config_port(cpw_resize->output[0], encoding, width, height);
}

static MMAL_BUFFER_HEADER_T* get_full_header(MMAL_PORT_T *port)
{
    MMAL_BUFFER_HEADER_T *header = NULL;
    for (; ; ) {
        while (mmal_wrapper_buffer_get_empty(port, &header, 0) == MMAL_SUCCESS)
            _check(mmal_port_send_buffer(port, header));
        _check(mmal_wrapper_buffer_get_full(port, &header, MMAL_WRAPPER_FLAG_WAIT));
        if (header->flags & (MMAL_BUFFER_HEADER_FLAG_EOS | MMAL_BUFFER_HEADER_FLAG_FRAME_END))
            break;
        mmal_buffer_header_release(header);
    }
    return header;
}

static void get_frame_full()
{
    MMAL_PORT_T *output = cpw_camera->output[2];

    if (is_frame_full_ready)
        return;
    if (!is_capture_ignited)
        rpigrafx_ignite_capture();

    header_frame_full = get_full_header(output);
    frame_full = header_frame_full->data;
    is_frame_full_ready = 1;
}


void local_rpigrafx_mmal_init()
{
    if (called.mmal != 0)
        goto skip;

    bcm_host_init();

    {
        MMAL_COMPONENT_T *cp_camera_info = NULL;
        MMAL_PARAMETER_CAMERA_INFO_T camera_info;

        _check(mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &cp_camera_info));

        camera_info.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
        camera_info.hdr.size = sizeof(camera_info);
        _check(mmal_port_parameter_get(cp_camera_info->control, &camera_info.hdr));

        num_cameras = camera_info.num_cameras;
        if (num_cameras <= 0)
            error_and_exit("No cameras found: %d\n", num_cameras);
        memcpy(camera_info_cameras, camera_info.cameras, sizeof(camera_info_cameras));
        /* Use camera 0 as default. */
        frame_full_width  = camera_info_cameras[0].max_width;
        frame_full_height = camera_info_cameras[0].max_height;

        _check(mmal_component_destroy(cp_camera_info));
    }
    frame_full_width = frame_full_height = 512;

    _check(mmal_wrapper_create(&cpw_camera, MMAL_COMPONENT_DEFAULT_CAMERA));
    _check(mmal_wrapper_create(&cpw_null, "vc.null_sink"));
    _check(mmal_connection_create(
            &connection_preview_null,
            cpw_camera->output[0], cpw_null->input[0],
            MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT
    ));
    _check(mmal_connection_enable(connection_preview_null));
    config_camera_output(MMAL_ENCODING_RGBA, frame_full_width, frame_full_height);
    //_check(mmal_wrapper_port_enable(cpw_camera->output[2], MMAL_WRAPPER_FLAG_PAYLOAD_ALLOCATE | MMAL_WRAPPER_FLAG_PAYLOAD_USE_SHARED_MEMORY));
    _check(mmal_wrapper_port_enable(cpw_camera->output[2], MMAL_WRAPPER_FLAG_PAYLOAD_ALLOCATE));

    frame_width  = frame_full_width;
    frame_height = frame_full_height;

skip:
    called.mmal ++;
}

void local_rpigrafx_mmal_finalize()
{
    if (called.mmal != 1)
        goto skip;

    if (connection_camera_resize != NULL)
        _check(mmal_connection_destroy(connection_camera_resize));
    if (connection_preview_null != NULL)
        _check(mmal_connection_destroy(connection_preview_null));
    connection_camera_resize = NULL;
    connection_preview_null = NULL;

    if (cpw_resize != NULL)
        _check(mmal_wrapper_destroy(cpw_resize));
    if (cpw_null != NULL)
        _check(mmal_wrapper_destroy(cpw_null));
    if (cpw_camera != NULL)
        _check(mmal_wrapper_destroy(cpw_camera));
    cpw_resize = NULL;
    cpw_null = NULL;
    cpw_camera = NULL;

    num_cameras = 0;

    frame_full_width = frame_full_height = frame_width = frame_height = 0;
    if (header_frame_full != NULL)
        mmal_buffer_header_release(header_frame_full);
    if (header_frame != NULL)
        mmal_buffer_header_release(header_frame);
    header_frame_full = header_frame = NULL;

    is_capture_ignited = 0;
    is_frame_full_ready = is_frame_ready = 0;
    is_no_resize = 1;

    bcm_host_deinit();

skip:
    called.mmal --;
}


void rpigrafx_set_camera_num(const int camera_num)
{
    MMAL_PARAMETER_INT32_T param = {
        {MMAL_PARAMETER_CAMERA_NUM, sizeof(param)},
        camera_num
    };
    _check(mmal_port_parameter_set(cpw_camera->control, &param.hdr));

    frame_full_width  = camera_info_cameras[camera_num].max_width;
    frame_full_height = camera_info_cameras[camera_num].max_height;
    rpigrafx_set_frame_size(frame_width, frame_height);
}

void rpigrafx_set_frame_format(const RPIGRAFX_FORMAT_T format)
{
    if (format != RPIGRAFX_FORMAT_RGBA32)
        error_and_exit("We only support RGBA32 for now\n");
}

void rpigrafx_get_frame_full_size(int *widthp, int *heightp)
{
    *widthp  = frame_full_width;
    *heightp = frame_full_height;
}

void rpigrafx_set_frame_size(const int width, const int height)
{
    frame_width  = width;
    frame_height = height;

    if (frame_width == frame_full_width && frame_height == frame_full_height) {
        is_no_resize = 1;
        if (cpw_resize != NULL) {
            _check(mmal_connection_destroy(connection_camera_resize));
            connection_camera_resize = NULL;
            _check(mmal_wrapper_destroy(cpw_resize));
            cpw_resize = NULL;
        }
        return;
    }

    is_no_resize = 0;

    if (cpw_resize == NULL)
        _check(mmal_wrapper_create(&cpw_resize, "vc.ril.isp"));
    config_resize_input(MMAL_ENCODING_RGBA, frame_full_width, frame_full_height);
    config_resize_output(frame_encoding, frame_width, frame_height);

    if (connection_camera_resize != NULL)
        _check(mmal_connection_destroy(connection_camera_resize));
    _check(mmal_connection_create(
            &connection_camera_resize,
            cpw_camera->output[2], cpw_resize->input[0],
            MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT
    ));
    _check(mmal_connection_enable(connection_camera_resize));
}


void rpigrafx_ignite_capture()
{
    _check(mmal_port_parameter_set_boolean(cpw_camera->output[2], MMAL_PARAMETER_CAPTURE, 1));
    if (header_frame_full != NULL)
        mmal_buffer_header_release(header_frame_full);
    if (header_frame != NULL)
        mmal_buffer_header_release(header_frame);
    header_frame_full = header_frame = NULL;
    frame_full = frame = NULL;
    is_capture_ignited = 1;
    is_frame_full_ready = is_frame_ready = 0;
}

RPIGRAFX_ELEMENT_T rpigrafx_display_frame(const int x, const int y, const int width, const int height)
{
    get_frame_full();
    return rpigrafx_render_image_scale(frame_full, x, y, frame_full_width, frame_full_height, width, height);
}

void* rpigrafx_get_frame()
{
    MMAL_PORT_T *input = NULL, *output = NULL;
    MMAL_BUFFER_HEADER_T *header = NULL;

    get_frame_full();

    if (is_no_resize)
        return frame_full;

    if (!is_frame_ready) {
        input  = cpw_resize->input[0];
        output = cpw_resize->output[0];

        header->data = frame_full;
        header->length = frame_full_width * frame_full_height * 4;
        header->flags = MMAL_BUFFER_HEADER_FLAG_EOS;
        _check(mmal_port_send_buffer(input, header));

        header_frame = get_full_header(output);
        frame = header_frame->data;
        is_frame_ready = 1;
    }

    return frame;
}
