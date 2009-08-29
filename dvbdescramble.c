
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/dvb/dmx.h>

#include "ca-t-c.h"
#include "ca-resource-manager.h"

#define MAXARGS 256
#define DEFAULT_CA "/dev/dvb/adapter0/ca0"
#define DEFAULT_DMX "/dev/dvb/adapter0/demux0"
#define DEFAULT_SLOT 0

int pmtv[MAXARGS];
int programv[MAXARGS];

int pmtc, programc;

gchar *cadev = DEFAULT_CA;
gchar *dmxdev = DEFAULT_DMX;

CATC *catc = NULL;
CAResourceManager *camgr = NULL;

static void
poll_in(int fd, GIOFunc callback, gpointer data)
{
    GIOChannel *ioc = g_io_channel_unix_new(fd);
    g_io_add_watch(ioc, G_IO_IN, callback, data);
    g_io_channel_unref(ioc);
}

static int
get_demux()
{
    int fd = open(dmxdev, O_RDWR);
    if (fd == -1)
        g_error("could not open %s: %s", dmxdev, g_strerror(errno));
    return fd;
}

static gboolean
read_pmt(GIOChannel *unused1, GIOCondition unused2, gpointer data)
{
    int dmx = GPOINTER_TO_INT(data);

    guint8 buf[512];
    int r = read(dmx, buf, 512);
    g_assert(r > 0);

    guint16 program_number = (buf[3] << 8) | buf[4];

    guint8 section_number = buf[6];
    guint8 last_section = buf[7];
    g_assert(section_number == last_section);

    if (camgr == NULL || !ca_resource_manager_has_cainfo(camgr, catc))
        return TRUE;

    ca_resource_manager_descramble_pmt(camgr, catc, buf);

    g_debug("programme %d should be ready", program_number);

    /* FIXME: maybe replace one-shot behaviour later with continuous
     * version_number checks */
    close(dmx);

    return FALSE;
}

static void
listen_pmt(int pid)
{
    struct dmx_sct_filter_params param;
    param.pid = pid;
    param.timeout = 0;
    param.flags = DMX_CHECK_CRC | DMX_IMMEDIATE_START;

    memset(&param.filter, 0, sizeof(struct dmx_filter));

    /* table_id */
    param.filter.filter[0] = 0x02;
    param.filter.mask[0]   = 0xFF;

    /* current_next_indicator */
    param.filter.filter[3] = 0x01;
    param.filter.mask[3]   = 0x01;

    /* section_number */
    param.filter.filter[4] = 0x00;
    param.filter.mask[4]   = 0xFF;

#if 0
    /* program_number */
    param.filter.filter[1] = use_program_number >> 8;
    param.filter.filter[2] = use_program_number & 0xFF;
    param.filter.mask[1]   = 0xFF;
    param.filter.mask[2]   = 0xFF;
#endif

    int dmx = get_demux();
    if (ioctl(dmx, DMX_SET_FILTER, &param) == -1)
        g_error("ioctl failed: %s", g_strerror(errno));

    poll_in(dmx, (GIOFunc)read_pmt, GINT_TO_POINTER(dmx));
}

int pat_dmx = -1;

static gboolean
read_pat()
{
    guint8 buf[512];
    int r = read(pat_dmx, buf, 512);
    g_assert(r > 0);

    g_debug("received PAT");

    guint8 section_number = buf[6];
    guint8 last_section = buf[7];
    g_assert(section_number == last_section);

    guint16 length = ((buf[1] & 0x0F) << 8) | buf[2];
    guint8 *end = buf+3+length-4; /* includes CRC */

    guint8 *cur;
    for (cur = buf+8; cur < end; cur += 4)
    {
        guint16 program_number = (cur[0] << 8) | cur[1];
        guint16 pid = ((cur[2] & 0x1F) << 8) | cur[3];
        for (int i = 0; i < programc; i++)
        {
            if (programv[i] == program_number)
            {
                g_debug("programme %d has PMT on %d", program_number, pid);
                programv[i] = -1;
                listen_pmt(pid);
            }
        }
    }
    g_assert(cur == end);

    close(pat_dmx);
    return FALSE;
}

static void
listen_pat()
{
    struct dmx_sct_filter_params param;
    param.pid = 0x0000;
    param.timeout = 0;
    param.flags = DMX_CHECK_CRC | DMX_IMMEDIATE_START;

    memset(&param.filter, 0, sizeof(struct dmx_filter));

    /* table_id */
    param.filter.filter[0] = 0x00;
    param.filter.mask[0]   = 0xFF;

    /* current_next_indicator */
    param.filter.filter[3] = 0x01;
    param.filter.mask[3]   = 0x01;

    /* section_number */
    param.filter.filter[4] = 0x00;
    param.filter.mask[4]   = 0xFF;

    pat_dmx = get_demux();
    if (ioctl(pat_dmx, DMX_SET_FILTER, &param) == -1)
        g_error("ioctl failed: %s", g_strerror(errno));

    g_debug("listening for PAT");

    poll_in(pat_dmx, (GIOFunc)read_pat, NULL);
}

int main(int argc, char **argv)
{
    gboolean error = FALSE;
    gboolean help = FALSE;
    guint ca_slot = DEFAULT_SLOT;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp("--ca", argv[i]))
            cadev = argv[++i];
        else if (!strcmp("--demux", argv[i]))
            dmxdev = argv[++i];
        else if (!strcmp("--slot", argv[i]))
            ca_slot = atoi(argv[++i]);
        else if (!strcmp("-p", argv[i]) || !strcmp("--program", argv[i]))
            programv[programc++] = atoi(argv[++i]);
        else if (!strcmp("--pmt", argv[i]))
            pmtv[pmtc++] = atoi(argv[++i]);
        else if (!strcmp("--help", argv[i]))
        {
            help = TRUE;
            break;
        }
        else
        {
            gchar *after;
            gulong pid = strtoul(argv[i], &after, 10);
            if (!after || *after)
            {
                fprintf(stderr, "invalid argument (expected PID): %s\n", argv[i]);
                error = TRUE;
                break;
            }
        }
    }
    if (error || argc <= 1)
    {
        fprintf(stderr, "Usage: dvbdescramble [options]\n");
        fprintf(stderr, "see \"dvbdescramble --help\" for more information\n");
        return 1;
    }
    if (help)
    {
        fprintf(stderr, "Usage: dvbdescramble [options]\n");
        fprintf(stderr, "Set up a CA Module for descrambling.\n\n");
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  --ca DEVICE     CA device (default: %s)\n", DEFAULT_CA);
        fprintf(stderr, "  --demux DEVICE  demux device (default: %s)\n", DEFAULT_DMX);
        fprintf(stderr, "  --slot N        slot in CA device (default: %d)\n", DEFAULT_SLOT);
        fprintf(stderr, "  -p|--program N  DVB program_number\n");
        fprintf(stderr, "  --pmt N         PID of PMT\n\n");
        fprintf(stderr, "dvbdescramble will tell the CA Module to descramble the PIDs\n"
                        "given at the command line. For this to work, the CA Module has\n"
                        "to be fed the corresponding Programme Map Tables (PMTs). You can\n"
                        "either give the PMTs' PIDs directly using --pmt, or let\n"
                        "dvbdescramble look them up in the Program Allocation Table with\n"
                        "--program.\n");
        return 0;
    }

    g_type_init();

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    /* set up filters */
    if (programc > 0)
        listen_pat();
    for (int i = 0; i < pmtc; i++)
        listen_pmt(pmtv[i]);

    CADevice *ca = ca_open_device(cadev);
    if (ca == NULL)
        g_error("failed to open CA device %s", cadev);

    ca_device_reset(ca);

    CAModule *cam = ca_device_open_module(ca, ca_slot);
    if (cam == NULL)
        g_error("failed to open module in slot %d", ca_slot);

    g_debug("waiting for module");
    ca_module_wait_ready(cam);
    g_assert(ca_module_ready(cam));

    catc = ca_module_create_t_c_block(cam, 1);
    if (catc == NULL)
        g_error("failed to create transport connection");

    camgr = ca_resource_manager_new();
    if (camgr == NULL)
        g_error("failed to create CA Resource Manager");

    ca_resource_manager_manage_t_c(camgr, catc);

    g_main_loop_run(loop);

    return 0;
}

