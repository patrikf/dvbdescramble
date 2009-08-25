
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

