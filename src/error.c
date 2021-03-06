/*
 * Copyright (c) 2017 Sugizaki Yukimasa (ysugi@idein.jp)
 * All rights reserved.
 *
 * This software is licensed under a Modified (3-Clause) BSD License.
 * You should have received a copy of this license along with this
 * software. If not, contact the copyright holder above.
 */

#include "rpigrafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void error_and_exit_core(const char *file, const int line, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s:%d: ", file, line);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}
