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

#include "diseqc.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>

static void
select_band(int fd,
	    char polarisation,
	    bool hiband)
{
    assert(polarisation == 'h' || polarisation == 'v');
    char cmd[2];

    cmd[0] = DISEQC_WRITE_N0; // the all-in-one set command
    /*
     * high nibble: clear
     * low nibble: 8 = options switch / satellite / horiz. pol. / high band = 1
     *
     * We set only Polarisation and Band.
     */
    uint8_t set = ((polarisation == 'h') << 1) | hiband;
    uint8_t mask = 0x03; // 0011
    cmd[1] = ((~set & mask) << 4) | (set & mask);

    secStatus sec;
    sec.voltage = (polarisation == 'v' ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18);
    sec.tone = (hiband ? SEC_TONE_ON : SEC_TONE_OFF);

    diseqc_lnb(fd, cmd, 1, sec, BURST_NONE);
}

void
tune_lnb(int fd,
	 uint32_t frequency,
	 char polarisation,
	 uint32_t symbolrate)
{
    static const uint32_t switch_freq = 11700*1000;
    static const uint32_t lo_freq =  9750*1000;
    static const uint32_t hi_freq = 10600*1000;
    bool hiband = (frequency > switch_freq);

    /* Tune LNB, polarisation and band */
    select_band(fd, polarisation, hiband);

    /* Tune frequency */
    struct dvb_frontend_parameters feparm;
    feparm.frequency = frequency - (hiband ? hi_freq : lo_freq);
    feparm.inversion = INVERSION_AUTO;
    feparm.u.qpsk.symbol_rate = symbolrate;
    feparm.u.qpsk.fec_inner = FEC_AUTO;

    if (ioctl(fd, FE_SET_FRONTEND, &feparm) == -1)
	perror("tune: FE_SET_FRONTEND failed");

    /* Wait until TS is ready */
    struct dvb_frontend_event ev;
    while (1)
    {
	if (ioctl(fd, FE_GET_EVENT, &ev) == -1)
	{
	    perror("tune: FE_GET_EVENT failed");
	    continue;
	}
	if (ev.status & FE_HAS_LOCK)
	    break;
    }
}


