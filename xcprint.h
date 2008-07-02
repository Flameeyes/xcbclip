/*
 *  $Id: xcprint.h,v 1.3 2001/10/18 04:49:27 kims Exp $
 * 
 *  xcprint.h - header file for functions in xcprint.c
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

/* functions in xcprint.c */
extern void prhelp(char *);
extern void prversion(void);
extern void errmalloc(void);
extern void errxdisplay(char *);

void perrorf(char *format, ...)
#ifdef SUPPORT_ATTRIBUTE_FORMAT
  __attribute__((format(printf(1, 2))))
#endif
  ;

void xcb_perror(xcb_connection_t *xconn, xcb_void_cookie_t cookie, const char *errstr);
