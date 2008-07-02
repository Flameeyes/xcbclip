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

/* xcout() contexts */
#define XCLIB_XCOUT_NONE	0	/* no context */
#define XCLIB_XCOUT_SENTCONVSEL	1	/* sent a request */
#define XCLIB_XCOUT_INCR	2	/* in an incr loop */

/* xcin() contexts */
#define XCLIB_XCIN_NONE		0
#define XCLIB_XCIN_SELREQ	1
#define XCLIB_XCIN_INCR		2

/* functions in xclib.c */
extern int xcout(
	xcb_connection_t*,
	xcb_window_t,
	xcb_generic_event_t*,
	xcb_atom_t,
	char**,
	size_t*,
	uint32_t*
);
extern int xcin(
	xcb_connection_t*,
	xcb_window_t*,
	xcb_generic_event_t*,
	xcb_atom_t*,
	uint8_t*,
	size_t,
	size_t*,
	uint32_t*
);
extern void *xcmalloc(size_t);
extern void *xcrealloc(void*, size_t);
