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
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_property.h>
#include "xcdef.h"
#include "xcprint.h"
#include "xclib.h"

/* check a pointer to allocater memory, print an error if it's null */
static void xcmemcheck(void *ptr)
{
	if (ptr == NULL)
		errmalloc();
}

/* wrapper for malloc that checks for errors */
void *xcmalloc (size_t size)
{
	void *mem;
	
	mem = malloc(size);
	xcmemcheck(mem);
	
	return(mem);
}

/* wrapper for realloc that checks for errors */
void *xcrealloc (void *ptr, size_t size)
{
	void *mem;

	mem = realloc(ptr, size);
	xcmemcheck(mem);

	return(mem);
}

static xcb_atom_t incr_atom;
static xcb_atom_t targets_atom;
static xcb_atom_t xclip_out_atom;

static void find_internal_atoms(xcb_connection_t* xconn) {
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

/* Retrieves the contents of a selections. Arguments are:
 *
 * A display that has been opened.
 * 
 * A window
 * 
 * An event to process
 * 
 * The selection to return
 * 
 * A pointer to a char array to put the selection into.
 * 
 * A pointer to a long to record the length of the char array
 *
 * A pointer to an int to record the context in which to process the event
 *
 * Return value is 1 if the retrieval of the selection data is complete,
 * otherwise it's 0.
 */
int xcout(
	xcb_connection_t* xconn,
	xcb_window_t win,
	xcb_generic_event_t* evt,
	xcb_atom_t sel,
	char** txt,
	size_t* len,
	uint32_t* context
)
{
  find_internal_atoms(xconn);

  /* local buffer of text to return */
  char *ltxt = *txt;

  xcb_void_cookie_t cookie;
  switch (*context) {
    /* there is no context, do an XConvertSelection() */
  case XCLIB_XCOUT_NONE:
    /* initialise return length to 0 */
    if (*len > 0) {
      free(*txt);
      *len = 0;
    }

    /* send a selection request */
    cookie = xcb_convert_selection_checked(xconn, win, sel, STRING,
					   xclip_out_atom, XCB_CURRENT_TIME);

    xcb_perror(xconn, cookie, "cannot convert selection");

    *context = XCLIB_XCOUT_SENTCONVSEL;
    return 0;
		
  case XCLIB_XCOUT_SENTCONVSEL: {
    if ((evt->response_type & ~0x80) != XCB_SELECTION_NOTIFY)
      return 0;

    xcb_get_property_cookie_t cookie = xcb_get_property(xconn, false, win,
							xclip_out_atom, STRING, 0, 128);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(xconn, cookie, 0);

    assert(reply != NULL);

    if ( reply->type == incr_atom ) {
      xcb_delete_property(xconn, win, xclip_out_atom);
      xcb_flush(xconn);
      *context = XCLIB_XCOUT_INCR;
      return 0;
    }
    
    assert(reply->format == 8);
    
    uint32_t reply_len = xcb_get_property_value_length(reply) * (reply->format / 8);
    if(reply->bytes_after) {
      cookie = xcb_get_property(xconn, 0, win, xclip_out_atom, reply->type, 0, reply_len);
      free(reply);
      reply = xcb_get_property_reply(xconn, cookie, 0);
      assert(reply != NULL);
    }
    *txt = calloc(reply_len, 1);
    assert(*txt != NULL);

    memcpy(*txt, xcb_get_property_value(reply), reply_len);
    *len = reply_len;
    
    /* finished with property, delete it */
    free(reply);
    xcb_delete_property_checked(xconn, win, xclip_out_atom);
    
    *context = XCLIB_XCOUT_NONE;

    /* complete contents of selection fetched, return 1 */
    return 1;
  }

  case XCLIB_XCOUT_INCR: {
    /* To use the INCR method, we basically delete the
     * property with the selection in it, wait for an
     * event indicating that the property has been created,
     * then read it, delete it, etc.
     */
    
    /* make sure that the event is relevant */
    if ((evt->response_type & ~0x80) != XCB_PROPERTY_NOTIFY)
      return 0;
    
    xcb_property_notify_event_t *const prop_event = (xcb_property_notify_event_t *)evt;
    /* skip unless the property has a new value */
    if (prop_event->state != XCB_PROPERTY_NEW_VALUE)
      return 0;
	
    xcb_get_property_cookie_t cookie = xcb_get_any_property(xconn, false,
							    win, xclip_out_atom,
							    0);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(xconn, cookie, 0);
    
    if ( reply->format != 8 ) {
      /* property does not contain text, delete it
       * to tell the other X client that we have read
       * it and to send the next property
       */
      xcb_delete_property_checked(xconn, win, xclip_out_atom);
      return 0;
    }

    if (reply->bytes_after == 0) {
      /* no more data, exit from loop */
      xcb_delete_property_checked(xconn, win, xclip_out_atom);
      *context = XCLIB_XCOUT_NONE;
      
      /* this means that an INCR transfer is now
       * complete, return 1
       */
      return 1;
    }

    /* if we have come this far, the propery contains
     * text, we know the size.
     */
    cookie = xcb_get_any_property(xconn, false,
				  win, xclip_out_atom,
				  reply->bytes_after);

    /* allocate memory to accommodate data in *txt */
    uint32_t reply_size = xcb_get_property_value_length(reply);
    *len += reply_size;
    ltxt = xcrealloc(ltxt, *len);
    
    /* add data to ltxt */
    memcpy(
	   &ltxt[*len - reply_size],
	   xcb_get_property_value(reply),
	   reply_size
	   );
    
    *txt = ltxt;
    
    /* delete property to get the next item */
    xcb_delete_property_checked(xconn, win, xclip_out_atom);
    xcb_flush(xconn);
    return 0;
  }
  }
  return 0;
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
int xcin(xcb_connection_t* xconn,
	 xcb_window_t* win,
	 xcb_generic_event_t* evt,
	 xcb_atom_t* pty,
	 uint8_t* txt,
	 size_t len,
	 size_t* pos,
	 uint32_t* context
)
{
  find_internal_atoms(xconn);

  unsigned long chunk_len;	/* length of current chunk (for incr
				 * transfers only)
				 */

  xcb_void_cookie_t cookie;
  switch (*context) {
  case XCLIB_XCIN_NONE: {
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

	*context = XCLIB_XCIN_INCR;
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

    xcb_perror(xconn, cookie, "cannot set data into property");

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

    xcb_perror(xconn, cookie, "cannot set selection notify");

    /* if len < XC_CHUNK, then the data was sent all at
     * once and the transfer is now complete, return 1
     */
    return !(len > XC_CHUNK);

  case XCLIB_XCIN_INCR:
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
      *context = XCLIB_XCIN_NONE;

    *pos += XC_CHUNK;

    /* if chunk_len == 0, we just finished the transfer,
     * return 1
     */
    return !(chunk_len > 0);
  }
  }

  return 0;
}
