
#ifndef MISC_H
#define MISC_H

#include <glib.h>

void
decode_length_field(const guint8 *buf,
		    guint *value,
		    guint8 **after);

guint8
length_field_size(guint value);

guint8 *
encode_length_field(guint8 *buf,
                    guint value);

#endif

