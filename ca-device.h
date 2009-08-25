
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

