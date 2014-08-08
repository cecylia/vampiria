/*
 * Copyright © 2001  Jörgen Scheibengruber  <mfcn@gmx.de>
 * Copyright © 2014  POJAR GEORGE  <geoubuntu@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <libv4l1-videodev.h>

#include "v4l1.h"

typedef struct _V4L1RadioDev V4L1RadioDev;
struct _V4L1RadioDev
{
	struct _RadioDev parent;

	int fd;
	int freq_fact;
};

static char *v4l1_radio_get_name(RadioDev *radio_dev, char *device)
{
	V4L1RadioDev *dev = (V4L1RadioDev*)radio_dev;
	struct video_capability caps;

	if ((dev->fd = open(device, O_RDONLY)) < 0)
		return NULL;

	if (ioctl(dev->fd, VIDIOCGCAP, &caps) < 0) {
		perror("VIDIOCGCAP");
		return NULL;
	}
	close(dev->fd);

	return strdup((char *)caps.name);
}

static int v4l1_radio_init(RadioDev *radio_dev, char *device)
{
	V4L1RadioDev *dev = (V4L1RadioDev*)radio_dev;
	struct video_tuner tuner;
	
	if ((dev->fd = open(device, O_RDONLY)) < 0)
		return 0;
	
	tuner.tuner = 0;
	if (ioctl (dev->fd, VIDIOCGTUNER, &tuner) < 0) {
		dev->freq_fact = 16;
	} else {
		if ((tuner.flags & VIDEO_TUNER_LOW) == 0)
			dev->freq_fact = 16;
		else
			dev->freq_fact = 16000;
	}
	
	return 1;
}

static int v4l1_radio_is_init(RadioDev *radio_dev)
{
	V4L1RadioDev *dev = (V4L1RadioDev*)radio_dev;
	return (dev->fd >= 0);
}

static void v4l1_radio_set_freq(RadioDev *radio_dev, float freq)
{
	V4L1RadioDev *dev = (V4L1RadioDev*)radio_dev;
	int ifreq;

	if (dev->fd < 0)
		return;
    
	ifreq = (freq+1.0/32)*dev->freq_fact;

	/* FIXME: Do we need really need these checks? */
	if ((freq > 108) || (freq < 65))
		return;

	assert ((freq <= 108) && (freq > 65));

	if (ioctl(dev->fd, VIDIOCSFREQ, &ifreq) < 0)
		perror ("VIDIOCSFREQ");
}

static void v4l1_radio_mute(RadioDev *radio_dev, int mute)
{
	V4L1RadioDev *dev = (V4L1RadioDev*)radio_dev;
	struct video_audio vid_aud;

	if (dev->fd < 0)
		return;

	if (ioctl(dev->fd, VIDIOCGAUDIO, &vid_aud)) {
		perror("VIDIOCGAUDIO");
		memset (&vid_aud, 0, sizeof (struct video_audio));
	}
	if (mute) {
		vid_aud.flags |= VIDEO_AUDIO_MUTE;
	} else {
		vid_aud.volume = 0xFFFF;
		vid_aud.flags &= ~VIDEO_AUDIO_MUTE;
		vid_aud.mode = VIDEO_SOUND_STEREO;
	}
	if (ioctl(dev->fd, VIDIOCSAUDIO, &vid_aud))
		perror("VIDIOCSAUDIO");
}

static int v4l1_radio_is_muted(RadioDev *radio_dev)
{
	V4L1RadioDev *dev = (V4L1RadioDev*)radio_dev;
	struct video_audio vid_aud;

	if (dev->fd < 0)
		return -1;

	if (ioctl (dev->fd, VIDIOCGAUDIO, &vid_aud)) {
		perror ("VIDIOCGAUDIO");
		return -1;
	}

	return (vid_aud.flags & VIDEO_AUDIO_MUTE);
}

static int v4l1_radio_get_stereo(RadioDev *radio_dev)
{
	V4L1RadioDev *dev = (V4L1RadioDev*)radio_dev;
	struct video_audio va;
	va.mode=-1;

	if (dev->fd < 0)
		return -1;
    
	if (ioctl (dev->fd, VIDIOCGAUDIO, &va) < 0)
		return -1;
	if (va.mode == VIDEO_SOUND_STEREO)
		return 1;
	else 
		return 0;
}

static int v4l1_radio_get_signal(RadioDev *radio_dev)
{
	V4L1RadioDev *dev = (V4L1RadioDev*)radio_dev;
	struct video_tuner vt;
	int signal;

	if (dev->fd < 0)
		return -1;

	memset(&vt,0,sizeof(vt));
	ioctl (dev->fd, VIDIOCGTUNER, &vt);
	signal=vt.signal>>13;

	return signal;
}

static void v4l1_radio_finalize(RadioDev *radio_dev)
{
	V4L1RadioDev *dev = (V4L1RadioDev*)radio_dev;
	
	if (dev->fd >= 0)
		close(dev->fd);
	free(dev);
}

RadioDev* v4l1_radio_dev_new(void)
{
	RadioDev *dev;
	V4L1RadioDev *v4l1_dev;

	v4l1_dev = malloc(sizeof(V4L1RadioDev));
	v4l1_dev->fd = -1;
	dev = (RadioDev*)v4l1_dev;

	dev->get_name   = v4l1_radio_get_name;
	dev->init       = v4l1_radio_init;
	dev->is_init    = v4l1_radio_is_init;
	dev->set_freq   = v4l1_radio_set_freq;
	dev->mute       = v4l1_radio_mute;
	dev->is_muted   = v4l1_radio_is_muted;
	dev->get_stereo = v4l1_radio_get_stereo;
	dev->get_signal = v4l1_radio_get_signal;
	dev->finalize   = v4l1_radio_finalize;

	return dev;
}

