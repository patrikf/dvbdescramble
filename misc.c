/*
 * (C) 2008-2009 Patrik Fimml.
 *
 * This file is part of dvbdescramble.
 *
 * dvbdescramble is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "misc.h"

void
decode_length_field(const guint8 *buf,
		    guint *value,
		    guint8 **after)
{
    int size = 0;
    if (buf[0] & 0x80)
    {
	size = 1 + (buf[0] & 0x7F);
        if (value)
        {
            *value = 0;
            for (int i = 1; i <= (buf[0] & 0x7F); i++)
            {
                *value <<= 8;
                *value |= buf[i];
            }
        }
    }
    else
    {
	size = 1;
        if (value)
            *value = buf[0];
    }
    if (after)
	*after = (guint8 *)buf+size;
}

guint8
length_field_size(guint value)
{
    if (value < 0x80)
	return 1;
    int size = 1;
    while (value)
    {
	value >>= 8;
	size++;
    }
    return size;
}

/*
 * Returns: pointer to first byte after the length field
 */
guint8 *
encode_length_field(guint8 *buf,
                    guint value)
{
    guint8 *pos = buf;
    if (value < 0x80)
	*(pos++) = value;
    else
    {
	pos++;
	while (value)
	{
	    *(pos++) = value & 0xFF;
	    value >>= 8;
	}
	buf[0] = 0x80 | (pos-buf-1);
    }
    return pos;
}
