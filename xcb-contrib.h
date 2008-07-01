#ifndef XCB_CONTRIB_H
#define XCB_CONTRIB_H

#include <xcb/xcb.h>

int xcb_get_text_property(xcb_connection_t *c,
			  xcb_window_t      window,
			  xcb_atom_t        property,
			  uint8_t          *format,
			  xcb_atom_t       *encoding,
			  uint32_t         *name_len,
			  char            **name);

#endif
