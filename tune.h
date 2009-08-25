
#ifndef TUNE_H
#define TUNE_H

#include <stdint.h>

void
tune_lnb(int fd,
	 uint32_t frequency,
	 char polarisation,
	 uint32_t symbolrate);

#endif

