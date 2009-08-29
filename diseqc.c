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

#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

static bool
do_burst(int fd, secBurst burst)
{
    if (burst == BURST_NONE)
	return false;
    if (ioctl(fd, FE_DISEQC_SEND_BURST, (burst == BURST_A ? SEC_MINI_A : SEC_MINI_B)) == -1)
	perror("do_burst: SEND_BURST failed");
    return true;
}

/*
 * Send a raw DiSEqC command.
 *
 * cmd and burst (if specified) will be sent and voltage/tone will be switched
 * in compliance with the Eutelsat DiSEqC specification.
 */
void
diseqc_raw(int                           fd,
           struct dvb_diseqc_master_cmd *cmd,
           secStatus			 status,
           secBurst			 burst)
{
    // Tone OFF
    if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
	perror("diseqc_raw: failed to switch off tone");
    
    // Switch Voltage
    if (ioctl(fd, FE_SET_VOLTAGE, status.voltage) == -1)
	perror("diseqc_raw: failed to switch voltage");

    usleep(15 * 1000);

    // DiSEqC message
    if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, cmd) == -1)
	perror("diseqc_raw: sending DiSEqC command failed");
    // TODO: wait for DiSEqC response here (if requested)

    usleep(15 * 1000);

    // Burst, if any, comes here
    if (do_burst(fd, burst))
	usleep(15 * 1000);

    // Switch tone to specified state
    if (ioctl(fd, FE_SET_TONE, status.tone) == -1)
	perror("diseqc_raw: failed to switch tone");
}

void
diseqc_lnb(int         fd,
	   char       *command, /* 1..4 */
	   uint8_t     datalen, /* 0..3 */
	   secStatus   status, /* after DiSEqC command */
	   secBurst    burst)
{
    struct dvb_diseqc_master_cmd cmd;
    memset(cmd.msg, 0, 6);
    cmd.msg[0] = DISEQC_MAGIC | DISEQC_MASTER;
    cmd.msg[1] = 0x10;
    memcpy(cmd.msg+2, command, datalen+1);
    cmd.msg_len = datalen+3;

    diseqc_raw(fd, &cmd, status, burst);
}
