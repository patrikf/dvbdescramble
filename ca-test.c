
#include "ca-device.h"
#include "ca-module.h"
#include "ca-resource-manager.h"
#include "tune.h"
#include <fcntl.h>
#include <linux/dvb/dmx.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int pat_dmx = -1;
const int use_program_number = 13001;

int main(int argc, char **argv)
{
    g_type_init();
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    CADevice *ca = ca_open_device("/dev/dvb/adapter0/ca0");
    ca_device_reset(ca);

    CAModule *cam = ca_device_open_module(ca, 0);
    ca_module_wait_ready(cam);

    CATC *catc = ca_module_create_t_c_block(cam, 1);

    if (catc)
    {
        CAResourceManager *camgr = ca_resource_manager_new();
        ca_resource_manager_manage_t_c(camgr, catc);
    }
    else
        g_debug("NO transport connection");

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    return 0;
}

