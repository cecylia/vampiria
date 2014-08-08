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

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/mman.h>
#include <string.h>
#include "mixer.h"

static char *mixer_device = "/dev/mixer";
static char *names[] = SOUND_DEVICE_NAMES;
static int saved_volume = (50 << 8 & 0xFF00) | (50 & 0x00FF);
static int mixer_channel = SOUND_MIXER_LINE;
static int mixer_dev_mask = 1 << SOUND_MIXER_LINE;
static int muted = 0;
static int mutecount = 0;
static int fd = -1;

static int oss_get_volume(void)
{
	int volume = 0;

	if (fd < 0)
		fd = open(mixer_device, O_RDONLY);
	if (fd != -1) {
		int r, l, cmd, devs;
		ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
		if (devs & mixer_dev_mask)
			cmd = MIXER_READ(mixer_channel);
		else
			return volume;

		ioctl(fd, cmd, &volume);
		r = (volume & 0xFF);
		l = (volume & 0xFF00) >> 8;
		volume = (r + l) / 2;
		volume = CLAMP (volume, 0, 100);
	}

	return volume;
}

static char **oss_get_channel(void)
{
	int devs;
	char **result;
	
	if ((ioctl(fd, SOUND_MIXER_READ_RECMASK, &devs)) == -1) {
		return NULL;
	} else {
		int i, o;

		result = malloc(sizeof(char*)*SOUND_MIXER_NRDEVICES);
		o = 0;
		for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
				int res;

				res = (devs >> i)%2;
				if (res) {
					result[o] = malloc(strlen(names[i])+1);
					sprintf(result[o], "%s", names[i]); 
					o++;
				}
				result[o] = NULL;
			}
	}

	return result;
}

static int oss_get_unmute_volume(void)
{
	if (muted) {
		return saved_volume;
	} else {
		int volume, devs;

		if (fd < 0)
			fd = open(mixer_device, O_RDONLY);
		if (fd != -1) {
			int cmd;

			ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
			if (devs & mixer_dev_mask)
				cmd = MIXER_READ(mixer_channel);
			else
				return -1;

			ioctl(fd, cmd, &volume);
			volume = CLAMP (volume, 0, 100);
			return volume;
		}
	}

	return -1;
}

static void oss_set_volume(int value)
{
	int volume, devs;

	if (fd < 0)
		fd = open(mixer_device, O_RDONLY);
	if (fd != -1) {
		int cmd;

		ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
		if (devs & mixer_dev_mask)
			cmd = MIXER_WRITE(mixer_channel);
		else
			return;

		value = CLAMP (value, 0, 100);
		volume = (value << 8) + value;
		ioctl(fd, cmd, &volume);
		muted = 0;
		mutecount = 0;
	}
}

static void oss_mute(int mute)
{
	int volume, cmd, devs;

	/**
	 * Make sure that if multiple users mute the card,
	 * we only honour the last one.
	 */
	if (!mute && mutecount)
		mutecount--;
	if (mutecount)
		return;

	if (fd < 0)
		fd = open(mixer_device, O_RDONLY);

	if (mute) {
		mutecount++;
		if (fd != -1) {
			/* Save volume */
			ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
			if (devs & mixer_dev_mask)
				cmd = MIXER_READ(mixer_channel);
			else
				return;

			ioctl(fd, cmd, &volume);
			saved_volume = volume;

			/* Now set volume to 0 */
			ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
			if (devs & mixer_dev_mask)
				cmd = MIXER_WRITE(mixer_channel);
			else
				return;

			volume = 0;
			ioctl(fd, cmd, &volume);

			muted = 1;
			return;
		}
	} else {
		if (fd != -1) {
			ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
			if (devs & mixer_dev_mask)
				cmd = MIXER_WRITE(mixer_channel);
			else
				return;

			volume = saved_volume;
			ioctl(fd, cmd, &volume);
			muted = 0;
			return;
		}
	}
}

static int oss_is_muted(void)
{
	return muted;
}

static int oss_set_device(const char *card, const char* channel)
{
	int i;

	mixer_channel = SOUND_MIXER_LINE;
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (!strcasecmp(channel, names[i])) {
			mixer_channel = i;
			break;
		}
	}

	fd = open(card, O_RDWR);

	if (mixer_channel < 0) {
		fprintf(stderr, "mixer: No such mixer channel '%s'.\n", channel);
		return -1;
	}

	if (fd < 0) {
		fprintf(stderr,
                        "mixer: Can't open card %s, "
                        "mixer volume and mute unavailable.\n", card);
		return 0;
	}

	mixer_dev_mask = 1 << mixer_channel;

	return 1;
}

static void oss_set_state(int is_muted, int unmute_volume)
{
	/**
	 * 1. we come back unmuted: Don't touch anything
	 * 2. we don't have a saved volume: Don't touch anything
	 * 3. we come back muted and we have a saved volume:
	 *    - if gnomeradio muted it, unmute to old volume
	 *    - if user did it, remember that we're muted and old volume
	 */
	if (oss_get_volume() == 0 && unmute_volume > 0) {
		saved_volume = unmute_volume;
		muted = 1;

		if (!is_muted)
			oss_mute(0);
	}
}

static int oss_set_record_device(void)
{
	int devmask, recmask;

	if (fd <= 0)
		return 0;

	if (mixer_channel < 0)
		return 0;

	if ((ioctl(fd, SOUND_MIXER_READ_RECMASK, &devmask)) == -1)
		return 0;
	
	recmask = 1 << mixer_channel;
	if (!(recmask & devmask))
		return 0;

	if ((ioctl(fd, SOUND_MIXER_WRITE_RECSRC, &recmask)) == -1)
		return 0;

	return 1;
}			

static void oss_close_device(void)
{
	if (fd >= 0)
		close(fd);

	saved_volume = (50 << 8 & 0xFF00) | (50 & 0x00FF);
	mixer_channel = SOUND_MIXER_LINE;
	mixer_dev_mask = 1 << SOUND_MIXER_LINE;
	muted = 0;
	mutecount = 0;
	fd = -1;
}

struct mixer oss_mixer = {
	.set_device = oss_set_device,
	.get_channel = oss_get_channel,
	.set_state = oss_set_state,
	.get_volume = oss_get_volume,
	.get_unmute_volume = oss_get_unmute_volume,
	.set_volume = oss_set_volume,
	.mute = oss_mute,
	.is_muted = oss_is_muted,
	.set_record_device = oss_set_record_device,
	.close_device = oss_close_device,
};
