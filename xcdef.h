/*
 *  $Id: xcdef.h,v 1.8 2001/12/17 06:14:40 kims Exp $
 * 
 *  xcdef.h - definitions for use throughout xclip
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

/* output level constants */
#define OSILENT  0
#define OQUIET   1
#define OVERBOSE 2

/* true/false string constants */
#define SF "F"	/* false */
#define ST "T"	/* true  */

/* maximume size to read/write to/from a property at once in bytes */
#define XC_CHUNK 4096
