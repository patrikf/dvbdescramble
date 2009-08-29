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

#include "ca-device.h"
#include "misc.h"
#include "marshal.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <linux/dvb/ca.h>

enum
{
    PROP_DEVICE = 1,
    N_PROPS
};

enum
{
    SIGNAL_RECEIVED_TPDU = 0,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static const gchar *
decode_tpdu_tag(int x)
{
    const char *t[] = {
            "T_SB",
            "T_RCV",
            "T_create_t_c",
            "T_c_t_c_reply",
            "T_delete_t_c",
            "T_d_t_c_reply",
            "T_request_t_c",
            "T_new_t_c",
            "T_t_c_error",
        };
    if (x < 0x80 || x > 0x88)
        return "(unknown)";
    return t[x-0x80];
}

static gboolean
dispatch(GIOChannel *ioc,
         GIOCondition cond,
         gpointer _data)
{
    CADevice *ca = _data;

    guint8 buf[512];
    int r = read(ca->fd, buf, 512);

    /* FIXME: implement dynamic buffering */
    g_assert(r < 512);

    if (r <= 0)
        g_error("read: %s", g_strerror(errno));

#if DEBUG
    g_debug("read %d bytes", r);
#endif

    guint8 *pos = buf+2;
    guint8 *end = buf+r;
    while (pos < end)
    {
#if DEBUG
        g_debug("%d bytes remain", end-pos);
#endif
        /* Note that the actual TPDU and the status part of the R_TPDU are
         * treated like two separate TPDUs.
         */

        guint len;
        guint8 *data;
        decode_length_field(pos+1, &len, &data);

        gboolean r;
        g_signal_emit(
                ca,
                signals[SIGNAL_RECEIVED_TPDU],
                0,
                buf[0], /* slot */
                buf[1], /* channel */
                pos[0], /* tag */
                data,
                len,
                &r
            );

#if DEBUG
        g_debug("TPDU received:");
        g_debug("   slot:    0x%02X", buf[0]);
        g_debug("   channel: 0x%02X", buf[1]);
        g_debug("   tag:     0x%02X (%s)", pos[0], decode_tpdu_tag(pos[0]));

        for (guint8 *cur = pos+1; cur < data; cur++)
            g_debug("   length_field: 0x%02X", *cur);
        for (guint i = 0; i < len; i++)
            g_debug("   data[%2d]  0x%02X", i, data[i]);
#endif

        pos = data + len;
    }
    g_assert(pos == end);

    return TRUE;
}

static void
set_property(CADevice *ca,
             guint prop_id,
             const GValue *value,
             GParamSpec *pspec)
{
    if (prop_id == PROP_DEVICE)
    {
        const gchar *device = g_value_get_string(value);
        ca->fd = open(device, O_RDWR);
        if (ca->fd == -1)
        {
            g_error("unable to open %s: %s", device, g_strerror(errno));
        }

        GIOChannel *ioc = g_io_channel_unix_new(ca->fd);
        ca->fd_watch = g_io_add_watch(ioc, G_IO_IN, dispatch, ca);
        g_io_channel_unref(ioc);
    }
}

static void
class_init(CADeviceClass *klass)
{
    GObjectClass *gobj_class = (GObjectClass *)klass;

    gobj_class->set_property = (GObjectSetPropertyFunc)set_property;

    g_object_class_install_property(
            gobj_class,
            PROP_DEVICE,
            g_param_spec_string(
                "device",
                "device",
                "The device file to open (usually /dev/dvb/adapterX/caY)",
                "/dev/dvb/adapter0/ca0",
                G_PARAM_STATIC_NAME | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY
            )
        );

    signals[SIGNAL_RECEIVED_TPDU] = g_signal_new(
            "received-tpdu",
            CA_TYPE_DEVICE,
            G_SIGNAL_RUN_LAST,
            0,
            g_signal_accumulator_true_handled,
            NULL,
            g_cclosure_user_marshal_BOOLEAN__UCHAR_UCHAR_UCHAR_POINTER_UINT,
            G_TYPE_BOOLEAN,
            5,
            G_TYPE_UCHAR, G_TYPE_UCHAR, G_TYPE_UCHAR, G_TYPE_POINTER, G_TYPE_UINT
        );
}

GType
ca_device_get_type()
{
    static GType type = 0;
    if (type == 0)
        type = g_type_register_static_simple(
                G_TYPE_OBJECT, /* parent_type */
                "CADeviceType", /* type_name */
                sizeof(CADeviceClass), /* class_size */
                (GClassInitFunc)class_init, /* class_init */
                sizeof(CADevice), /* instance_size */
                NULL, /* instance_init */
                0 /* flags */
            );
    return type;
}

CADevice *
ca_open_device(gchar *device)
{
    return g_object_new(CA_TYPE_DEVICE, "device", device, NULL);
}

void
ca_device_reset(CADevice *ca)
{
    ioctl(ca->fd, CA_RESET);

    /* This is not necessary. */
#if 0
    /* clear any messages that arrived so far */
    int flags = fcntl(ca->fd, F_GETFL);
    fcntl(ca->fd, F_SETFL, flags | O_NONBLOCK);

    char buf[512];
    while (read(ca->fd, buf, 512) > 0);

    if (errno != EAGAIN)
        g_error("error clearing buffer of CA device: %s", g_strerror(errno));

    fcntl(ca->fd, F_SETFL, flags);
#endif
}

