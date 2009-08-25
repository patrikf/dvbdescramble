
#ifndef CA_T_C_H
#define CA_T_C_H

#include <glib-object.h>

#define CA_TYPE_T_C (ca_t_c_get_type())
GType ca_t_c_get_type();
typedef struct _CATCClass CATCClass;
typedef struct _CATC CATC;

#include "ca-module.h"

struct _CATCClass
{
    GObjectClass _parent;
    gboolean (*received_tpdu)(CATC *catc,
                              guint8 tag,
                              guint8 *data,
                              guint len);
    gboolean (*received_spdu)(CATC *catc,
                              guint8 tag,
                              guint8 *data,
                              guint len);
};

struct _CATC
{
    GObject _parent;
    CAModule *module;
    guint8 id;
    guint poll_id;
};

CATC *ca_t_c_new(CAModule *module,
                 guint8 id);

void
ca_t_c_send_data(CATC *catc,
                 const guint8 *data,
                 guint len);

void
ca_t_c_send_spdu(CATC *catc,
                 guint8 tag,
                 const guint8 *data,
                 guint len);

void
ca_t_c_send_apdu(CATC *catc,
                 guint16 session,
                 guint32 tag,
                 const guint8 *data,
                 guint len);

#endif

