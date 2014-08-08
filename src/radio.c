/*
 * Copyright © 2001  Jörgen Scheibengruber  <mfcn@gmx.de>
 * Copyright © 2014  POJAR GEORGE  <geoubuntu@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or(at your option) any later version.
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

#include "radio.h"
#include "v4l1.h"
#include "v4l2.h"
#include "alsa_stream.h"
#include "media_devices.h"

extern int   audio_loopback;
extern char *audio_output;
extern char *audio_input;
extern int   audio_latency;
extern int   verbose;

static RadioDev *dev;

int
radio_init (char       *device,
            DriverType  driver)
{
	int rv = -1;

	switch (driver) {
		case DRIVER_ANY:
		case DRIVER_V4L2:
		default:
			goto try_v4l2;
		case DRIVER_V4L1:
			goto try_v4l1;
	}

 try_v4l1:
	dev = v4l1_radio_dev_new();
	rv = dev->init (dev, device);
	if (rv == 0) {
        	fprintf(stderr, "v4l1: Initialization failed\n");
		dev->finalize (dev);
		dev = NULL;
		if (audio_loopback)
			audio_loopback = 0;
		if (driver != DRIVER_ANY)
			goto failure;
	} else {
		goto success;
	}

 try_v4l2:
	dev = v4l2_radio_dev_new();
	rv = dev->init (dev, device);
	if (rv == 0) {
        	fprintf(stderr, "v4l2: Initialization failed\n");
		dev->finalize (dev);
		dev = NULL;
		if (audio_loopback)
			audio_loopback = 0;
		if (driver != DRIVER_ANY)
			goto failure;
	} else {
		goto success;
	}

 success:
	if (audio_loopback) {
		void *md;
		const char *p = NULL;

		md = discover_media_devices();

		p = strrchr(device, '/');
		if (p)
			p++;
		else
			p = device;

		p = get_associated_device(md, NULL, MEDIA_SND_CAP, p, MEDIA_V4L_RADIO);
		if (p) {
			if (audio_input == NULL)
				audio_input = strdup(p);
			if (audio_output == NULL)
				audio_output = strdup("default");
		} else {		
			audio_loopback = 0;
		}

		free_media_devices(md);
	}

	radio_unmute();

 failure:
	return rv;
}

int
radio_is_init (void)
{
	if (dev)
		return dev->is_init(dev);
	else
		return 0;
}

void
radio_stop (void)
{
	radio_mute();
	
	if (dev)
		dev->finalize(dev);
	if (audio_loopback) {
		if (audio_input) {
			free(audio_input);
			audio_input = NULL;
		}
		if (audio_output) {
			free(audio_output);
			audio_output = NULL;
		}
	}
}

void
radio_set_freq (float frequency)
{
	if (dev)
		dev->set_freq(dev, frequency);
}

void
radio_unmute (void)
{
	if (dev)
		dev->mute(dev, 0);
	if (audio_loopback)
		alsa_thread_startup(audio_output,
                                    audio_input,
                                    audio_latency,
                                    stderr, verbose);
}

void
radio_mute (void)
{
	if (dev)
		dev->mute (dev, 1);
	if (audio_loopback)
		alsa_thread_stop();
}

int
radio_is_muted (void)
{
	if (dev)
		return dev->is_muted(dev);
	else
		return -1;
}

int
radio_get_stereo (void)
{
	if (dev)
		return dev->get_stereo(dev);
	else
		return -1;
}

int
radio_get_signal (void)
{
	if (dev)
		return dev->get_signal(dev);
	else
		return -1;
}

char *
radio_get_name (char       *device,
                DriverType  driver)
{
	switch (driver) {
		case DRIVER_ANY:
		case DRIVER_V4L2:
		default:
			goto try_v4l2;
		case DRIVER_V4L1:
			goto try_v4l1;
	}

 try_v4l1:
	dev = v4l1_radio_dev_new();
	if (dev)
		return dev->get_name(dev, device);
	else
		return NULL;

 try_v4l2:
	dev = v4l2_radio_dev_new();
	if (dev)
		return dev->get_name(dev, device);
	else
		return NULL;
}

int
radio_check_station (float freq)
{
	static int a, b;
	static float last;
	int signal;
	
	signal = radio_get_signal();
	
	if (last == 0.0f)
		last = freq;
	
	if ((a + b + signal > 8) && (fabsf(freq - last) > 0.25f)) {
		a = b = 0;
		last = freq;
		return 1;
	}
	a = b;
	b = signal;

	return 0;
}

