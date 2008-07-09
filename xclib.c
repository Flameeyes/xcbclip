/*
 *  $Id: xclib.c,v 1.12 2001/12/17 06:14:40 kims Exp $
 * 
 *  xclib.c - xclip library to look after xlib mechanics for xclip
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_property.h>
#include "xcbclip.h"
#include "xcb-contrib.h"

typedef enum {
  XCLIP_OUT_NONE,        /**< no context */
  XCLIP_OUT_SENTCONVSEL, /**< sent a request */
  XCLIP_OUT_INCR         /**< in an incr loop */
} XClipOutContext;

typedef enum {
  XCLIP_IN_NONE,
  XCLIP_IN_SELREQ,
  XCLIP_IN_INCR
} XClipInContext;

static xcb_atom_t incr_atom;
static xcb_atom_t targets_atom;
static xcb_atom_t xclip_out_atom;

static void find_internal_atoms() {
  static bool executed = false;
  if ( executed ) return;

  {
    const intern_atom_fast_cookie_t cookie = intern_atom_fast(xconn, false, sizeof("INCR") -1, "INCR");
    incr_atom = intern_atom_fast_reply(xconn, cookie, 0);
  }

  {
    const intern_atom_fast_cookie_t cookie = intern_atom_fast(xconn, false, sizeof("XCLIP_OUT") -1, "XCLIP_OUT");
    xclip_out_atom = intern_atom_fast_reply(xconn, cookie, 0);
  }

  {
    const intern_atom_fast_cookie_t cookie = intern_atom_fast(xconn, false, sizeof("TARGETS") -1, "TARGETS");
    targets_atom = intern_atom_fast_reply(xconn, cookie, 0);
  }

  executed = true;
}

/* put data into a selection, in response to a SelecionRequest event from
 * another window (and any subsequent events relating to an INCR transfer).
 *
 * Arguments are:
 *
 * A display
 * 
 * A window
 * 
 * The event to respond to
 * 
 * A pointer to an Atom. This gets set to the property nominated by the other
 * app in it's SelectionRequest. Things are likely to break if you change the
 * value of this yourself.
 * 
 * A pointer to an array of chars to read selection data from.
 * 
 * The length of the array of chars.
 *
 * In the case of an INCR transfer, the position within the array of chars
 * that is being processed.
 *
 * The context that event is the be processed within.
 */
static int doIn_internal_loop(
	 xcb_window_t* win,
	 xcb_generic_event_t* evt,
	 xcb_atom_t* pty,
	 char* txt,
	 size_t len,
	 size_t* pos,
	 uint32_t* context
)
{
  unsigned long chunk_len;	/* length of current chunk (for incr
				 * transfers only)
				 */

  xcb_void_cookie_t cookie;
  switch (*context) {
  case XCLIP_IN_NONE: {
    if ((evt->response_type & ~0x80) != XCB_SELECTION_REQUEST)
      return 0;

    xcb_selection_request_event_t *req_event = (xcb_selection_request_event_t *)evt;
    
    /* set the window and property that is being used */
    *win = req_event->requestor;
    *pty = req_event->property;

    /* reset position to 0 */
    *pos = 0;
		
    /* put the data into an property */
    if (req_event->target == targets_atom) {
      xcb_atom_t types[] = { targets_atom, STRING };
			
      /* send data all at once (not using INCR) */
      cookie = xcb_change_property_checked(xconn,
			  XCB_PROP_MODE_REPLACE,
			  *win,
			  *pty,
			  targets_atom,
			  8,
			  sizeof(types), types);
    } else if (len > XC_CHUNK) {
      /* send INCR response */
      cookie = xcb_change_property_checked(xconn,
			  XCB_PROP_MODE_REPLACE,
			  *win,
			  *pty,
			  incr_atom,
			  32,
			  0, NULL);

	*context = XCLIP_IN_INCR;
    } else {
      /* send data all at once (not using INCR) */
      cookie = xcb_change_property_checked(xconn,
			  XCB_PROP_MODE_REPLACE,
			  *win,
			  *pty,
			  STRING,
			  8,
			  len, txt);
    }

    xcb_perror(cookie, "cannot set data into property");

    {
      /* response to event */
      xcb_selection_notify_event_t res = {
	.response_type = XCB_SELECTION_NOTIFY,
	.pad0 = 0,
	.sequence = 0,
	.time = XCB_CURRENT_TIME,
	.requestor = *win,
	.selection = req_event->selection,
	.target = req_event->target,
	.property = *pty
      };

      cookie = xcb_send_event_checked(xconn, false, req_event->requestor, 0, (char*)&res);
    }

    xcb_perror(cookie, "cannot set selection notify");

    /* if len < XC_CHUNK, then the data was sent all at
     * once and the transfer is now complete, return 1
     */
    return !(len > XC_CHUNK);

  case XCLIP_IN_INCR:
    /* length of current chunk */

    /* ignore non-property events */
    if ((evt->response_type & ~0x80) != XCB_PROPERTY_NOTIFY)
      return 0;

    xcb_property_notify_event_t *notify_event = (xcb_property_notify_event_t *)evt;

    /* ignore the event unless it's to report that the
     * property has been deleted
     */
    if (notify_event->state != XCB_PROPERTY_NOTIFY)
      return 0;

    /* set the chunk length to the maximum size */
    chunk_len = XC_CHUNK;

    /* if a chunk length of maximum size would extend
     * beyond the end ot txt, set the length to be the
     * remaining length of txt
     */
    if ( (*pos + chunk_len) > len )
      chunk_len = len - *pos;

    /* if the start of the chunk is beyond the end of txt,
     * then we've already sent all the data, so set the
     * length to be zero
     */
    if (*pos > len)
      chunk_len = 0;

    if (chunk_len) {
      /* put the chunk into the property */
      xcb_change_property_checked(xconn,
			  XCB_PROP_MODE_REPLACE,
			  *win,
			  *pty,
			  STRING,
			  8,
			  chunk_len, &txt[*pos]);
    } else {
      /* make an empty property to show we've
       * finished the transfer
       */
      xcb_change_property_checked(xconn,
			  XCB_PROP_MODE_REPLACE,
			  *win,
			  *pty,
			  STRING,
			  8,
			  0, NULL);
    }
    xcb_flush(xconn);

    /* all data has been sent, break out of the loop */
    if (!chunk_len)
      *context = XCLIP_IN_NONE;

    *pos += XC_CHUNK;

    /* if chunk_len == 0, we just finished the transfer,
     * return 1
     */
    return !(chunk_len > 0);
  }
  }

  return 0;
}

void do_in_string(char *buf, size_t len)
{
  xcb_void_cookie_t cookie = xcb_change_property_checked(xconn,
							 XCB_PROP_MODE_REPLACE,
							 xwin,
							 CUT_BUFFER0,
							 STRING,
							 8,
							 len, buf);
  xcb_perror(cookie, "unable to set selection into string");
}

void do_in(char *buf, size_t len)
{
  int dloop = 0;	/* done loops counter */

  /* in mode */
  /* buffer for selection data */

  /* take control of the selection so that we receive
   * SelectionRequest events from other windows
   */
  xcb_void_cookie_t cookie = xcb_set_selection_owner_checked(xconn, xwin, sseln, XCB_CURRENT_TIME);

  xcb_perror(cookie, "cannot set selection owner");

  /* fork into the background, exit parent process if we
   * are in silent mode
   */
  if (fverb == OSILENT) {
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

    find_internal_atoms();

    /* wait for a SelectionRequest event */
    static unsigned int context = XCLIP_IN_NONE;
    static unsigned long sel_pos = 0;
    static xcb_window_t cwin;
    static xcb_atom_t pty;

    xcb_generic_event_t *event;
    bool clear = false;
    while ((event = xcb_wait_for_event(xconn))) {
      bool finished = doIn_internal_loop(
			   &cwin,
			   event,
			   &pty,
			   buf,
			   len,
			   &sel_pos,
			   &context
			   );

      if ( (event->response_type & ~0x80) == XCB_SELECTION_CLEAR )
	clear = true;

      if ( (context == XCLIP_IN_NONE) && clear)
	return;

      if (finished)
	break;
    }

    dloop++;	/* increment loop counter */
  }
}

static void send_selection_request() {
  /* send a selection request */
  xcb_void_cookie_t cookie = xcb_convert_selection_checked(xconn, xwin, sseln,
							   STRING, xclip_out_atom,
							   XCB_CURRENT_TIME);
  
  xcb_perror(cookie, "cannot convert selection");
}

static int handle_convert_selection(xcb_generic_event_t *event, char **txt, size_t *len) {
  if ((event->response_type & ~0x80) != XCB_SELECTION_NOTIFY)
    return 0;
  
  xcb_get_property_cookie_t cookie = xcb_get_property(xconn, false, xwin,
						      xclip_out_atom, STRING, 0, 128);
  xcb_get_property_reply_t *reply = xcb_get_property_reply(xconn, cookie, 0);
  
  assert(reply != NULL);
  
  if ( reply->type == incr_atom ) {
    xcb_delete_property(xconn, xwin, xclip_out_atom);
    xcb_flush(xconn);
    return -1;
  }
  
  assert(reply->format == 8);
  
  uint32_t reply_len = xcb_get_property_value_length(reply) * (reply->format / 8);
  if(reply->bytes_after) {
    cookie = xcb_get_property(xconn, 0, xwin, xclip_out_atom, reply->type, 0, reply_len);
    free(reply);
    reply = xcb_get_property_reply(xconn, cookie, 0);
    assert(reply != NULL);
  }
  *txt = malloc(reply_len);
  assert(*txt != NULL);
  
  memcpy(*txt, xcb_get_property_value(reply), reply_len);
  *len = reply_len;
  
  /* finished with property, delete it */
  free(reply);
  xcb_delete_property_checked(xconn, xwin, xclip_out_atom);
    
  /* complete contents of selection fetched, return 1 */
  return 1;
}

static bool handle_incr_request(xcb_generic_event_t *event, char **txt, size_t *len) {
    /* To use the INCR method, we basically delete the
     * property with the selection in it, wait for an
     * event indicating that the property has been created,
     * then read it, delete it, etc.
     */
    
    /* make sure that the event is relevant */
    if ((event->response_type & ~0x80) != XCB_PROPERTY_NOTIFY)
      return false;
    
    xcb_property_notify_event_t *const prop_event = (xcb_property_notify_event_t *)event;
    /* skip unless the property has a new value */
    if (prop_event->state != XCB_PROPERTY_NEW_VALUE)
      return false;
	
    xcb_get_property_cookie_t cookie = xcb_get_any_property(xconn, false,
							    xwin, xclip_out_atom,
							    0);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(xconn, cookie, 0);
    
    if ( reply->format != 8 ) {
      /* property does not contain text, delete it
       * to tell the other X client that we have read
       * it and to send the next property
       */
      xcb_delete_property_checked(xconn, xwin, xclip_out_atom);
      return false;
    }

    if (reply->bytes_after == 0) {
      /* no more data, exit from loop */
      xcb_delete_property_checked(xconn, xwin, xclip_out_atom);
      
      /* this means that an INCR transfer is now
       * complete, return true
       */
      return true;
    }

    /* if we have come this far, the propery contains
     * text, we know the size.
     */
    cookie = xcb_get_any_property(xconn, false,
				  xwin, xclip_out_atom,
				  reply->bytes_after);

    /* allocate memory to accommodate data in *txt */
    uint32_t reply_size = xcb_get_property_value_length(reply);
    *len += reply_size;
    char *ltxt = realloc(*txt, *len);
    
    if ( ltxt == NULL ) {
      perrorf("%s: %s", progname, __FUNCTION__);
      exit(EXIT_FAILURE);
    }
    
    /* add data to ltxt */
    memcpy(
	   &ltxt[*len - reply_size],
	   xcb_get_property_value(reply),
	   reply_size
	   );
    
    *txt = ltxt;
    
    /* delete property to get the next item */
    xcb_delete_property_checked(xconn, xwin, xclip_out_atom);
    xcb_flush(xconn);
    return false;
}

void do_out_string()
{
  char *buf; uint8_t format; uint32_t prop_len;
  int res = xcb_get_text_property(xconn, xwin, CUT_BUFFER0,
				  &format, NULL, &prop_len, &buf);
  
  if ( res != 0 && format == 8 )
    fwrite(buf, sizeof(char), prop_len, stdout);

#ifdef VALGRIND_CLEAN
  free(buf);
#endif
}

void do_out()
{
  char *buf = NULL;	/* buffer for selection data */
  size_t len = 0;		/* length of sel_buf */

  find_internal_atoms();
  
  send_selection_request();
  
  xcb_generic_event_t *event;
  unsigned int context = XCLIP_OUT_SENTCONVSEL;
  while ((event = xcb_wait_for_event(xconn))) {
    switch(context) {
    case XCLIP_OUT_SENTCONVSEL:
      switch(handle_convert_selection(event, &buf, &len)) {
      case -1:
	context = XCLIP_OUT_INCR;
      case 0:
	continue;
      case 1:
	  goto done;
      }
      break;
    case XCLIP_OUT_INCR:
	if ( handle_incr_request(event, &buf, &len) )
	  goto done;
	break;
    }
  }
  
  /* if we reach here, event was NULL, and something bad happened */
  assert(event != NULL);

 done:
  fwrite(buf, sizeof(char), len, stdout);

#ifdef VALGRIND_CLEAN
  free(buf);
#endif
}
