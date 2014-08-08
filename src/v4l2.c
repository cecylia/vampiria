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
#include <linux/videodev2.h>

#include "v4l2.h"

typedef struct _V4L2RadioDev V4L2RadioDev;
struct _V4L2RadioDev
{
	struct _RadioDev parent;

	int fd;
	int freq_fact;
	int radio_rangelow;
	int radio_rangehigh;
};

static char *v4l2_radio_get_name(RadioDev *radio_dev, char *device)
{
	V4L2RadioDev *dev = (V4L2RadioDev*)radio_dev;
	struct v4l2_capability caps;

	if ((dev->fd = open(device, O_RDONLY)) < 0)
		goto error;

	if (ioctl(dev->fd, VIDIOC_QUERYCAP, &caps) < 0) {
		perror("VIDIOC_QUERYCAP");
		goto error;
	}
	close(dev->fd);

	return strdup((char *)caps.card);

 error:
	if (dev->fd >= 0)
		close(dev->fd);
	dev->fd = -1;

	return NULL;
}

static int v4l2_radio_init(RadioDev *radio_dev, char *device)
{
	V4L2RadioDev *dev = (V4L2RadioDev*)radio_dev;
	struct v4l2_capability caps;
	struct v4l2_tuner tuner;
	
	if ((dev->fd = open(device, O_RDONLY)) < 0)
		goto error;
	
	/* does this device provide a tuner? */
	memset(&caps, 0, sizeof(caps));
	if (ioctl(dev->fd, VIDIOC_QUERYCAP, &caps) < 0) {
		perror("VIDIOC_QUERYCAP");
		goto error;
	}
	if ((caps.capabilities & V4L2_CAP_TUNER) == 0) {
		fprintf(stderr, "Not a radio tuner\n");
		goto error;
	}

	/* check the tuner */
	memset(&tuner, 0, sizeof(tuner));
	tuner.index = 0;
	if (ioctl(dev->fd, VIDIOC_G_TUNER, &tuner) < 0) {
		perror("VIDIOC_G_TUNER");
		goto error;
	}
	if (tuner.type != V4L2_TUNER_RADIO) {
		fprintf(stderr, "Not a radio tuner\n");
		goto error;
	}
	/* Does this tuner expect data in 62.5Hz or 62.5kHz multiples? */
	dev->radio_rangelow = tuner.rangelow;
	dev->radio_rangehigh = tuner.rangehigh;
	if ((tuner.capability & V4L2_TUNER_CAP_LOW) != 0)
		dev->freq_fact = 16000;
	else
		dev->freq_fact = 16;
	
	return 1;

 error:
	if (dev->fd >= 0)
		close(dev->fd);
	dev->fd = -1;

	return 0;
}

static int v4l2_radio_is_init(RadioDev *radio_dev)
{
	V4L2RadioDev *dev = (V4L2RadioDev*)radio_dev;

	return (dev->fd >= 0);
}

static void v4l2_radio_set_freq(RadioDev *radio_dev, float frequency)
{
	V4L2RadioDev *dev = (V4L2RadioDev*)radio_dev;
	struct v4l2_frequency freq;

	if (dev->fd < 0)
		return;

	memset(&freq, 0, sizeof(freq));
	freq.tuner = 0;
	freq.type = V4L2_TUNER_RADIO;
	freq.frequency = frequency * dev->freq_fact;

	if (freq.frequency < dev->radio_rangelow ||
	    freq.frequency > dev->radio_rangehigh)
		return;
	if (ioctl(dev->fd, VIDIOC_S_FREQUENCY, &freq) < 0)
		perror("VIDIOC_S_FREQUENCY");
}

static void v4l2_radio_mute(RadioDev *radio_dev, int mute)
{
	V4L2RadioDev *dev = (V4L2RadioDev*)radio_dev;
	struct v4l2_control control;

	if (dev->fd < 0)
		return;

	memset(&control, 0, sizeof(control));
	control.id = V4L2_CID_AUDIO_MUTE;
	control.value = mute;
	if (ioctl(dev->fd, VIDIOC_S_CTRL, &control) < 0)
		perror("VIDIOC_S_CTRL");
}

static int v4l2_radio_is_muted(RadioDev *radio_dev)
{
	V4L2RadioDev *dev = (V4L2RadioDev*)radio_dev;
	struct v4l2_control control;

	if (dev->fd < 0)
		return -1;

	memset(&control, 0, sizeof(control));
	control.id = V4L2_CID_AUDIO_MUTE;

	if (ioctl (dev->fd, VIDIOC_G_CTRL, &control) < 0) {
		perror ("VIDIOC_S_CTRL");
		return -1;
	}

	return control.value;
}

static int v4l2_radio_get_stereo(RadioDev *radio_dev)
{
	V4L2RadioDev *dev = (V4L2RadioDev*)radio_dev;
	struct v4l2_tuner tuner;

	if (dev->fd < 0)
		return -1;

	memset(&tuner, 0, sizeof(tuner));
	tuner.index = 0;
	if (ioctl(dev->fd, VIDIOC_G_TUNER, &tuner) < 0) {
		perror("VIDIOC_G_TUNER");
		return -1;
	}

	return tuner.audmode == V4L2_TUNER_MODE_STEREO;
}

static int v4l2_radio_get_signal(RadioDev *radio_dev)
{
	V4L2RadioDev *dev = (V4L2RadioDev*)radio_dev;
	struct v4l2_tuner tuner;

	if (dev->fd < 0)
		return -1;

	memset(&tuner, 0, sizeof(tuner));
	tuner.index = 0;
	if (ioctl(dev->fd, VIDIOC_G_TUNER, &tuner) < 0) {
		perror("VIDIOC_G_TUNER");
		return -1;
	}

	return tuner.signal >> 13;
}

static void v4l2_radio_finalize(RadioDev *radio_dev)
{
	V4L2RadioDev *dev = (V4L2RadioDev*)radio_dev;
	
	if (dev->fd >= 0)
		close(dev->fd);
	free(dev);
}

RadioDev* v4l2_radio_dev_new(void)
{
	RadioDev *dev;
	V4L2RadioDev *v4l2_dev;

	v4l2_dev = malloc (sizeof (V4L2RadioDev));
	v4l2_dev->fd = -1;
	dev = (RadioDev*)v4l2_dev;

	dev->get_name   = v4l2_radio_get_name;
	dev->init       = v4l2_radio_init;
	dev->is_init    = v4l2_radio_is_init;
	dev->set_freq   = v4l2_radio_set_freq;
	dev->mute       = v4l2_radio_mute;
	dev->is_muted   = v4l2_radio_is_muted;
	dev->get_stereo = v4l2_radio_get_stereo;
	dev->get_signal = v4l2_radio_get_signal;
	dev->finalize   = v4l2_radio_finalize;

	return dev;
}

