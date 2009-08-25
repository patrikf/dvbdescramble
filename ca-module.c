
#include "ca-module.h"
#include "misc.h"
#include <string.h>
#include <unistd.h>
#include "marshal.h"

#include <sys/ioctl.h>
#include <linux/dvb/ca.h>

enum
{
    PROP_DEVICE = 1,
    PROP_SLOT,
    N_PROPS
};

enum
{
    SIGNAL_RECEIVED_TPDU = 0,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static gboolean
received_tpdu_default(CAModule *cam,
                      guint8 channel,
                      guint8 tag,
                      guint8 *data,
                      guint len)
{
    /* D_T_C_Reply */
    if (tag == 0x85)
    {
        g_assert(data[0] == channel);

        /* If this channel existed, it should have handled the message. If it
         * didn't, we silently ignore this TPDU. */

        return TRUE;
    }

    /* for all other operations, a channel needs to exist
     * Note: This is actually not true for Create_T_C, but we expect the one
     * who requested a Transport Channel to be listening and catch the TPDU
     * before we get it.
     */

    g_debug("oops! message (tag 0x%02X) for channel 0x%02X fell through", tag, channel);

    /* no such channel! tell the module */
    /* Delete_T_C */
    ca_module_send_tpdu(cam, channel, 0x84, (guint8[]){channel}, 1);
    return TRUE;
}

static gboolean
received_tpdu(CAModule *cam,
              guint8 slot,
              guint8 channel,
              guint8 tag,
              guint8 *data,
              guint len)
{
    /* that one is not for us */
    if (slot != cam->slot)
        return FALSE;

    gboolean r;
    g_signal_emit(
            cam,
            signals[SIGNAL_RECEIVED_TPDU],
            0,
            channel, tag, data, len,
            &r
        );
    return r;
}

static void
set_property(CAModule *cam,
             guint prop_id,
             const GValue *value,
             GParamSpec *pspec)
{
    if (prop_id == PROP_DEVICE)
    {
        cam->device = g_object_ref(g_value_get_object(value));
        g_signal_connect_swapped(cam->device, "received-tpdu", G_CALLBACK(received_tpdu), cam);
    }
    else if (prop_id == PROP_SLOT)
    {
        cam->slot = g_value_get_uchar(value);
    }
}

static void
class_init(CAModuleClass *klass)
{
    GObjectClass *gobj_class = (gpointer)klass;

    klass->received_tpdu = received_tpdu_default;

    gobj_class->set_property = (GObjectSetPropertyFunc)set_property;

    g_object_class_install_property(
            gobj_class,
            PROP_DEVICE,
            g_param_spec_object(
                "device",
                "device",
                "The CADevice to use",
                CA_TYPE_DEVICE,
                G_PARAM_STATIC_NAME | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY
            )
        );
    g_object_class_install_property(
            gobj_class,
            PROP_SLOT,
            g_param_spec_uchar(
                "slot",
                "slot",
                "Which slot to use",
                0x00, 0xFF, 0x00,
                G_PARAM_STATIC_NAME | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY
            )
        );

    signals[SIGNAL_RECEIVED_TPDU] = g_signal_new(
            "received-tpdu",
            CA_TYPE_MODULE,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(CAModuleClass, received_tpdu),
            g_signal_accumulator_true_handled,
            NULL,
            g_cclosure_user_marshal_BOOLEAN__UCHAR_UCHAR_POINTER_UINT,
            G_TYPE_BOOLEAN,
            4,
            G_TYPE_UCHAR, G_TYPE_UCHAR, G_TYPE_POINTER, G_TYPE_UINT
        );
}

GType
ca_module_get_type()
{
    static GType type = 0;
    if (type == 0)
        type = g_type_register_static_simple(
                G_TYPE_OBJECT, /* parent_type */
                "CAModuleType", /* type_name */
                sizeof(CAModuleClass), /* class_size */
                (GClassInitFunc)class_init, /* class_init */
                sizeof(CAModule), /* instance_size */
                (GInstanceInitFunc)NULL, /* instance_init */
                0 /* flags */
            );
    return type;
}

CAModule *
ca_device_open_module(CADevice *device,
                      guint8 slot)
{
    return g_object_new(CA_TYPE_MODULE,
            "device", device,
            "slot", slot,
            NULL);
}

void
ca_module_send_raw_tpdu(CAModule *cam,
                        const guint8 *tpdu)
{
    guint len;
    guint8 *data;
    decode_length_field(tpdu+1, &len, &data);

    guint8 *buf = g_malloc(data+len-tpdu + 2);
    buf[0] = cam->slot;
    buf[1] = data[0];
    memcpy(buf+2, tpdu, data+len-tpdu);

    write(cam->device->fd, buf, data+len-tpdu+2);
    g_free(buf);
}

void
ca_module_send_tpdu(CAModule *cam,
                    guint8 c_tpdu_tag,
                    guint8 t_c_id,
                    const guint8 *data,
                    guint len)
{
    guint8 *tpdu = g_malloc(length_field_size(len+1)+2+len);
    tpdu[0] = c_tpdu_tag;
    guint8 *buf = encode_length_field(tpdu+1, len+1);
    buf[0] = t_c_id;
    memcpy(buf+1, data, len);
    ca_module_send_raw_tpdu(cam, tpdu);
    g_free(tpdu);
}

void
ca_module_create_t_c(CAModule *cam,
                     guint8 id)
{
    ca_module_send_tpdu(cam, 0x82, id, (guint8[]){id}, 1);
}

struct ctc_status
{
    guint8 id;
    CATC *tc;
    GMainLoop *loop;
    gulong handler_id;
};

static gboolean
ctc_reply_cb(CAModule *cam,
             guint8 channel,
             guint8 tag,
             guint8 *data,
             guint len,
             struct ctc_status *s)
{
    if (channel != s->id)
        return FALSE;

    /* C_T_C_Reply */
    if (tag == 0x83)
    {
        s->tc = ca_t_c_new(cam, s->id);

        g_signal_handler_disconnect(cam, s->handler_id);
        g_main_loop_quit(s->loop);
        return TRUE;
    }
    /* Status Byte */
    else if (tag == 0x80)
    {
        /* we do not handle this one, but keep waiting */
        g_debug("skipping spontaneus status byte");
        return TRUE;
    }

    /* fail */
    g_signal_handler_disconnect(cam, s->handler_id);
    g_main_loop_quit(s->loop);
    return FALSE;
}

CATC *
ca_module_create_t_c_block(CAModule *cam,
                           guint8 id)
{
    struct ctc_status s;
    s.id = id;
    s.tc = NULL;
    s.loop = g_main_loop_new(NULL, FALSE);

    s.handler_id = g_signal_connect(cam, "received-tpdu", G_CALLBACK(ctc_reply_cb), &s);
    ca_module_create_t_c(cam, id);

    g_main_loop_run(s.loop);
    g_main_loop_unref(s.loop);

    return s.tc;
}

gboolean
ca_module_present(CAModule *cam)
{
    ca_slot_info_t slot;
    slot.num = cam->slot;
    ioctl(cam->device->fd, CA_GET_SLOT_INFO, &slot);

    return !!(slot.flags & CA_CI_MODULE_PRESENT);
}

gboolean
ca_module_ready(CAModule *cam)
{
    ca_slot_info_t slot;
    slot.num = cam->slot;
    ioctl(cam->device->fd, CA_GET_SLOT_INFO, &slot);

    return !!(slot.flags & CA_CI_MODULE_READY);
}

struct reset_info
{
    CAModule *cam;
    GMainLoop *loop;
};

static gboolean
check_ready(struct reset_info *i)
{
    ca_slot_info_t slot;
    slot.num = i->cam->slot;
    ioctl(i->cam->device->fd, CA_GET_SLOT_INFO, &slot);

    g_assert(slot.flags & CA_CI_MODULE_PRESENT);

    if (slot.flags & CA_CI_MODULE_READY)
    {
        g_main_loop_quit(i->loop);
        return FALSE;
    }
    else
        return TRUE;
}

void
ca_module_wait_ready(CAModule *cam)
{
    struct reset_info i;
    i.cam = cam;
    i.loop = g_main_loop_new(NULL, FALSE);

    g_timeout_add_full(
            G_PRIORITY_HIGH,
            100,
            (GSourceFunc)check_ready,
            &i,
            NULL
        );

    g_main_loop_run(i.loop);
    g_main_loop_unref(i.loop);
}

