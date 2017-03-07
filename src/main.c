/*
 * Copyright (c) 2017 Sugizaki Yukimasa (ysugi@idein.jp)
 * All rights reserved.
 *
 * This software is licensed under a Modified (3-Clause) BSD License.
 * You should have received a copy of this license along with this
 * software. If not, contact the copyright holder above.
 */

#include "rpigrafx.h"
#include "local/init_finalize.h"
#include "local/error.h"

struct called called = {
    .main = 0,
    .dispmanx = 0,
    .mmal = 0
};

void rpigrafx_init()
{
    if (called.main != 0) {
        called.main ++;
        return;
    }

    local_rpigrafx_dispmanx_init();
    local_rpigrafx_mmal_init();

    if (called.dispmanx <= 0)
        error_and_exit("called.dispmanx is 0 or negative: %d\n", called.dispmanx);
    if (called.mmal <= 0)
        error_and_exit("called.mmal is 0 or negative: %d\n", called.mmal);

    called.main ++;
}

void rpigrafx_finalize()
{
    if (called.main != 1) {
        called.main --;
        return;
    }

    local_rpigrafx_mmal_finalize();
    local_rpigrafx_dispmanx_finalize();

    if (called.mmal != 0)
        error_and_exit("called.mmal is not 0: %d\n", called.mmal);
    if (called.dispmanx != 0)
        error_and_exit("called.dispmanx is not 0: %d\n", called.dispmanx);

    called.main --;
}
