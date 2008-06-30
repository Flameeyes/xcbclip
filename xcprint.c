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
#include "xcdef.h"
#include "xclib.h"
#include "xcprint.h"

/* print the help screen. argument is argv[0] from main() */
void prhelp (char *name)
{
	fprintf(
		stderr,
		"Usage: %s [OPTION] [FILE]...\n"\
		"Access an X server selection for reading or writing.\n"\
		"\n"\
		"  -i, -in          read text into X selection from standard input or files\n"\
		"                   (default)\n"\
		"  -o, -out         prints the selection to standard out (generally for\n"\
		"                   piping to a file or program)\n"\
		"  -l, -loops       number of selection requests to "\
		"wait for before exiting\n"\
		"  -d, -display     X display to connect to (eg "\
		"localhost:0\")\n"\
		"  -h, -help        usage information\n"\
		"      -selection   selection to access (\"primary\", "\
		"\"secondary\", \"clipboard\" or \"buffer-cut\")\n"\
		"      -version     version information\n"\
		"      -silent      errors only, run in background (default)\n"\
		"      -quiet       run in foreground, show what's happening\n"\
		"      -verbose     running commentary\n"\
		"\n"\
		"Report bugs to <flameeyes@gmail.com>\n",
		name
	);
	exit(EXIT_SUCCESS);
}


/* A function to print the software version info */
void prversion (void)
{
  fprintf(stdout,
	  PACKAGE_STRING " - command line interface to X selections" "\n"
	  "Copyright (c) 2001 Kim Saunders" "\n"
	  "Copyright (c) 2008 Diego 'Flameeyes' Pettenò" "\n"
	  "\n"
	  "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>" "\n"
	  "This is free software: you are free to change and redistribute it." "\n"
	  "There is NO WARRANTY, to the extent permitted by law." "\n"
	  );

  exit(EXIT_SUCCESS);
}

/* failure message for malloc() problems */
void errmalloc (void)
{
	fprintf(stderr, "Error: Could not allocate memory.\n");
	exit(EXIT_FAILURE);
}

/* failure to connect to X11 display */
void errxdisplay (char *display)
{
	/* if the display wasn't specified, read it from the enviroment
	 * just like XOpenDisplay would
	 */
	if (display == NULL)
		display = getenv("DISPLAY");
	
	fprintf(stderr, "Error: Can't open display: %s\n", display);
	exit(EXIT_FAILURE);
}

/* a wrapper for perror that joins multiple prefixes together. Arguments
 * are an integer, and any number of strings. The integer needs to be set to
 * the number of strings that follow.
 */
void errperror(char *format, ...)
{
	/* start looping through the viariable arguments */
	va_list ap;             /* argument pointer */
	va_start(ap, format);

	char *prefix = NULL;
	asprintf(&prefix, format, ap);
	va_end(ap);

	perror(prefix);

	/* free the complete string */
	free(prefix);
}
