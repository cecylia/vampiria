/*
 * ALSA streaming support
 *
 * Copyright © 2010  Devin Heitmueller <dheitmueller@kernellabs.com>
 *      Originally written  for usage at tvtime
 * Copyright © 2010  Jaroslav Kysela <perex@perex.cz>
 *      Derived from the alsa-driver test tool latency.c
 * Copyright © 2011  Mauro Carvalho Chehab <mchehab@redhat.com>
 *	Ported to xawtv, with bug fixes and improvements
 * Copyright © 2014  POJAR GEORGE  <geoubuntu@gmail.com>
 *	Ported to gnomeradio
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ALSA_STREAM_H
#define _ALSA_STREAM_H

int alsa_thread_startup(const char *pdevice, const char *cdevice, int latency,
			FILE *__error_fp,
			int __verbose);
void alsa_thread_stop(void);
int alsa_thread_is_running(void);

#endif
