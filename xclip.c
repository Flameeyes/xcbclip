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
#include <string.h>

#include <X11/Xresource.h>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include "xcdef.h"
#include "xcprint.h"
#include "xclib.h"
#include "xcb-contrib.h"

/* Options that get set on the command line */
int             sloop = 0;			/* number of loops */
char           *sdisp = NULL;			/* X display to connect to */
xcb_atom_t      sseln;				/* X selection to work with */

/* Flags for command line options */
static int      fverb = OSILENT;		/* output level */
static int      fdiri = true;			/* direction is in */
static int      ffilt = false;			/* filter mode */

static xcb_connection_t	*xconn;				/* connection to X11 display */
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
  /* command line option table for XrmParseCommand() */
  XrmOptionDescRec opt_tab[] = {
    /* loop option entry */
    {
      .option 	=	strdup("-loops"),
      .specifier=	strdup(".loops"),
      .argKind	=	XrmoptionSepArg,
      .value	=	(XPointer) NULL
    },

    /* display option entry */
    {
      .option	=	strdup("-display"),
      .specifier=	strdup(".display"),
      .argKind	=	XrmoptionSepArg,
      .value	=	(XPointer) NULL
    },
	
    /* selection option entry */
    {
      .option 	=	strdup("-selection"),
      .specifier=	strdup(".selection"),
      .argKind	=	XrmoptionSepArg,
      .value	=	(XPointer) NULL
    },
		
    /* filter option entry */
    {
      .option	=	strdup("-filter"),
      .specifier=	strdup(".filter"),
      .argKind	=	XrmoptionNoArg,
      .value	=	(XPointer) strdup(ST)
    },
		
    /* in option entry */
    {
      .option	=	strdup("-in"),
      .specifier=	strdup(".direction"),
      .argKind	=	XrmoptionNoArg,
      .value	=	(XPointer) strdup("I")
    },
		
    /* out option entry */
    {
      .option	=	strdup("-out"),
      .specifier=	strdup(".direction"),
      .argKind	=	XrmoptionNoArg,
      .value	=	(XPointer) strdup("O")
    },
		
    /* version option entry */
    {
      .option	=	strdup("-version"),
      .specifier=	strdup(".print"),
      .argKind	=	XrmoptionNoArg,
      .value	=	(XPointer) strdup("V")
    },
		
    /* help option entry */
    {
      .option	=	strdup("-help"),
      .specifier=	strdup(".print"),
      .argKind	=	XrmoptionNoArg,
      .value	=	(XPointer) strdup("H")
    },
		
    /* silent option entry */
    {
      .option	=	strdup("-silent"),
      .specifier=	strdup(".olevel"),
      .argKind	=	XrmoptionNoArg,
      .value	=	(XPointer) strdup("S")
    },
		
    /* quiet option entry */
    {
      .option	=	strdup("-quiet"),
      .specifier=	strdup(".olevel"),
      .argKind	=	XrmoptionNoArg,
      .value	=	(XPointer) strdup("Q")
    },
		
    /* verbose option entry */
    {
      .option	=	strdup("-verbose"),
      .specifier=	strdup(".olevel"),
      .argKind	=	XrmoptionNoArg,
      .value	=	(XPointer) strdup("V")
    }

  };

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
  sseln = PRIMARY;

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
				sseln = PRIMARY;
				break;
			case 's':
				sseln = SECONDARY;
				break;
			case 'c':
			  // TODO! FIX THIS
			  sseln = PRIMARY; // XA_CLIPBOARD(dpy);
				break;
			case 'b':
				sseln = STRING;
				break;
		}
    
		if (fverb == OVERBOSE)
		{
			fprintf(stderr, "Using selection: ");
	   
			if (sseln == PRIMARY)
				fprintf(stderr, "PRIMARY");
			if (sseln == SECONDARY)
				fprintf(stderr, "SECONDARY");
#if 0
			if (sseln == XA_CLIPBOARD(dpy))
				fprintf(stderr, "CLIPBOARD");
#endif

			if (sseln == STRING)
				fprintf(stderr, "STRING");

			fprintf(stderr, "\n");
		}
	}
}

static void doIn(xcb_window_t win, const char *progname)
{
  size_t sel_len = 0;	/* length of sel_buf */
  size_t sel_all = 16;	/* allocated size of sel_buf */
  int dloop = 0;	/* done loops counter */

  /* in mode */
  /* buffer for selection data */
  uint8_t *sel_buf = xcmalloc(sel_all * sizeof(char));

  /* Put chars into inc from stdin or files until we hit EOF */
  do {
    if (fil_number == 0) {
      /* read from stdin if no files specified */
      fil_handle = stdin;
    } else if (
	       (fil_handle = fopen(
				   fil_names[fil_current],
				   "r"
				   )) == NULL
	       ) {
      perrorf("%s: %s", progname, fil_names[fil_current]);
      exit(EXIT_FAILURE);
    } else {
      /* file opened successfully. Print
       * message (verbose mode only).
       */
      if (fverb == OVERBOSE)
	fprintf(stderr, "Reading %s...\n", fil_names[fil_current]);
    }

    fil_current++;
    while (!feof(fil_handle)) {
      /* If sel_buf is full (used elems =
       * allocated elems)
       */
      if (sel_len == sel_all) {
	/* double the number of
	 * allocated elements
	 */
	sel_all *= 2;
	sel_buf = xcrealloc(sel_buf, sel_all * sizeof(char));
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
  if ((fil_number == 0) && ffilt) {
    fwrite(sel_buf, sizeof(char), sel_len, stdout); 
    fclose(stdout);
  }

  /* Handle cut buffer if needed */
  if (sseln == STRING) {
    xcb_change_property_checked(xconn,
			XCB_PROP_MODE_REPLACE,
			win,
			CUT_BUFFER0,
			STRING,
			8,
			sel_len, sel_buf);
    return;
  }
	
  /* take control of the selection so that we receive
   * SelectionRequest events from other windows
   */
  xcb_void_cookie_t cookie = xcb_set_selection_owner_checked(xconn, win, sseln, XCB_CURRENT_TIME);

  xcb_perror(xconn, cookie, "cannot set selection owner");

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
  if (fverb > OSILENT) {
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
  while (dloop < sloop || sloop < 1) {
    /* print messages about what we're waiting for
     * if not in silent mode
     */
    if (fverb > OSILENT) {
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
    static unsigned int context = XCLIB_XCIN_NONE;
    static unsigned long sel_pos = 0;
    static xcb_window_t cwin;
    static xcb_atom_t pty;

    xcb_generic_event_t *event;
    bool clear = false;
    while ((event = xcb_wait_for_event(xconn))) {
      bool finished = xcin(
			   xconn,
			   &cwin,
			   event,
			   &pty,
			   sel_buf,
			   sel_len,
			   &sel_pos,
			   &context
			   );

      if ( (event->response_type & ~0x80) == XCB_SELECTION_CLEAR )
	clear = true;

      if ( (context == XCLIB_XCIN_NONE) && clear)
	return;

      if (finished)
	break;
    }

    dloop++;	/* increment loop counter */
  }
}

static void doOut(xcb_window_t win)
{
  char *sel_buf = NULL;	/* buffer for selection data */
  size_t sel_len = 0;		/* length of sel_buf */

  if (sseln == STRING) {
    uint8_t format; uint32_t prop_len;
    int res = xcb_get_text_property(xconn, win, CUT_BUFFER0,
				    &format, NULL, &prop_len, &sel_buf);

    if ( res == 0 || format != 8 ) {
      free(sel_buf);
      return;
    }

    sel_len = prop_len;
  } else {
    xcb_generic_event_t *event;
    unsigned int context = XCLIB_XCOUT_NONE;
    while (1) {
      /* only get an event if xcout() is doing something */
      if (context != XCLIB_XCOUT_NONE)
	event = xcb_wait_for_event(xconn);

      /* fetch the selection, or part of it */
      xcout(
	    xconn,
	    win,
	    event,
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

  fwrite(sel_buf, sizeof(char), sel_len, stdout);
  free(sel_buf);
}

int main (int argc, char *argv[])
{
  /* parse command line options */
  doOptMain(argc, argv);
  
  /* Connect to the X server. */
  if ( (xconn = xcb_connect(sdisp, NULL)) == NULL )
    /* couldn't connect to X server. Print error and exit */
    errxdisplay(sdisp);
  else
    /* successful */
    if (fverb == OVERBOSE)
      fprintf(stderr, "Connected to X server on %s.\n", sdisp);
    
  /* parse selection command line option */
  doOptSel();

  /* Get the first screen*/
  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(xconn)).data;

  /* Ask for a new window ID */
  xcb_window_t win = xcb_generate_id(xconn);

  static const uint32_t values[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };

  /* Create a window to trap events */
  xcb_void_cookie_t cookie = xcb_create_window(xconn,
			    XCB_COPY_FROM_PARENT,
			    win,
			    screen->root,
			    0, 0, 1, 1,
			    0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
			    screen->root_visual,
			    XCB_CW_EVENT_MASK, values);

  xcb_perror(xconn, cookie, "cannot create window");

  if (fdiri)
    doIn(win, argv[0]);
  else
    doOut(win);

  /* Disconnect from the X server */
  xcb_disconnect(xconn);
  
  /* exit */
  return(EXIT_SUCCESS);
}
