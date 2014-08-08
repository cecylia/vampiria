/*
 * Copyright © 2010  Jonathan Matthew <jonathan@d14n.org>
 * Copyright © 2014  POJAR GEORGE <geoubuntu@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or(at your option)
 * any later version.
 *
 * The Rhythmbox authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Rhythmbox. This permission is above and beyond the permissions granted
 * by the GPL license by which Rhythmbox is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MEDIA_TYPES_H
#define _MEDIA_TYPES_H

#include <gst/gst.h>
#include <gst/pbutils/encoding-target.h>
#include <gst/pbutils/encoding-profile.h>

G_BEGIN_DECLS

#define MEDIA_TYPE_MP3		"audio/mpeg"
#define MEDIA_TYPE_OGG_VORBIS 	"audio/x-vorbis"
#define MEDIA_TYPE_FLAC		"audio/x-flac"
#define MEDIA_TYPE_AAC 		"audio/x-aac"

typedef enum {
	MEDIA_TYPE_NONE = 0,
	MEDIA_TYPE_CONTAINER,
	MEDIA_TYPE_AUDIO,
	MEDIA_TYPE_VIDEO,
	MEDIA_TYPE_OTHER
} GstMediaType;

char *caps_to_media_type(const GstCaps *caps);

GstCaps *media_type_to_caps(const char *media_type);

const char *media_type_to_extension(const char *media_type);

const char *mime_type_to_media_type(const char *mime_type);

const char *media_type_to_mime_type(const char *media_type);

GstEncodingTarget *get_default_encoding_target(void);

GstEncodingProfile *get_encoding_profile(const char *media_type);

gboolean media_type_matches_profile(GstEncodingProfile *profile,
                                    const char *media_type);

char *encoding_profile_get_media_type(GstEncodingProfile *profile);

gboolean media_type_is_lossless(const char *media_type);

gboolean check_missing_plugins(GstEncodingProfile *profile,
                               char ***details,
                               char ***descriptions);

G_END_DECLS

#endif
