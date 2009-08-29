
#include "ca-t-c.h"
#include <stdio.h>
#include "marshal.h"
#include "misc.h"
#include <string.h>
#include <stdio.h>

enum
{
    PROP_MODULE = 1,
    PROP_ID,
    N_PROPS
};

enum
{
    SIGNAL_RECEIVED_TPDU = 0,
    SIGNAL_RECEIVED_SPDU,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static gboolean
poll_timeout(CATC *catc)
{
    ca_t_c_send_data(catc, NULL, 0);
    return FALSE; /* one-shot function */
}

static gboolean
received_spdu_default(CATC *catc,
                      guint8 tag,
                      guint8 *data,
                      guint len)
{
    g_debug("SPDU fell through:");
    g_debug("    tag = 0x%02X", tag);
    g_debug("    len = 0x%02X", len);
    for (int i = 0; i < len; i++)
        g_debug("    data[%2d] = 0x%02X", i, data[i]);
    return TRUE;
}

static gboolean
received_tpdu_default(CATC *catc,
                      guint8 tag,
                      guint8 *data,
                      guint len)
{
    /* Status Byte */
    if (tag == 0x80)
    {
        g_assert(len == 2);
        g_assert(data[0] == catc->id);

        /* kill pending poll function */
        if (catc->poll_id)
            g_source_remove(catc->poll_id);
        if (data[1] & 0x80)
        {
#if DEBUG
            g_debug("module has data!");
#endif

            /* Receive Data */
            ca_module_send_tpdu(catc->module, 0x81, catc->id, NULL, 0);
        }
        else
        {
            /* no more data, poll again in 75 ms. (spec: < 100 ms) */
            catc->poll_id = g_timeout_add_full(
                G_PRIORITY_HIGH,
                75,
                (GSourceFunc)poll_timeout,
                catc,
                NULL);
        }
        return TRUE;
    }
    if (tag == 0xA0)
    {
        g_assert(len > 1);
        g_assert(data[0] == catc->id);

#if DEBUG
        fprintf(stderr, "receiving data for t_c_id 0x%02X:", data[0]);
        for (int i = 0; i < len; i++)
            fprintf(stderr, " %02X", data[i]);
        fprintf(stderr, "\n");
#endif

        /* data contains an SPDU */

        guint8 spdu_tag = data[1];
        guint spdu_len;
        guint8 *spdu_data;
        decode_length_field(data+2, &spdu_len, &spdu_data);

        gboolean r;
        g_signal_emit(
                catc,
                signals[SIGNAL_RECEIVED_SPDU],
                0,
                spdu_tag, spdu_data, spdu_len,
                &r
            );

        return TRUE;
    }

    return FALSE;
}

static gboolean
received_tpdu(CATC *catc,
              guint8 channel,
              guint8 tag,
              guint8 *data,
              guint len)
{
    /* that one is not for us */
    if (channel != catc->id)
        return FALSE;

    gboolean r;
    g_signal_emit(
            catc,
            signals[SIGNAL_RECEIVED_TPDU],
            0,
            tag, data, len,
            &r
        );
    return r;
}

static void
set_property(CATC *catc,
             guint prop_id,
             const GValue *value,
             GParamSpec *pspec)
{
    if (prop_id == PROP_MODULE)
    {
        catc->module = g_object_ref(g_value_get_object(value));
        g_signal_connect_swapped(catc->module, "received-tpdu", G_CALLBACK(received_tpdu), catc);
    }
    else if (prop_id == PROP_ID)
    {
        catc->id = g_value_get_uchar(value);
    }
}

static void
class_init(CATCClass *klass)
{
    GObjectClass *gobj_class = (gpointer)klass;

    klass->received_tpdu = received_tpdu_default;
    klass->received_spdu = received_spdu_default;

    gobj_class->set_property = (GObjectSetPropertyFunc)set_property;

    g_object_class_install_property(
            gobj_class,
            PROP_MODULE,
            g_param_spec_object(
                "module",
                "module",
                "The CAModule to use",
                CA_TYPE_MODULE,
                G_PARAM_STATIC_NAME | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY
            )
        );
    g_object_class_install_property(
            gobj_class,
            PROP_ID,
            g_param_spec_uchar(
                "id",
                "id",
                "The ID of the transport channel",
                0x00, 0xFF, 0x00,
                G_PARAM_STATIC_NAME | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY
            )
        );

    signals[SIGNAL_RECEIVED_TPDU] = g_signal_new(
            "received-tpdu",
            CA_TYPE_T_C,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(CATCClass, received_tpdu),
            g_signal_accumulator_true_handled,
            NULL,
            g_cclosure_user_marshal_BOOLEAN__UCHAR_POINTER_UINT,
            G_TYPE_BOOLEAN,
            3,
            G_TYPE_UCHAR, G_TYPE_POINTER, G_TYPE_UINT
        );

    signals[SIGNAL_RECEIVED_SPDU] = g_signal_new(
            "received-spdu",
            CA_TYPE_T_C,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(CATCClass, received_spdu),
            g_signal_accumulator_true_handled,
            NULL,
            g_cclosure_user_marshal_BOOLEAN__UCHAR_POINTER_UINT,
            G_TYPE_BOOLEAN,
            3,
            G_TYPE_UCHAR, G_TYPE_POINTER, G_TYPE_UINT
        );
}

GType
ca_t_c_get_type()
{
    static GType type = 0;
    if (type == 0)
        type = g_type_register_static_simple(
                G_TYPE_OBJECT, /* parent_type */
                "CATCType", /* type_name */
                sizeof(CATCClass), /* class_size */
                (GClassInitFunc)class_init, /* class_init */
                sizeof(CATC), /* instance_size */
                (GInstanceInitFunc)NULL, /* instance_init */
                0 /* flags */
            );
    return type;
}

CATC *
ca_t_c_new(CAModule *module,
           guint8 id)
{
    return g_object_new(CA_TYPE_T_C,
        "module", module,
        "id", id,
        NULL);
}

void
ca_t_c_send_data(CATC *catc,
                 const guint8 *data,
                 guint len)
{
    ca_module_send_tpdu(catc->module, 0xA0, catc->id, data, len);
}


void
ca_t_c_send_spdu(CATC *catc,
                 guint8 tag,
                 const guint8 *data,
                 guint len)
{
    guint spdu_len = length_field_size(len)+len+1;
    guint8 *spdu = g_malloc(spdu_len);
    spdu[0] = tag;
    guint8 *dst = encode_length_field(spdu+1, len);
    memcpy(dst, data, len);
    ca_t_c_send_data(catc, spdu, spdu_len);
    g_free(spdu);
}

void
ca_t_c_send_apdu(CATC *catc,
                 guint16 session,
                 guint32 tag,
                 const guint8 *data,
                 guint len)
{
    guint spdu_len = 4 + 3 + length_field_size(len) + len;

    /* "Session Number" SPDU */
    guint8 *spdu = g_malloc(spdu_len);
    spdu[0] = 0x90;
    spdu[1] = 2;
    spdu[2] = session >> 8;
    spdu[3] = session & 0xFF;

    /* this is where the APDU starts */
    spdu[4] = tag >> 16;
    spdu[5] = (tag >> 8) & 0xFF;
    spdu[6] = tag & 0xFF;

    guint8 *dst = encode_length_field(spdu+7, len);
    memcpy(dst, data, len);

    ca_t_c_send_data(catc, spdu, spdu_len);

    fprintf(stderr, "sending SPDU:");
    for (int i = 0; i < spdu_len; i++)
        fprintf(stderr, " %02X", spdu[i]);
    fprintf(stderr, "\n");

    g_free(spdu);
}

