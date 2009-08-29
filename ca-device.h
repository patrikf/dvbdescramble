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

#ifndef CA_DEVICE_H
#define CA_DEVICE_H

#include <glib-object.h>

#define CA_TYPE_DEVICE (ca_device_get_type())

GType ca_device_get_type();

typedef GObjectClass CADeviceClass;

typedef struct
{
    GObject _parent;
    gint fd;
    guint fd_watch;
} CADevice;

typedef gboolean
(*CADeviceReceivedTPDUFunc)(CADevice *ca,
                            guint8 slot,
                            guint8 channel,
                            guint8 tag,
                            guint8 *data,
                            guint len,
                            gpointer user_data);

CADevice *
ca_open_device(gchar *device);

void
ca_device_reset(CADevice *ca);

#endif

