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

#ifndef CA_MODULE_H
#define CA_MODULE_H

#include <glib-object.h>

#define CA_TYPE_MODULE (ca_module_get_type())
GType ca_module_get_type();
typedef struct _CAModuleClass CAModuleClass;
typedef struct _CAModule CAModule;

#include "ca-device.h"
#include "ca-t-c.h"

struct _CAModuleClass
{
    GObjectClass _parent;
    gboolean (*received_tpdu)(CAModule *cam,
                              guint8 channel,
                              guint8 tag,
                              guint8 *data,
                              guint len);
};

struct _CAModule
{
    GObject _parent;
    CADevice *device;
    guint slot;
};

CAModule *
ca_device_open_module(CADevice *device,
                      guint8 slot);

void
ca_module_send_raw_tpdu(CAModule *cam,
                        const guint8 *tpdu);

void
ca_module_send_tpdu(CAModule *cam,
                    guint8 c_tpdu_tag,
                    guint8 t_c_id,
                    const guint8 *data,
                    guint len);

void
ca_module_create_t_c(CAModule *cam,
                     guint8 id);

CATC *
ca_module_create_t_c_block(CAModule *cam,
                           guint8 id);

gboolean
ca_module_present(CAModule *cam);

gboolean
ca_module_ready(CAModule *cam);

void
ca_module_wait_ready(CAModule *cam);

#endif

