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
#include <sys/types.h>
#include <string.h>
#include <math.h>
#include <alsa/asoundlib.h>

#include "mixer.h"

static const char alsa_core_devnames[] = "default";
static int muted = 0;
static int mutecount = 0;
static snd_mixer_t *handle = NULL;
static snd_mixer_elem_t *elem = NULL;
static snd_mixer_selem_id_t *sid = NULL;
static int err = 0;

static long alsa_min, alsa_max, alsa_vol;

static void alsa_open_mixer (const char *card, const char *channel)
{
	if ((err = snd_mixer_open (&handle, 0)) < 0) {
		fprintf (stderr, "mixer: open error: %s\n", snd_strerror (err));
		return;
	}
	if ((err = snd_mixer_attach (handle, card)) < 0) {
		fprintf (stderr, "mixer: attach error: %s\n", snd_strerror (err));
		goto error;
	}
	if ((err = snd_mixer_selem_register (handle, NULL, NULL)) < 0) {
		fprintf (stderr, "mixer: register error: %s\n", snd_strerror (err));
		goto error;
	}
	if ((err = snd_mixer_load (handle)) < 0) {
		fprintf (stderr, "mixer: load error: %s\n", snd_strerror (err));
		goto error;
	}

	snd_mixer_selem_id_malloc (&sid);
	if (sid == NULL)
		goto error;
	snd_mixer_selem_id_set_name (sid, channel);
	if ((elem = snd_mixer_find_selem (handle, sid)) == NULL) {
		fprintf (stderr, "mixer: find error: %s\n", snd_strerror (err));
		goto error;
	}
	if (!snd_mixer_selem_has_playback_volume (elem)) {
		fprintf (stderr, "mixer: no playback\n");
		goto error;
	}
	snd_mixer_selem_get_playback_volume_range (elem, &alsa_min, &alsa_max);
	if ((alsa_max - alsa_min) <= 0) {
		fprintf (stderr, "mixer: no valid playback range\n");
		goto error;
	}

	return;

error:
	if (elem)
		elem = NULL;
	if (sid) {
		snd_mixer_selem_id_free (sid);
		sid = NULL;
	}
	if (handle) {
		snd_mixer_close (handle);
		handle = NULL;
	}

	return;
}

static char **alsa_get_channel (void)
{
	snd_mixer_elem_t *active = NULL;

	int count, i;
	char **result;

	if ((handle == NULL) || (elem == NULL))
		return NULL;

	count = snd_mixer_get_count (handle);
	result = (char **) malloc(sizeof(char *) * count);

	active = elem;
	i = 0;
	for (elem = snd_mixer_first_elem (handle); elem != NULL; elem = snd_mixer_elem_next (elem)) {
		if (!snd_mixer_selem_is_active (elem))
			continue;
		if (snd_mixer_selem_has_playback_volume (elem) &&
                    snd_mixer_selem_has_playback_switch (elem) &&
                    snd_mixer_selem_has_capture_switch (elem)) {
			result[i] = strdup(snd_mixer_selem_get_name (elem));
			i++;
		}
		result[i] = NULL;
	}
	elem = active;

	return result;
}

/* Volume saved to file */
static int alsa_get_unmute_volume (void)
{
	long volume;

	assert (elem);

	if (snd_mixer_selem_is_playback_mono (elem)) {
		snd_mixer_selem_get_playback_volume (elem, SND_MIXER_SCHN_MONO, &volume);
		return volume;
	} else {
		int c, n = 0;
		long sum = 0;
		for (c = 0; c <= SND_MIXER_SCHN_LAST; c++) {
			if (snd_mixer_selem_has_playback_channel (elem, c)) {
				snd_mixer_selem_get_playback_volume (elem, SND_MIXER_SCHN_FRONT_LEFT, &volume);
				sum += volume;
				n++;
			}
		}
		if (!n)
			return 0;

		volume = sum / n;
		sum = (long)((double)((alsa_vol / 100.0) * (alsa_max - alsa_min) + alsa_min));

		if (sum != volume)
			alsa_vol = (long)( 100. * (volume - alsa_min) / (alsa_max - alsa_min));

		return alsa_vol;
	}
}

static int alsa_get_volume (void)
{
	if (muted)
		return 0;
	else
		return alsa_get_unmute_volume();
}

static void alsa_set_volume (int value)
{
	long volume;

	assert (elem);

	value = CLAMP (value, 0, 100);
	volume = (long)((value / 100.) * (alsa_max - alsa_min) + alsa_min);

	snd_mixer_selem_set_playback_volume_all (elem, volume);
	snd_mixer_selem_set_playback_switch_all (elem, 1);
	muted = 0;
	mutecount = 0;
}

static void alsa_mute (int mute)
{
	/**
	 * Make sure that if multiple users mute the card,
	 * we only honour the last one.
	 */
	if (!mute && mutecount)
	if (mutecount)
		return;

	if (mute) {
		mutecount++;
		muted = 1;
		if (snd_mixer_selem_has_playback_switch (elem))
			snd_mixer_selem_set_playback_switch_all (elem, 0);
		else
			fprintf (stderr, "mixer: mute not implemented\n");
	} else {
		muted = 0;
		if (snd_mixer_selem_has_playback_switch (elem))
			snd_mixer_selem_set_playback_switch_all (elem, 1);
		else
			fprintf (stderr, "mixer: mute not implemented\n");
	}
}

static int alsa_is_muted (void)
{
	return muted;
}

static int alsa_set_device (const char *card, const char* channel)
{
	alsa_open_mixer (card, channel);

	if (!handle) {
		fprintf (stderr, "mixer: Can't open mixer %s, "
                         "mixer volume and mute unavailable.\n", card);
		return 0;
	}

	return 1;
}

static int alsa_set_record_device (void)
{
	snd_mixer_elem_t *active = NULL;

	if ((handle == NULL) || (elem == NULL))
		return 0;

	if (snd_mixer_selem_has_capture_switch (elem)) {
	    if (snd_mixer_selem_set_capture_switch_all (elem, 1) < 0) {
        	fprintf(stderr, "mixer: capture switch error: %s\n", snd_strerror (err));
		return 0;
    	    }
        }

	active = elem;
	snd_mixer_selem_id_set_name (sid, "Capture");
	if ((elem = snd_mixer_find_selem (handle, sid)) == NULL) {
		fprintf (stderr, "mixer: find error: %s\n", snd_strerror (err));
	} else {
		if (snd_mixer_selem_set_capture_switch_all (elem, 1) < 0) {
			fprintf(stderr, "mixer: capture switch error: %s\n", snd_strerror (err));
			elem = active;
			return 0;
		}
        }
	elem = active;

	return 1;
}	

static void alsa_set_state (int is_muted, int unmute_volume)
{
	/**
	 * 1. we come back unmuted: Don't touch anything
	 * 2. we don't have a saved volume: Don't touch anything
	 * 3. we come back muted and we have a saved volume:
	 *    - if gnomeradio muted it, unmute to old volume
	 *    - if user did it, remember that we're muted and old volume
	 */
	if (alsa_get_volume () == 0 && unmute_volume > 0) {
		snd_mixer_selem_set_playback_volume_all (elem, unmute_volume);
		muted = 1;

		if (!is_muted)
			alsa_mute (0);
	}
}

static void alsa_close_device (void)
{
	elem = NULL;
	if (sid) {
		snd_mixer_selem_id_free (sid);
		sid = NULL;
	}
	if (handle) {
		snd_mixer_close (handle);
		handle = NULL;
	}
	muted = 0;
	mutecount = 0;
}

struct mixer alsa_mixer = {
	.set_device        = alsa_set_device,
	.get_channel       = alsa_get_channel,
	.set_state         = alsa_set_state,
	.get_volume        = alsa_get_volume,
	.get_unmute_volume = alsa_get_unmute_volume,
	.set_volume        = alsa_set_volume,
	.mute              = alsa_mute,
	.is_muted          = alsa_is_muted,
	.set_record_device = alsa_set_record_device,
	.close_device      = alsa_close_device,
};
