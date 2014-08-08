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

#ifndef _RADIO_H
#define _RADIO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _RadioDev RadioDev;
struct _RadioDev
{
	char * (*get_name)    (RadioDev *dev, char *device);
	int    (* init)       (RadioDev *dev, char *device);
	int    (* is_init)    (RadioDev *dev);
	void   (* set_freq)   (RadioDev *dev, float freq);
	void   (* mute)       (RadioDev *dev, int   mute);
	int    (* is_muted)   (RadioDev *dev);
	int    (* get_stereo) (RadioDev *dev);
	int    (* get_signal) (RadioDev *dev);
	void   (* finalize)   (RadioDev *dev);
};

typedef enum _DriverType DriverType;
enum _DriverType
{
	DRIVER_ANY,
	DRIVER_V4L1,
	DRIVER_V4L2
};

char *
radio_get_name (char       *device,
                DriverType  type);

int
radio_init (char       *device,
            DriverType  type);

int
radio_is_init (void);

void
radio_stop (void);

void
radio_set_freq (float freq);

int
radio_check_station (float freq);

void
radio_unmute (void);

void
radio_mute (void);

int
radio_is_muted (void);

int
radio_get_stereo (void);

int
radio_get_signal (void);

#ifdef __cplusplus
}
#endif

#endif /* _RADIO_H */
