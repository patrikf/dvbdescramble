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

#ifndef TUNE_H
#define TUNE_H

#include <stdint.h>

void
tune_lnb(int fd,
	 uint32_t frequency,
	 char polarisation,
	 uint32_t symbolrate);

#endif

