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

#include "ca-resource-manager.h"
#include "misc.h"
#include <string.h>

struct session_info
{
    const CAResourceInfo *res;
};

static guint8
resmgr_alwaysok(CAResourceManager *mgr,
                CATC *catc,
                guint16 session);
static void
resmgr_init(CAResourceManager *mgr,
            CATC *catc,
            guint16 session);
static gboolean
resmgr_apdu(CAResourceManager *mgr,
            CATC *catc,
            guint16 session,
            guint32 tag,
            guint8 *data,
            guint len);
static void
appinfo_init(CAResourceManager *mgr,
             CATC *catc,
             guint16 session);
static gboolean
appinfo_apdu(CAResourceManager *mgr,
             CATC *catc,
             guint16 session,
             guint32 tag,
             guint8 *data,
             guint len);
static void
cainfo_init(CAResourceManager *mgr,
            CATC *catc,
            guint16 session);
static gboolean
cainfo_apdu(CAResourceManager *mgr,
            CATC *catc,
            guint16 session,
            guint32 tag,
            guint8 *data,
            guint len);
static gboolean
datetime_apdu(CAResourceManager *mgr,
              CATC *catc,
              guint16 session,
              guint32 tag,
              guint8 *data,
              guint len);

static const int n_builtin_resources = 4;
static const CAResourceInfo builtin_resources[5] = {
    { 1, 1, 1, "Resource Manager",           resmgr_alwaysok, resmgr_init,  resmgr_apdu,  NULL},
    { 2, 1, 1, "Application Information",    resmgr_alwaysok, appinfo_init, appinfo_apdu, NULL},
    { 3, 1, 1, "Conditional Access Support", resmgr_alwaysok, cainfo_init,  cainfo_apdu,  NULL},
    {36, 1, 1, "Date-Time",                  resmgr_alwaysok, NULL, datetime_apdu, NULL},
    { 0, 0, 0, NULL, NULL, NULL, NULL, NULL}
};

static void
instance_init(CAResourceManager *mgr)
{
    mgr->resources = NULL;
    mgr->sessions = g_array_new(FALSE, FALSE, sizeof(gpointer));
    mgr->first = TRUE;
}

GType
ca_resource_manager_get_type()
{
    static GType type = 0;
    if (type == 0)
        type = g_type_register_static_simple(
                G_TYPE_OBJECT, /* parent_type */
                "CAResourceManagerType", /* type_name */
                sizeof(CAResourceManagerClass), /* class_size */
                (GClassInitFunc)NULL, /* class_init */
                sizeof(CAResourceManager), /* instance_size */
                (GInstanceInitFunc)instance_init, /* instance_init */
                0 /* flags */
            );
    return type;
}

CAResourceManager *
ca_resource_manager_new()
{
    return g_object_new(CA_TYPE_RESOURCE_MANAGER, NULL);
}

static guint32
res_get_identifier(const CAResourceInfo *res)
{
    return (res->res_class << 16) | (res->res_type << 6) | res->res_version;
}

static guint8
resmgr_alwaysok(CAResourceManager *mgr,
                CATC *catc,
                guint16 session)
{ return 0x00; }

static void
resmgr_init(CAResourceManager *mgr,
            CATC *catc,
            guint16 session)
{
    /* Profile Enquiry */
    ca_t_c_send_apdu(catc, session, 0x9F8010, NULL, 0);
}

static gboolean
resmgr_apdu(CAResourceManager *mgr,
            CATC *catc,
            guint16 session,
            guint32 tag,
            guint8 *data,
            guint len)
{
    /* Profile Reply */
    if (tag == 0x9F8011)
    {
        if (len != 0)
            g_warning("CA Module provides resources, not implemented");
        else
            g_debug("CA Module provides no resources");

        /* Profile Changed */
        ca_t_c_send_apdu(catc, session, 0x9F8012, NULL, 0);
        return TRUE;
    }

    /* Profile Enquiry */
    if (tag == 0x9F8010)
    {
        g_assert(len == 0);

        guint8 *buf = g_new(guint8, 4*n_builtin_resources);
        guint8 *cur = buf;
        for (int i = 0; i < n_builtin_resources; i++, cur += 4)
        {
            guint32 identifier = res_get_identifier(&builtin_resources[i]);
            cur[0] = identifier >> 24;
            cur[1] = (identifier >> 16) & 0xFF;
            cur[2] = (identifier >> 8) & 0xFF;
            cur[3] = identifier & 0xFF;
        }

        /* Profile Reply */
        ca_t_c_send_apdu(catc, session, 0x9F8011, buf, 4*n_builtin_resources);

        g_free(buf);

        return TRUE;
    }

    return FALSE;
}

static void
appinfo_init(CAResourceManager *mgr,
             CATC *catc,
             guint16 session)
{
    /* Application Info Enquiry */
    ca_t_c_send_apdu(catc, session, 0x9F8020, NULL, 0);
}

static gboolean
appinfo_apdu(CAResourceManager *mgr,
             CATC *catc,
             guint16 session,
             guint32 tag,
             guint8 *data,
             guint len)
{
    /* Application Info */
    if (tag == 0x9F8021)
    {
        guint8 app_type = data[0];
        guint16 app_manufacturer = (data[1] << 8) | data[2];
        guint16 manufacturer_code = (data[3] << 8) | data[4];
        gchar *menu_string = g_strndup((gchar *)data+6, data[5]);

        g_debug("Application Information:");
        g_debug("    type         = 0x%02X", app_type);
        g_debug("    manufacturer = 0x%04X", app_manufacturer);
        g_debug("    manuf_code   = 0x%04X", manufacturer_code);
        g_debug("    menu_string  = %s", menu_string);

        g_free(menu_string);

        return TRUE;
    }

    return FALSE;
}

static void
cainfo_init(CAResourceManager *mgr,
            CATC *catc,
            guint16 session)
{
    /* CA Info Enquiry */
    ca_t_c_send_apdu(catc, session, 0x9F8030, NULL, 0);
}

static gboolean
cainfo_apdu(CAResourceManager *mgr,
            CATC *catc,
            guint16 session,
            guint32 tag,
            guint8 *data,
            guint len)
{
    /* CA Info */
    if (tag == 0x9F8031)
    {
        g_debug("CA Information:");

        guint8 *pos;
        for (pos = data; pos < data+len; pos += 2)
        {
            guint16 ca_system_id = (pos[0] << 8) | pos[1];
            g_debug("    CA_system_id = 0x%04X", ca_system_id);
        }
        g_assert(pos == data+len);

        return TRUE;
    }

    return FALSE;
}

static gboolean
datetime_apdu(CAResourceManager *mgr,
              CATC *catc,
              guint16 session,
              guint32 tag,
              guint8 *data,
              guint len)
{
    return FALSE;
}

static const CAResourceInfo *
find_resource(CAResourceManager *mgr,
              guint res_class,
              guint res_type)
{
    for (const CAResourceInfo *res = builtin_resources; res->res_class; res++)
    {
        if (res->res_class == res_class && res->res_type == res_type)
            return res;
    }
    return NULL;
}

static gboolean
handle_spdu(CAResourceManager *mgr,
            guint8 tag,
            guint8 *data,
            guint len,
            CATC *catc)
{
    /* open_session_request */
    if (tag == 0x91)
    {
        const CAResourceInfo *res = NULL;
        int status = 0xF0; /* no such resource */

        guint res_id_type = data[0] >> 6;
        if (res_id_type != 3)
        {
            guint res_class = ((data[0] & 0x3F) << 8) | data[1];
            guint res_type = (data[2] << 2) | (data[3] >> 6);
            guint res_version = data[3] & 0x3F;

            res = find_resource(mgr, res_class, res_type);
            if (res != NULL)
                status = 0x00;
            if (status == 0x00 && res->res_version < res_version)
                status = 0xF2;
        }

        guint session = mgr->sessions->len + 1;
        if (status == 0x00)
            status = res->open(mgr, catc, session);
        if (status == 0x00)
        {
            struct session_info *i = g_new(struct session_info, 1);
            i->res = res;
            g_array_append_val(mgr->sessions, i);
        }

        guint32 res_identifier;
        if (res == NULL)
            res_identifier = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        else
            res_identifier = res_get_identifier(res);

        g_debug("resource requested: 0x%08X - response: 0x%02X", res_identifier, status);

        guint8 response[7];
        response[0] = status;

        response[1] = res_identifier >> 24;
        response[2] = (res_identifier >> 16) & 0xFF;
        response[3] = (res_identifier >> 8) & 0xFF;
        response[4] = res_identifier & 0xFF;

        if (status == 0x00)
        {
            response[5] = session >> 8;
            response[6] = session & 0xFF;
        }

        /* open session response */
        ca_t_c_send_spdu(catc, 0x92, response, 7);

        if (status == 0x00 && res->init != NULL)
            res->init(mgr, catc, session);

        return TRUE;
    }

    /* Session Number SPDU with attached APDU */
    if (tag == 0x90)
    {
        guint16 session = (data[0] << 8) | data[1];
        struct session_info *i = g_array_index(mgr->sessions, struct session_info *, session-1);

        guint32 apdu_tag = (data[2] << 16) | (data[3] << 8) | data[4];
        guint apdu_len;
        guint8 *apdu_data;
        decode_length_field(data+5, &apdu_len, &apdu_data);

        gboolean handled = FALSE;
        if (i->res->received_apdu)
        {
            handled = i->res->received_apdu(mgr, catc, session, apdu_tag, apdu_data, apdu_len);
        }
        if (!handled)
        {
            g_debug("APDU fell through:");
            g_debug("    session = 0x%04X (connected to resource %s)", session, i->res->name);
            g_debug("    tag     = 0x%06X", apdu_tag);
            g_debug("    len     = %d", apdu_len);
            for (int i = 0; i < apdu_len; i++)
                g_debug("    data[%2d] = 0x%02X", i, apdu_data[i]);
        }
        return TRUE;
    }

    return FALSE;
}

void
ca_resource_manager_manage_t_c(CAResourceManager *mgr,
                               CATC *catc)
{
    g_signal_connect_swapped(catc, "received-spdu", G_CALLBACK(handle_spdu), mgr);
}

static guint16
find_cainfo(CAResourceManager *mgr,
            CATC *catc)
{
    for (int i = 0; i < mgr->sessions->len; i++)
    {
        struct session_info *info = g_array_index(mgr->sessions, struct session_info *, i);
        if (info->res->res_class == 3)
            return i+1;
    }
    return 0;
}

gboolean
ca_resource_manager_has_cainfo(CAResourceManager *mgr,
                               CATC *catc)
{
    return !!find_cainfo(mgr, catc);
}

static void
copy_descriptors(guint8 **dst,
                 guint8 **src,
                 guint8 cmd_id)
{
    guint8 *cur = *src;
    guint16 srclen = ((cur[0] & 0x0F) << 8) | cur[1];
    guint16 dstlen = 0;

    for (cur = *src + 2; cur < *src + 2 + srclen;)
    {
        guint8 tag = cur[0];
        guint8 len = cur[1];

        /* CA_descriptor */
        if (tag == 9)
        {
            memcpy(*dst + 3 + dstlen, cur, 2 + len);
            dstlen += 2 + len;
        }

        cur += 2 + len;
    }
    g_assert(cur == *src + 2 + srclen);
    *src = cur;

    if (dstlen || 1) /* TODO */
    {
        dstlen += 1;
        (*dst)[0] = 0xF0 | (dstlen >> 8);
        (*dst)[1] = dstlen & 0xFF;
        (*dst)[2] = cmd_id;

        *dst += 2 + dstlen;
    }
}

void
ca_resource_manager_descramble_pmt(CAResourceManager *mgr,
                                   CATC *catc,
                                   guint8 *pmt)
{
    guint session = find_cainfo(mgr, catc);
    g_assert(session != 0);

    guint16 section_length = ((pmt[1] & 0x0F) << 8) | pmt[2];
    guint8 *end = pmt+3+section_length-4; /* includes CRC */

    guint8 ca_pmt[512];
    if (mgr->first)
    {
	mgr->first = FALSE;

        /* list management: only */
        ca_pmt[0] = 0x03;
    }
    else
    {
        /* list management: add */
        ca_pmt[0] = 0x04;
    }

    /* [1..3] program number, version, current_next */
    memcpy(ca_pmt+1, pmt+3, 3);

    guint8 *ca_cur = ca_pmt+4;
    guint8 *cur = pmt+10;

    /* copy descriptors if present, setting "ok_descrambling" */
    copy_descriptors(&ca_cur, &cur, 0x01);

    while (cur < end)
    {
        g_debug("pid = 0x%02X%02X", cur[1]&0x0F, cur[2]);

        /* stream_type, pid */
        memcpy(ca_cur, cur, 3);
        cur += 3;
        ca_cur += 3;

        /* same as above */
        copy_descriptors(&ca_cur, &cur, 0x01);
    }
    g_assert(cur == end);

    /* CA PMT */
    ca_t_c_send_apdu(
            catc,
            session,
            0x9F8032,
            ca_pmt,
            ca_cur-ca_pmt
        );
}

