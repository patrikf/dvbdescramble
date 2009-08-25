
#ifndef CA_RESOURCE_MANAGER_H
#define CA_RESOURCE_MANAGER_H

#include <glib-object.h>

#define CA_TYPE_RESOURCE_MANAGER (ca_resource_manager_get_type())

GType
ca_resource_manager_get_type();

typedef struct _CAResourceManagerClass CAResourceManagerClass;
typedef struct _CAResourceManager CAResourceManager;

typedef struct _CAResourceInfo CAResourceInfo;

#include "ca-t-c.h"

struct _CAResourceManagerClass
{
    GObjectClass _parent;
};

struct _CAResourceManager
{
    GObject _parent;
    GList *resources;
    GArray *sessions;
};

struct _CAResourceInfo
{
    guint res_class : 14;
    guint res_type : 10;
    guint res_version : 6;

    const char *name;

    /* Returns: 0 or an error as described in EN 50221, page 20 */
    guint8 (*open)(CAResourceManager *mgr,
                   CATC *catc,
                   guint16 session);

    /* called after opening the session */
    void (*init)(CAResourceManager *mgr,
                 CATC *catc,
                 guint16 session);

    gboolean (*received_apdu)(CAResourceManager *mgr,
                              CATC *catc,
                              guint16 session,
                              guint32 tag,
                              guint8 *data,
                              guint len);

    void (*close)(CAResourceManager *mgr,
                  CATC *catc,
                  guint16 session);
};

CAResourceManager *
ca_resource_manager_new();

void
ca_resource_manager_manage_t_c(CAResourceManager *mgr,
                               CATC *catc);


gboolean
ca_resource_manager_has_cainfo(CAResourceManager *mgr,
                               CATC *catc);

void
ca_resource_manager_descramble_pmt(CAResourceManager *mgr,
                                   CATC *catc,
                                   guint8 *pmt);

#endif

