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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>

#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>

#include "xcbclip.h"

/* Options that get set on the command line */
int             sloop = 0;			/* number of loops */
char           *sdisp = NULL;			/* X display to connect to */
xcb_atom_t      sseln;				/* X selection to work with */

/* Flags for command line options */

/** Output verbosity level */
XcbClipVerboseLevel fverb = OSILENT;
/** Filter mode */
bool ffilt = false;

/** Direction (input if true, output if false) */
static bool fdiri = true;

/** XCB connection to the display */
xcb_connection_t *xconn;
/** xcbclip window ID */
xcb_window_t xwin;

/** argv[0], the name the program is invoked with */
const char *progname = NULL;

/** the start of the non-option parameters */
static char **params = NULL;
/** the number of non-option parameters */
static int params_count = 0;

/* Use XrmParseCommand to parse command line options to option variable */
static void doOptMain (int argc, char *argv[])
{
  static const char usageOutput[] =
    "Usage: %s [OPTION] [FILE]...\n"
    "Access an X server selection for reading or writing.\n"
    "\n"
    "  -i, --in         read text into X selection from standard input or"
                       "files\n"
    "                   (default)\n"
    "  -o, --out        prints the selection to standard out (generally for\n"
    "                   piping to a file or program)\n"
    "  -l, --loops      number of selection requests to "
                       "wait for before exiting\n"
    "  -d, --display    X display to connect to (eg "
                       "localhost:0\")\n"
    "  -h, --help       usage information\n"
    "      --selection  selection to access (\"p(rimary)\", "
                       "\"s(econdary)\", \"c(lipboard)\" or "
                       "\"b(uffer-cut)\")\n"
    "  -v, --version    version information\n"
    "  -S, --silent     errors only, run in background (default)\n"
    "  -Q, --quiet      run in foreground, show what's happening\n"
    "  -V, --verbose    running commentary\n"
    "\n"
    "Report bugs to Diego 'Flameeyes' Pettenò <flameeyes@gmail.com>\n";

  static const char versionOutput[] =
    PACKAGE_STRING " - command line interface to X selections" "\n"
    "Copyright (c) 2001 Kim Saunders" "\n"
    "Copyright (c) 2008 Diego 'Flameeyes' Pettenò" "\n"
    "\n"
    "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>" "\n"
    "This is free software: you are free to change and redistribute it." "\n"
    "There is NO WARRANTY, to the extent permitted by law." "\n";

  static const char optionsString[] = "l:d:s:fiovhSQV";
  static const struct option optionsTable[] = {
    { "loops",     required_argument, NULL,   'l'  },
    { "display",   required_argument, NULL,   'd'  },
    { "selection", required_argument, NULL,   's'  },
    { "filter",    no_argument,       NULL,   'f'  },
    { "in",        no_argument,       NULL,   'i'  },
    { "out",       no_argument,       NULL,   'o'  },
    { "version",   no_argument,       NULL,   'v'  },
    { "help",      no_argument,       NULL,   'h'  },
    { "silent",    no_argument,       NULL,   'S'  },
    { "quiet",     no_argument,       NULL,   'Q'  },
    { "verbose",   no_argument,       NULL,   'V'  },
    { NULL,        0,                 NULL,   '\0' }
  };

  int opt;
  while ((opt = getopt_long(argc, argv, optionsString, optionsTable, NULL)) >= 0) {
    switch (opt) {
    case 'l':
      assert(optarg != NULL);
      sloop = atoi(optarg);
      break;
    case 'd':
      assert(optarg != NULL);
      sdisp = strdup(optarg);
      break;
    case 's':
      assert(optarg != NULL);
      if ( strncasecmp(optarg, "p", 1) == 0 ) {
	sseln = PRIMARY;
      } else if ( strncasecmp(optarg, "s", 1) == 0 ) {
	sseln = SECONDARY;
      } else if ( strncasecmp(optarg, "c", 1) == 0 ) {
	fprintf(stderr, "%s: FIXME: clipboard not implemented\n", progname);
	exit(EXIT_FAILURE);
      } else if ( strncasecmp(optarg, "b", 1) == 0 ) {
	sseln = STRING;
      } else {
	fprintf(stderr, "%s: unknown selection %s", progname, optarg);
      }
      break;
    case 'f':
      ffilt = true;
      break;
    case 'i':
      fdiri = true;
      break;
    case 'o':
      fdiri = false;
      break;
    case 'h':
      printf(usageOutput, progname);
      exit(EXIT_SUCCESS);
      break; // useless
    case 'v':
      printf(versionOutput);
      exit(EXIT_SUCCESS);
      break; // useless
    case 'S':
      fverb = OSILENT;
      break;
    case 'Q':
      fverb = OQUIET;
      break;
    case 'V':
      fverb = OVERBOSE;
      break;
    }
  }

  if ( optind < argc ) {
    params = &argv[optind];
    params_count = argc - optind;
  }
}

/**
 * @brief Read a whole stream appending to the buffer
 * @param stream Stream to read from
 * @param buf Buffer to append the read data from
 * @param len Length of the user buffer
 * @param size Size allocated for the buffer
 */
static void read_all(FILE *stream, char **buf, size_t *len, size_t *size) {
  while(!feof(stream)) {
    assert(*len <= *size);
    if ( *len >= *size ) {
      /* double the allocated size of the buffer */
      *size *= 2;
      *buf = realloc(*buf, *size);
      if ( *buf == NULL ) {
	perrorf("%s: %s", progname, __FUNCTION__);
	exit(EXIT_FAILURE);
      }
    }

    *len += fread(*buf + *len, sizeof(char), *size - *len, stream);
  }
}

void get_input_buffer(char **out_buf, size_t *out_len)
{
  size_t len = 0;	/* length of sel_buf */
  size_t size = 16;	/* allocated size of sel_buf */

  char *buf = malloc(size);
  if ( buf == NULL ) {
    perrorf("%s: %s", progname, __FUNCTION__);
    exit(EXIT_FAILURE);
  }

  /* No files specified, use stdin */
  if ( params_count == 0 ) {
    read_all(stdin, &buf, &len, &size);
    
    /* if there are no files being read from (i.e., input
     * is from stdin not files, and we are in filter mode,
     * spit all the input back out to stdout
     */
    if (ffilt) {
      fwrite(buf, sizeof(char), len, stdout); 
      fclose(stdout);
    }
  } else {
    for(int i = 0; i < params_count; i++) {
      FILE *handler = fopen(params[i], "r");
      if ( handler == NULL ) {
	perrorf("%s: %s (%s)", progname, __FUNCTION__, params[i]);
	exit(EXIT_FAILURE);
      }

      if ( fverb == OVERBOSE )
	fprintf(stderr, "Reading %s...\n", params[i]);

      read_all(stdin, &buf, &len, &size);
    }
  }

  *out_buf = buf; *out_len = len;
}

int main (int argc, char *argv[])
{
  sseln = PRIMARY;

  progname = argv[0];
  /* parse command line options */
  doOptMain(argc, argv);

  if ( fverb == OVERBOSE ) {
    fprintf(stderr, "Usign selection: ");
    if ( sseln == PRIMARY )
      fprintf(stderr, "PRIMARY\n");
    else if ( sseln == SECONDARY )
      fprintf(stderr, "SECONDARY\n");
    // else if ( sseln == XA_CLIPBOARD(dpy) )
    //   fprintf(stderr, "CLIPBOARD\n");
    else if ( sseln == STRING )
      fprintf(stderr, "STRING\n");
    else
      assert ( 1 == 0 );
  }
  
  /* Connect to the X server. */
  if ( (xconn = xcb_connect(sdisp, NULL)) == NULL ) {
    /* couldn't connect to X server. Print error and exit */
    if (sdisp == NULL)
      sdisp = getenv("DISPLAY");
    
    fprintf(stderr, "%s: can't open display: %s\n", progname, sdisp);
    return EXIT_FAILURE;
  }

  /* successful */
  if (fverb == OVERBOSE)
    fprintf(stderr, "%s: connected to X server on %s.\n", progname, sdisp);
  
  /* Get the first screen*/
  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(xconn)).data;

  /* Ask for a new window ID */
  xwin = xcb_generate_id(xconn);

  static const uint32_t values[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };

  /* Create a window to trap events */
  xcb_void_cookie_t cookie = xcb_create_window(xconn,
			    XCB_COPY_FROM_PARENT,
			    xwin,
			    screen->root,
			    0, 0, 1, 1,
			    0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
			    screen->root_visual,
			    XCB_CW_EVENT_MASK, values);

  xcb_perror(cookie, "cannot create window");

  if (fdiri) {
    /* input */
    char *buffer = NULL;
    size_t len = 0;

    get_input_buffer(&buffer, &len);
    if ( sseln == STRING )
      do_in_string(buffer, len);
    else
      do_in(buffer, len);
  } else {
    if ( sseln == STRING )
      do_out_string();
    else
      do_out();
  }

  /* Disconnect from the X server */
  xcb_disconnect(xconn);
  
  /* exit */
  return(EXIT_SUCCESS);
}
