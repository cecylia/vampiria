/**
 * Copyright © 2002-2003  Doug Bell  <drbell@users.sourceforge.net>

 * Some mixer routines from http://mplayer.sourceforge.net
 * Copyright © 2000-2002  Árpád Gereöffy (A'rpi/ESP-team) & others
 * Copyright © 2014  POJAR GEORGE  <geoubuntu@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mixer.h"

static int null_set_device (const char *card, const char* channel)
{
	return 0;
}

static char **null_get_channel (void)
{
	return 0;
}

static void null_set_state (int is_muted, int unmute_volume)
{
}

static int null_get_volume (void)
{
	return 0;
}

static int null_get_unmute_volume (void)
{
	return 0;
}

static void null_set_volume (int value)
{
}

static void null_mute (int mute)
{
}

static int null_is_muted (void)
{
	return 0;
}

static int null_set_record_device (void)
{
	return 0;
}

static void null_close_device (void)
{
}

static struct mixer null_mixer = {
	.set_device        = null_set_device,
	.get_channel       = null_get_channel,
	.set_state         = null_set_state,
	.get_volume        = null_get_volume,
	.get_unmute_volume = null_get_unmute_volume,
	.set_volume        = null_set_volume,
	.mute              = null_mute,
	.is_muted          = null_is_muted,
	.set_record_device = null_set_record_device,
	.close_device      = null_close_device,
};

static struct mixer *mixers[] = {
	&alsa_mixer,
	&oss_mixer,
	&null_mixer
};

struct mixer *mixer = &null_mixer;

int mixer_set_device (const char *card, const char *channel)
{
	int i, ret = 0;

	mixer->close_device ();
	for (i = 0; i < sizeof(mixers)/sizeof(mixers[0]); i++) {
		mixer = mixers[i];
		if (!mixer)
			continue;
		ret = mixer->set_device (card, channel);
		if (ret == 1)
			break;
	}

	return ret;
}
