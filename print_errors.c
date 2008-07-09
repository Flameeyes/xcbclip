/*
 *  $Id: xcprint.c,v 1.13 2001/12/17 06:14:40 kims Exp $
 * 
 *  xcprint.c - functions to print help, version, errors, etc
 *  Copyright (c) 2001 Kim Saunders
 *  Copyright (c) 2008 Diego 'Flameeyes' Pettenò
 *
 *  This file is part of xcbclip.
 *
 *  xcbclip is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  xcbclip is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with xcbclip.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "xcbclip.h"

/* a wrapper for perror that joins multiple prefixes together. Arguments
 * are an integer, and any number of strings. The integer needs to be set to
 * the number of strings that follow.
 */
void perrorf(const char *format, ...)
{
	/* start looping through the viariable arguments */
	va_list ap;             /* argument pointer */
	va_start(ap, format);

	char *prefix = NULL;
	vasprintf(&prefix, format, ap);
	va_end(ap);

	perror(prefix);

	/* free the complete string */
	free(prefix);
}

void xcb_perror(xcb_void_cookie_t cookie, const char *errstr) {
  xcb_generic_error_t *error = xcb_request_check(xconn, cookie);
  if ( error == NULL )
    return;
  
  fprintf(stderr, "ERROR: %s: %d\n", errstr, error->error_code);
  xcb_disconnect(xconn);
  abort();
}
