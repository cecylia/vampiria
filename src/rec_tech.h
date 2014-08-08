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

#ifndef _REC_TECH_H
#define _REC_TECH_H

#include <gio/gio.h>
#include <gst/gst.h>

typedef struct Recording_Settings recording_settings;
struct Recording_Settings
{
	gchar *profile;
	gchar *destination;
};

recording_settings rec_settings;

typedef struct
{
	GstElement* pipeline;
	GFile *file;
	char *station;
} Recording;

Recording* recording_start (const char *filename);

void recording_stop (Recording *recording);

#endif
