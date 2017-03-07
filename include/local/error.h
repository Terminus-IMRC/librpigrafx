/*
 * Copyright (c) 2017 Sugizaki Yukimasa (ysugi@idein.jp)
 * All rights reserved.
 *
 * This software is licensed under a Modified (3-Clause) BSD License.
 * You should have received a copy of this license along with this
 * software. If not, contact the copyright holder above.
 */

#ifndef LOCAL_ERROR_H
#define LOCAL_ERROR_H

    void error_and_exit_core(const char *file, const int line, const char *fmt, ...) __attribute__ ((noreturn));

#define error_and_exit(fmt, ...) error_and_exit_core(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif /* LOCAL_ERROR_H */
