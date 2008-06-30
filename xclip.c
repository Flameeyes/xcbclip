/*
 *  $Id: xclip.c,v 1.60 2001/11/12 00:00:21 kims Exp $
 * 
 *  xclip.c - command line interface to X server selections 
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
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h>
#include "xcdef.h"
#include "xcprint.h"
#include "xclib.h"

/* command line option table for XrmParseCommand() */
XrmOptionDescRec opt_tab[11];

/* Options that get set on the command line */
int             sloop = 0;			/* number of loops */
char           *sdisp = NULL;			/* X display to connect to */
Atom            sseln = XA_PRIMARY;		/* X selection to work with */

/* Flags for command line options */
static int      fverb = OSILENT;		/* output level */
static int      fdiri = true;			/* direction is in */
static int      ffilt = false;			/* filter mode */

Display	       *dpy;				/* connection to X11 display */
XrmDatabase     opt_db = NULL;			/* database for options */

char          **fil_names;			/* names of files to read */
int             fil_number = 0;                 /* number of files to read */
int		fil_current = 0;
FILE*           fil_handle = NULL;

/* variables to hold Xrm database record and type */
XrmValue        rec_val;
char           *rec_typ;

int		tempi = 0;

/* Use XrmParseCommand to parse command line options to option variable */
static void doOptMain (int argc, char *argv[])
{
	/* Initialise resource manager and parse options into database */
	XrmInitialize();
	XrmParseCommand(
		&opt_db,
		opt_tab,
		sizeof(opt_tab) / sizeof(opt_tab[0]),
		PACKAGE_NAME,
		&argc,
		argv
	);
  
	/* set output level */
	if (
		XrmGetResource(
			opt_db,
			PACKAGE_NAME ".olevel",
			PACKAGE_NAME ".Olevel",
			&rec_typ,
			&rec_val
		)
	)
	{
		/* set verbose flag according to option */
		if (strcmp(rec_val.addr, "S") == 0)
			fverb = OSILENT;
		if (strcmp(rec_val.addr, "Q") == 0)
			fverb = OQUIET;
		if (strcmp(rec_val.addr, "V") == 0)
			fverb = OVERBOSE;
	}
  
	/* set direction flag (in or out) */
	if (
		XrmGetResource(
			opt_db,
			PACKAGE_NAME ".direction",
			PACKAGE_NAME ".Direction",
			&rec_typ,
			&rec_val
		)
	)
	{
		if (strcmp(rec_val.addr, "I") == 0)
			fdiri = true;
		if (strcmp(rec_val.addr, "O") == 0)
			fdiri = false;
	}
  
	/* set filter mode */
	if (
		XrmGetResource(
			opt_db,
			PACKAGE_NAME ".filter",
			PACKAGE_NAME ".Filter",
			&rec_typ,
			&rec_val
		)
	)
	{
		/* filter mode only allowed in silent mode */
		if (fverb == OSILENT)
			ffilt = true;
	}
  
	/* check for -help and -version */
	if (
		XrmGetResource(
			opt_db,
			PACKAGE_NAME ".print",
			PACKAGE_NAME ".Print",
			&rec_typ,
			&rec_val
		)
	)
	{
		if (strcmp(rec_val.addr, "H") == 0)
			prhelp(argv[0]);
		if (strcmp(rec_val.addr, "V") == 0)
			prversion();
	}
  
	/* check for -display */
	if (
		XrmGetResource(
			opt_db,
			PACKAGE_NAME ".display",
			PACKAGE_NAME ".Display",
			&rec_typ,
			&rec_val
		)
	)
	{
		sdisp = rec_val.addr;
		if (fverb == OVERBOSE)	/* print in verbose mode only */
			fprintf(stderr, "Display: %s\n", sdisp);
	}
  
	/* check for -loops */
	if (
		XrmGetResource(
			opt_db,
			PACKAGE_NAME ".loops",
			PACKAGE_NAME ".Loops",
			&rec_typ,
			&rec_val
		)
	)
	{
		sloop = atoi(rec_val.addr);
		if (fverb == OVERBOSE)	/* print in verbose mode only */
			fprintf(stderr, "Loops: %i\n", sloop);
	}

	/* Read remaining options (filenames) */
	while ( (fil_number + 1) < argc )
	{
		if (fil_number > 0)
		{
			fil_names = xcrealloc(
				fil_names,
				(fil_number + 1) * sizeof(char*)
			);
		} else
		{
			fil_names = xcmalloc(sizeof(char*));
		}
		fil_names[fil_number] = argv[fil_number + 1];
		fil_number++;
	}
}

/* process selection command line option */
static void doOptSel (void)
{
	/* set selection to work with */
	if (
		XrmGetResource(
			opt_db,
			PACKAGE_NAME ".selection",
			PACKAGE_NAME ".Selection",
			&rec_typ,
			&rec_val
		)
	)
	{
		switch (tolower(rec_val.addr[0]))
		{
			case 'p':
				sseln = XA_PRIMARY;
				break;
			case 's':
				sseln = XA_SECONDARY;
				break;
			case 'c':
				sseln = XA_CLIPBOARD(dpy);
				break;
			case 'b':
				sseln = XA_STRING;
				break;
		}
    
		if (fverb == OVERBOSE)
		{
			fprintf(stderr, "Using selection: ");
	   
			if (sseln == XA_PRIMARY)
				fprintf(stderr, "XA_PRIMARY");
			if (sseln == XA_SECONDARY)
				fprintf(stderr, "XA_SECONDARY");
			if (sseln == XA_CLIPBOARD(dpy))
				fprintf(stderr, "XA_CLIPBOARD");
			if (sseln == XA_STRING)
				fprintf(stderr, "XA_STRING");

			fprintf(stderr, "\n");
		}
	}
}

static void doIn(Window win, const char *progname)
{
	unsigned char *sel_buf;		/* buffer for selection data */
	unsigned long sel_len = 0;	/* length of sel_buf */
	unsigned long sel_all = 0;	/* allocated size of sel_buf */
	XEvent evt;			/* X Event Structures */
	int dloop = 0;			/* done loops counter */

	/* in mode */
	sel_all = 16;		/* Reasonable ballpark figure */
	sel_buf = xcmalloc(sel_all * sizeof(char));

	/* Put chars into inc from stdin or files until we hit EOF */
	do {
		if (fil_number == 0)
		{
			/* read from stdin if no files specified */
			fil_handle = stdin;
		} else
		{
			if (
					(fil_handle = fopen(
										fil_names[fil_current],
										"r"
									   )) == NULL
			   )
			{
			  perrorf("%s: %s", progname, fil_names[fil_current]);
			  exit(EXIT_FAILURE);
			} else
			{
				/* file opened successfully. Print
				 * message (verbose mode only).
				 */
				if (fverb == OVERBOSE)
					fprintf(
							stderr,
							"Reading %s...\n",
							fil_names[fil_current]
						   );
			}
		}

		fil_current++;
		while (!feof(fil_handle))
		{
			/* If sel_buf is full (used elems =
			 * allocated elems)
			 */
			if (sel_len == sel_all)
			{
				/* double the number of
				 * allocated elements
				 */
				sel_all *= 2;
				sel_buf = (unsigned char *)xcrealloc(
						sel_buf,
						sel_all * sizeof(char)
						);
			}
			sel_len += fread(
					sel_buf + sel_len,
					sizeof(char),
					sel_all - sel_len,
					fil_handle
					);
		}
	} while (fil_current < fil_number);

	/* if there are no files being read from (i.e., input
	 * is from stdin not files, and we are in filter mode,
	 * spit all the input back out to stdout
	 */
	if ((fil_number == 0) && ffilt)
	{
		fwrite(sel_buf, sizeof(char), sel_len, stdout); 
		fclose(stdout);
	}

	/* Handle cut buffer if needed */
	if (sseln == XA_STRING)
	{
		XStoreBuffer(dpy, (char *)sel_buf, (int)sel_len, 0);
		return;
	}
	
	/* take control of the selection so that we receive
	 * SelectionRequest events from other windows
	 */
	XSetSelectionOwner(dpy, sseln, win, CurrentTime);

	/* fork into the background, exit parent process if we
	 * are in silent mode
	 */
	if (fverb == OSILENT)
	{
		pid_t pid;

		pid = fork();
		/* exit the parent process; */
		if (pid)
			exit(EXIT_SUCCESS);
	}

	/* print a message saying what we're waiting for */
	if (fverb > OSILENT)
	{
		if (sloop == 1)
			fprintf(
					stderr,
					"Waiting for one selection request.\n"
				   );

		if (sloop  < 1)
			fprintf(
					stderr,
					"Waiting for selection requests, Control-C to quit\n"
				   );

		if (sloop  > 1)
			fprintf(
					stderr,
					"Waiting for %i selection requests, Control-C to quit\n",
					sloop
				   );
	}

	/* Avoid making the current directory in use, in case it will need to be umounted */
	chdir("/");
	
	/* loop and wait for the expected number of
	 * SelectionRequest events
	 */
	while (dloop < sloop || sloop < 1)
	{
		/* print messages about what we're waiting for
		 * if not in silent mode
		 */
		if (fverb > OSILENT)
		{
			if (sloop  > 1)
				fprintf(
						stderr,
						"  Waiting for selection request %i of %i.\n",
						dloop + 1,
						sloop
					   );

			if (sloop == 1)
				fprintf(
						stderr,
						"  Waiting for a selection request.\n"
					   );

			if (sloop  < 1)
				fprintf(
						stderr,
						"  Waiting for selection request number %i\n",
						dloop + 1
					   );
		}

		/* wait for a SelectionRequest event */
		while (1)
		{
			static unsigned int clear = 0;
			static unsigned int context = XCLIB_XCIN_NONE;
			static unsigned long sel_pos = 0;
			static Window cwin;
			static Atom pty;
			int finished;

			XNextEvent(dpy, &evt);

			finished = xcin(
					dpy,
					&cwin,
					evt,
					&pty,
					sel_buf,
					sel_len,
					&sel_pos,
					&context
					);

			if (evt.type == SelectionClear)
				clear = 1;

			if ( (context == XCLIB_XCIN_NONE) && clear)
				exit(EXIT_SUCCESS);

			if (finished)
				break;
		}

		dloop++;	/* increment loop counter */
	}
}

static void doOut(Window win)
{
	unsigned char *sel_buf;		/* buffer for selection data */
	unsigned long sel_len = 0;	/* length of sel_buf */
	XEvent evt;			/* X Event Structures */
	unsigned int context = XCLIB_XCOUT_NONE;

	if (sseln == XA_STRING)
		sel_buf = (unsigned char *)XFetchBuffer(dpy, (int *)&sel_len, 0);
	else
	{
		while (1)
		{
			/* only get an event if xcout() is doing something */
			if (context != XCLIB_XCOUT_NONE)
				XNextEvent(dpy, &evt);

			/* fetch the selection, or part of it */
			xcout(
					dpy,
					win,
					evt,
					sseln,
					&sel_buf,
					&sel_len,
					&context
				 );

			/* only continue if xcout() is doing something */
			if (context == XCLIB_XCOUT_NONE)
				break;
		}
	}

	if (sel_len)
	{
		/* only print the buffer out, and free it, if it's not
		 * empty
		 */
		fwrite(sel_buf, sizeof(char), sel_len, stdout);
		if (sseln == XA_STRING)
			XFree(sel_buf);
		else
			free(sel_buf);
	}
}

int main (int argc, char *argv[])
{
	/* Declare variables */
	Window win;			/* Window */

	/* set up option table. I can't figure out a better way than this to
	 * do it while sticking to pure ANSI C. The option and specifier
	 * members have a type of volatile char *, so they need to be allocated
	 * by strdup or malloc, you can't set them to a string constant at
	 * declare time, this is note pure ANSI C apparently, although it does
	 * work with gcc
	 */
	
	/* loop option entry */
	opt_tab[0].option 	=	strdup("-loops");
	opt_tab[0].specifier	=	strdup(".loops");
	opt_tab[0].argKind	=	XrmoptionSepArg;
	opt_tab[0].value	=	(XPointer) NULL;

	/* display option entry */
	opt_tab[1].option	=	strdup("-display");
	opt_tab[1].specifier	=	strdup(".display");
	opt_tab[1].argKind	=	XrmoptionSepArg;
	opt_tab[1].value	=	(XPointer) NULL;
				
	/* selection option entry */
	opt_tab[2].option 	=	strdup("-selection");
	opt_tab[2].specifier	=	strdup(".selection");
	opt_tab[2].argKind	=	XrmoptionSepArg;
	opt_tab[2].value	=	(XPointer) NULL;
		
	/* filter option entry */
	opt_tab[3].option	=	strdup("-filter");
	opt_tab[3].specifier	=	strdup(".filter");
	opt_tab[3].argKind	=	XrmoptionNoArg;	
	opt_tab[3].value	=	(XPointer) strdup(ST);
		
	/* in option entry */
	opt_tab[4].option	=	strdup("-in");
	opt_tab[4].specifier	=	strdup(".direction");
	opt_tab[4].argKind	=	XrmoptionNoArg;
	opt_tab[4].value	=	(XPointer) strdup("I");
		
	/* out option entry */
	opt_tab[5].option	=	strdup("-out");
	opt_tab[5].specifier	=	strdup(".direction");
	opt_tab[5].argKind	=	XrmoptionNoArg;
	opt_tab[5].value	=	(XPointer) strdup("O");
		
	/* version option entry */
	opt_tab[6].option	=	strdup("-version");
	opt_tab[6].specifier	=	strdup(".print");
	opt_tab[6].argKind	=	XrmoptionNoArg;
	opt_tab[6].value	=	(XPointer) strdup("V");
		
	/* help option entry */
	opt_tab[7].option	=	strdup("-help");
	opt_tab[7].specifier	=	strdup(".print");
	opt_tab[7].argKind	=	XrmoptionNoArg;
	opt_tab[7].value	=	(XPointer) strdup("H");
		
	/* silent option entry */
	opt_tab[8].option	=	strdup("-silent");
	opt_tab[8].specifier	=	strdup(".olevel");
	opt_tab[8].argKind	=	XrmoptionNoArg;
	opt_tab[8].value	=	(XPointer) strdup("S");
		
	/* quiet option entry */
	opt_tab[9].option	=	strdup("-quiet");
	opt_tab[9].specifier	=	strdup(".olevel");
	opt_tab[9].argKind	=	XrmoptionNoArg;
	opt_tab[9].value	=	(XPointer) strdup("Q");
		
	/* verbose option entry */
	opt_tab[10].option	=	strdup("-verbose");
	opt_tab[10].specifier	=	strdup(".olevel");
	opt_tab[10].argKind	=	XrmoptionNoArg;
	opt_tab[10].value	=	(XPointer) strdup("V");

	/* parse command line options */
	doOptMain(argc, argv);
   
	/* Connect to the X server. */
	if ( (dpy = XOpenDisplay(sdisp)) )
	{
		/* successful */
		if (fverb == OVERBOSE)
			fprintf(stderr, "Connected to X server.\n");
	} else
	{
		/* couldn't connect to X server. Print error and exit */
		errxdisplay(sdisp);
	}

	/* parse selection command line option */
	doOptSel();
  
	/* Create a window to trap events */
	win = XCreateSimpleWindow(
		dpy,
		DefaultRootWindow(dpy),
		0,
		0,
		1,
		1,
		0,
		0,
		0
	);

	/* get events about property changes */
	XSelectInput(dpy, win, PropertyChangeMask);
  
	if (fdiri)
		doIn(win, argv[0]);
	else
		doOut(win);

	/* Disconnect from the X server */
	XCloseDisplay(dpy);

	/* exit */
	return(EXIT_SUCCESS);
}
