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

#ifndef MIXER_H_INCLUDED
#define MIXER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

int mixer_set_device (const char *card, const char* channel);

struct mixer
{
	int     (* set_device)        (const char *card, const char* channel);
	char ** (*get_channel)        (void);
	void    (* set_state)         (int is_muted, int unmute_volume);
	int     (* get_volume)        (void);
	int     (* get_unmute_volume) (void);
	void    (* set_volume)        (int value);
	void    (* mute)              (int mute);
	int     (* is_muted)          (void);
	int     (* set_record_device) (void);
	void    (* close_device)      (void);
};

#pragma weak alsa_mixer
extern struct mixer alsa_mixer;
#pragma weak oss_mixer
extern struct mixer oss_mixer;
extern struct mixer *mixer;

#ifdef __cplusplus
};
#endif
#endif /* MIXER_H_INCLUDED */
