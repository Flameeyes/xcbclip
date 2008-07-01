#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_property.h>

/*
 * @note Taken from xcb-util/icccm/icccm.c; modified to accept NULL pointers
 */
int xcb_get_text_property(xcb_connection_t *c,
                      xcb_window_t      window,
                      xcb_atom_t        property,
                      uint8_t          *format,
                      xcb_atom_t       *encoding,
                      uint32_t         *name_len,
                      char            **name)
{
        xcb_get_property_cookie_t cookie;
        xcb_get_property_reply_t *reply;

        cookie = xcb_get_any_property(c, 0, window, property, 128);
        reply = xcb_get_property_reply(c, cookie, 0);
        if(!reply)
                return 0;
	if(format)
	  *format = reply->format;
	if(encoding)
	  *encoding = reply->type;
	if(name_len)
	  *name_len = xcb_get_property_value_length(reply) * *format / 8;
        if(reply->bytes_after)
        {
                cookie = xcb_get_property(c, 0, window, property, reply->type, 0, *name_len);
                free(reply);
                reply = xcb_get_property_reply(c, cookie, 0);
                if(!reply)
                        return 0;
        }
        memmove(reply, xcb_get_property_value(reply), *name_len);
        *name = (char *) reply;
        return 1;
}
