
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

CATC *catc = NULL;
CAResourceManager *camgr = NULL;

static void
poll_in(int fd, GIOFunc callback)
{
    GIOChannel *ioc = g_io_channel_unix_new(fd);
    g_io_add_watch(ioc, G_IO_IN, callback, NULL);
    g_io_channel_unref(ioc);
}

static void
demux_ts_tap(int pid)
{
    int dmx = open("/dev/dvb/adapter0/demux0", O_RDWR);

    struct dmx_pes_filter_params param;

    param.pid = pid;
    param.input = DMX_IN_FRONTEND;
    param.output = DMX_OUT_TS_TAP;
    param.pes_type = DMX_PES_OTHER;
    param.flags = DMX_IMMEDIATE_START;

    int r = ioctl(dmx, DMX_SET_PES_FILTER, &param);
    if (r != 0)
        perror("ioctl dmx_pes");
}

static void
start_stream()
{
    demux_ts_tap(160);
    demux_ts_tap(161);
}

static gboolean
read_pmt()
{
    guint8 buf[512];
    int r = read(pat_dmx, buf, 512);

    guint8 section_number = buf[6];
    guint8 last_section = buf[7];
    g_assert(section_number == last_section);

    if (camgr == NULL || !ca_resource_manager_has_cainfo(camgr, catc))
        return TRUE;

    // TODO
    start_stream();

    ca_resource_manager_descramble_pmt(camgr, catc, buf);

    /* FIXME: maybe replace one-shot behaviour later with continuous
     * version_number checks */
    close(pat_dmx);

    return FALSE;
}

static gboolean
read_pat()
{
    guint16 pmt_pid = 0;

    guint8 buf[512];
    int r = read(pat_dmx, buf, 512);

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
        if (program_number == use_program_number)
            pmt_pid = pid;
    }
    g_assert(cur == end);

    if (pmt_pid)
    {
        g_debug("Found PMT for program number %d: PID 0x%04X", use_program_number, pmt_pid);

        struct dmx_sct_filter_params param;
        param.pid = pmt_pid;
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

        /* program_number */
        param.filter.filter[1] = use_program_number >> 8;
        param.filter.filter[2] = use_program_number & 0xFF;
        param.filter.mask[1]   = 0xFF;
        param.filter.mask[2]   = 0xFF;

        ioctl(pat_dmx, DMX_STOP);
        ioctl(pat_dmx, DMX_SET_FILTER, &param);

        poll_in(pat_dmx, (GIOFunc)read_pmt);

        return FALSE;
    }

    return TRUE;
}

int main(int argc, char **argv)
{
    g_type_init();

    int fe = open("/dev/dvb/adapter0/frontend0", O_RDWR);

    /* Tune to ORF */
    tune_lnb(fe, 12692250, 'h', 22000000);
    g_debug("Tuned to ORF");

    /* Set up demux for PMT */
    pat_dmx = open("/dev/dvb/adapter0/demux0", O_RDWR);
    if (pat_dmx < 0)
        perror("dmx");

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

    ioctl(pat_dmx, DMX_SET_FILTER, &param);

    poll_in(pat_dmx, (GIOFunc)read_pat);

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
#if 0
    g_main_loop_run(loop);
    return 0;
#endif

    CADevice *ca = ca_open_device("/dev/dvb/adapter0/ca0");
    CAModule *cam = ca_device_open_module(ca, 0);
    catc = ca_module_create_t_c_block(cam, 1);

    camgr = ca_resource_manager_new();

    ca_resource_manager_manage_t_c(camgr, catc);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    return 0;
}

