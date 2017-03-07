/*
 * Copyright (c) 2017 Sugizaki Yukimasa (ysugi@idein.jp)
 * All rights reserved.
 *
 * This software is licensed under a Modified (3-Clause) BSD License.
 * You should have received a copy of this license along with this
 * software. If not, contact the copyright holder above.
 */

#ifndef LOCAL_INIT_FINALIZE_H
#define LOCAL_INIT_FINALIZE_H

    extern struct called {
        int main, dispmanx, mmal;
    } called;

    void local_rpigrafx_dispmanx_init();
    void local_rpigrafx_dispmanx_finalize();
    void local_rpigrafx_mmal_init();
    void local_rpigrafx_mmal_finalize();

#endif /* LOCAL_INIT_FINALIZE_H */
