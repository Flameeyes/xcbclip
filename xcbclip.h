/*
 *  $Id: xclib.h,v 1.8 2001/11/11 23:56:33 kims Exp $
 * 
 *  xclib.h - header file for functions in xclib.c
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

#include <xcb/xcb.h>

/* output level constants */
typedef enum {
  OSILENT,
  OQUIET,
  OVERBOSE
} XcbClipVerboseLevel;

/* maximume size to read/write to/from a property at once in bytes */
#define XC_CHUNK 4096

extern int sloop;
extern char *sdisp;
extern xcb_atom_t sseln;

extern int fverb;
extern int ffilt;

extern xcb_connection_t *xconn;
extern xcb_window_t xwin;

extern const char *progname;

extern char **params;
extern int params_count;

/* xclib.c */
void doIn();
void doOut();

/* print_errors.c */
void perrorf(const char *format, ...)
#ifdef SUPPORT_ATTRIBUTE_FORMAT
  __attribute__((format(printf, 1, 2)))
#endif
  ;

void xcb_perror(xcb_void_cookie_t cookie, const char *errstr);
