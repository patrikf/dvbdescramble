
#ifndef DISEQC_H
#define DISEQC_H

#include <stdint.h>
#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>

typedef enum
{
    DISEQC_WRITE_N0 = 0x38,
} diseqcCommand;

typedef enum
{
    DISEQC_MAGIC = 0xE0,
    DISEQC_MASK = 0x0F,

    DISEQC_MASTER = 0x0,
    DISEQC_REPEAT = 0x1,
    DISEQC_REPLY = 0x2,

    DISEQC_SLAVE_OK = 0x4,
    DISEQC_SLAVE_UNSUPPORTED = 0x5,
    DISEQC_SLAVE_EPARITY = 0x6,
    DISEQC_SLAVE_ECOMMAND = 0x7,
} diseqcFraming;

typedef struct
{
    fe_sec_voltage_t voltage;
    fe_sec_tone_mode_t tone;
} secStatus;

typedef enum
{
    BURST_NONE,
    BURST_A,
    BURST_B
} secBurst;

void
diseqc_raw(int fd,
           struct dvb_diseqc_master_cmd *cmd,
           secStatus status,
           secBurst burst);

void
diseqc_lnb(int fd,
	   char *command,
	   uint8_t datalen,
	   secStatus status,
	   secBurst burst);

#endif

