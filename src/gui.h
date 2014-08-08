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

#ifndef _GUI_H
#define _GUI_H

#define FREQ_MAX 108
#define FREQ_MIN 87.5
#define STEPS 20
#define SUNSHINE 106.15

typedef struct Gnomeradio_Settings gnomeradio_settings;
typedef struct Device device;
typedef struct Preset preset;

struct Gnomeradio_Settings
{
	gchar    *driver;
	gchar    *device;
	gchar    *mixer_device;
	gchar    *mixer_channel;
	gboolean  muted;
	gboolean  mute_on_exit;
	gboolean  audio_loopback;
	gfloat    unmute_volume;

	GList    *devices;	
	GList    *presets;
};

struct Device
{
	gchar    *device;
	gchar    *name;
	gboolean  audio;
};

struct Preset
{
	gchar  *title;
	gfloat  freq;
};

GList *
get_device_list (void);

GList *
get_sound_card_list (void);

GList *
get_mixer_list (void);

void
start_radio (gboolean restart, GtkWidget *app);

void
start_mixer (gboolean   restart,
             GtkWidget *app);

void
exit_gnomeradio (void);

void
scfw_button_clicked_cb (GtkButton *button,
                        gpointer   data);

void
scbw_button_clicked_cb (GtkButton *button,
                        gpointer   data);

void
rec_button_clicked_cb (GtkButton *button,
                       gpointer   app);

void
volume_up (void);

void
volume_down (void);

void
toggle_volume(void);

void
toggle_mainwindow_visibility (GtkWidget *app);

void
preset_combo_set_item (gint index);

void
preset_menuitem_activate_cb (GtkMenuItem *menuitem,
                             gpointer     user_data);

gboolean
key_press_event_cb (GtkWidget *app,
                    GdkEventKey *event,
                    gpointer data);

void
recording_set_sensible (gboolean sensible);

void
display_help_cb (char *topic);

void
change_preset (gboolean next);

void
switch_to_preset (gint index);

#endif
