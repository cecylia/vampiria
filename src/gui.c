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

#include <config.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>

#include "gui.h"
#include "messages.h"
#include "trayicon.h"
#include "mixer.h"
#include "radio.h"
#include "rec_tech.h"
#include "lirc.h"
#include "prefs.h"
#include "record.h"
#include "media_types.h"
#include "missing_plugins.h"
#include "media_devices.h"

#include "../data/pixmaps/digits.xpm"
#include "../data/pixmaps/signal.xpm"
#include "../data/pixmaps/stereo.xpm"
#include "../data/pixmaps/freq_up.xpm"
#include "../data/pixmaps/freq_down.xpm"

#define DIGIT_WIDTH 20
#define DIGIT_HEIGTH 30
#define SIGNAL_WIDTH 25
#define STEREO_WIDTH 35
#define SCAN_SPEED 20

#define VOLUME_STEP 5

#define TRANSLATORS "TRANSLATORS"

extern int verbose;

gnomeradio_settings settings;
GSettings *gsettings;

int audio_loopback;
char *audio_output = NULL;
char *audio_input = NULL;
int audio_latency = 50;

int mom_ps;

gboolean main_visible;

GtkWidget *app;
GtkWidget *preset_combo;
GtkAdjustment *adj;
GtkWidget *volume_button;

static GtkWidget *drawing_area;
static GdkPixbuf *digits;
static GdkPixbuf *signal_s;
static GdkPixbuf *stereo;
static GtkWidget *freq_scale;
static GtkWidget *scbw_button;
static GtkWidget *stbw_button;
static GtkWidget *stfw_button;
static GtkWidget *scfw_button;
static GtkWidget *rec_button;
static GtkWidget *prefs_button;

static int timeout_id;
static int bp_timeout_id = -1;
static int bp_timeout_steps = 0;
static int volume_value_changed_id;

static gboolean do_scan = FALSE;
static gboolean dialog_visible;

static GDBusProxy *proxy;

static void
session_die_cb (GDBusProxy *proxy,
                gchar      *sender_name,
                gchar      *signal_name,
                GVariant   *parameters,
                gpointer    user_data)
{
	if (g_strcmp0 (signal_name, "SessionOver") == 0) {
		if (settings.mute_on_exit)
			radio_stop ();

		mixer->close_device ();
		gtk_widget_destroy (GTK_WIDGET (app));
		exit (0);
	}
}

static void
connect_to_session (void)
{
	GDBusConnection *connection;
	GError *error = NULL;

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (connection == NULL) {
		g_warning ("Could not connect to system bus: %s",
                           error->message);
		g_error_free (error);
		return;
	}

	proxy = g_dbus_proxy_new_sync (connection,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       NULL,
                                       "org.gnome.SessionManager",
                                       "/org/gnome/SessionManager",
                                       "org.gnome.SessionManager",
                                       NULL,
                                       &error);
	g_object_unref (connection);

	if (proxy == NULL) {
                g_warning ("Could not connect to the Session manager: %s",
                           error->message);
                g_error_free (error);
                return;
	}

	g_signal_connect (proxy, "g-signal", G_CALLBACK (session_die_cb), NULL);
}

typedef struct {
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *progress;
	GList     *stations;
} FreqScanData;

static gboolean
scan_cb (gpointer data)
{
	static gfloat  freq = FREQ_MIN - 4.0f/STEPS;
	FreqScanData  *fsd = data;
	
	g_assert (fsd);
	
	if (freq > FREQ_MAX) {
		gtk_widget_destroy (fsd->dialog);
		timeout_id = 0;
		return FALSE;
	}
	
	if (radio_check_station (freq)) {
		guint   num;
		char   *text;
		gfloat *f;

		num = g_list_length (fsd->stations) + 1;
		text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
					"%d station found",
					"%d stations found", num), num);
		gtk_label_set_text (GTK_LABEL (fsd->label), text);
		g_free (text);
		
		g_print ("Found a station at %.2f MHz\n", freq);

		f = g_malloc (sizeof (gfloat));
		
		*f = freq;
		fsd->stations = g_list_append (fsd->stations, f);
	}

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (fsd->progress),
				       MAX (0, (freq - FREQ_MIN)/ (FREQ_MAX - FREQ_MIN)));	
	
	freq += 1.0/STEPS;
	radio_set_freq (freq);
	
	return TRUE;
}

static void
scan (GtkWidget *app)
{
	FreqScanData  data;
	GtkWidget    *content_area;
	GtkWidget    *box;
	GtkWidget    *label;
	gchar        *markup;
	
	data.stations = NULL;
	
	data.dialog = gtk_dialog_new_with_buttons (_("Scanning"),
						   GTK_WINDOW (app),
						   DIALOG_FLAGS,
						   _("_Cancel"), GTK_RESPONSE_CANCEL,
						   NULL);
	gtk_window_set_resizable (GTK_WINDOW (data.dialog), FALSE);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (data.dialog));

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_set_border_width (GTK_CONTAINER (box), 8);
	
	label = gtk_label_new (NULL);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	markup = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>",
				  _("Scanning for available stations"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	data.progress = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (box), data.progress, FALSE, FALSE, 0);

	data.label = gtk_label_new (_("No stations found"));
	gtk_widget_set_halign (data.label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (box), data.label, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (content_area), box);
	gtk_widget_show_all (data.dialog);
	
	radio_mute ();
	timeout_id = g_timeout_add (1000/SCAN_SPEED,
				    (GSourceFunc)scan_cb,
				    (gpointer)&data);
	gtk_dialog_run (GTK_DIALOG (data.dialog));

	radio_unmute ();
	if (timeout_id) {
		g_source_remove (timeout_id);
		timeout_id = 0;
		gtk_widget_destroy (data.dialog);
	} else {
		if (data.stations != NULL) {
			GtkWidget *dialog;
			gfloat     freq;
			guint      num;
			GList     *l;
			char      *text;
			int        response;

			freq = * ((gfloat *) data.stations->data);
			gtk_adjustment_set_value (adj, freq*STEPS);
			radio_set_freq (freq);
			
			num = g_list_length (data.stations);
			text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                                "%d station found.",
                                                "%d stations found.", num), num);

			dialog = gtk_message_dialog_new (GTK_WINDOW (app),
                                                         DIALOG_FLAGS,
                                                         GTK_MESSAGE_QUESTION,
                                                         GTK_BUTTONS_YES_NO,
                                                         "%s", text);

			if (num == 1)
				gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                                          _("Do you want to add it as preset?"));
			else
				gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                                          _("Do you want to add them as presets?"));
			g_free (text);

			response = gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			for (l = data.stations; l != NULL; l = l->next) {
				if (response == GTK_RESPONSE_YES) {
					preset *ps;

					ps = g_malloc0 (sizeof (preset));
					ps->title = g_strdup (_("unnamed"));
					ps->freq = * ((gfloat *) l->data);
					settings.presets = g_list_append (settings.presets, ps);
				}
				g_free (l->data);
			}
		}
	}	
}

static gboolean
is_first_run (void)
{
	return g_settings_get_boolean (gsettings, "first-run");
}

static void
set_first_run_flag (void)
{
	g_settings_set_boolean (gsettings, "first-run", FALSE);
}

static gdouble
get_volume (void)
{
	return gtk_scale_button_get_value (GTK_SCALE_BUTTON (volume_button)) * 100;
}

static void
set_volume (gdouble volume)
{
	mixer->set_volume (volume);
	g_signal_handler_block (volume_button, volume_value_changed_id);
	gtk_scale_button_set_value (GTK_SCALE_BUTTON (volume_button), volume / 100);
	g_signal_handler_unblock (volume_button, volume_value_changed_id);

	tray_menu_enable_mute_button (volume == 0);

	if (radio_is_muted ())
		radio_unmute ();

	settings.unmute_volume = volume / 100;
}

void
volume_up (void)
{
	gdouble volume;

	volume = get_volume ();
	set_volume (volume > 95 ? 100 : volume + VOLUME_STEP);
}

void
volume_down (void)
{
	gdouble volume;

	volume = get_volume ();
	set_volume (volume < 5 ? 0 : volume - VOLUME_STEP);
}

static void
volume_value_changed_cb (GtkVolumeButton *button,
                         gpointer         user_data)
{
	set_volume (gtk_scale_button_get_value (GTK_SCALE_BUTTON (volume_button)) * 100);
}

void
toggle_volume (void)
{
	gdouble volume;

	if (radio_is_muted ())
		radio_unmute ();
	else
		radio_mute ();

	if (mixer->is_muted ()) {
		mixer->mute (0);
		settings.muted = FALSE;
	} else {
		mixer->mute (1);
		settings.muted = TRUE;
	}

	volume = (gdouble) mixer->get_volume ();
	g_signal_handler_block (volume_button, volume_value_changed_id);
	gtk_scale_button_set_value (GTK_SCALE_BUTTON (volume_button), volume / 100);
	g_signal_handler_unblock (volume_button, volume_value_changed_id);

	tray_menu_enable_mute_button (volume == 0);
}

static void prefs_button_clicked_cb (GtkButton *button,
                                     gpointer   app)
{
	GtkWidget *dialog;
	int choise;

	if (dialog_visible)
		return;

	dialog_visible = TRUE;
	dialog = prefs_window (app);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (app));
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

	choise = gtk_dialog_run (GTK_DIALOG (dialog));
	switch (choise) {
		case GTK_RESPONSE_HELP:
			display_help_cb ("gnomeradio-settings");
			break;
		default:
			gtk_widget_destroy (dialog);
	}

	dialog_visible = FALSE;
}

void
start_radio (gboolean   restart,
             GtkWidget *app)
{
	DriverType driver = DRIVER_ANY;

	if (restart)
		radio_stop ();

	if (settings.driver) {
		if (g_strcmp0 (settings.driver, "v4l1") == 0)
			driver = DRIVER_V4L1;
		if (g_strcmp0 (settings.driver, "v4l2") == 0)
			driver = DRIVER_V4L2;
	}

	if (settings.audio_loopback)
		audio_loopback = 1;
	else
		audio_loopback = 0;

	if (!radio_init (settings.device, driver)) {
		if (settings.devices) {
			gchar *caption;

			caption = g_strdup_printf (_("Cannot not open radio device \"%s\"!"),
                                                   settings.device);
			show_error_message (caption,
                                            _("Check settings and make sure that no other program is using it. Make also sure "
                                              "that have read access to it."));
			g_free (caption);
		} else {
			show_error_message (_("Could not find an radio device!"),
                                            _("To listen to the FM radio, you will need an FM tuner connected to or installed "
                                              "in the computer, and an antenna connected to the tuner to receive a signal."));
		}
	}
}

void
start_mixer (gboolean   restart,
             GtkWidget *app)
{
	if (restart)
		mixer->close_device ();

	if (!mixer_set_device (settings.mixer_device, settings.mixer_channel)) {
		gchar *caption;

		caption = g_strdup_printf (_("Cannot load mixer channel. Invalid argument \"%s\"!"),
                                           settings.mixer_channel);
		show_error_message (caption,
                                    _("You can use alsamixer program to show all channels associated with sound card."));
		g_free (caption);

		gtk_widget_set_sensitive (volume_button, FALSE);
	} else {
		mixer->set_state (settings.muted, settings.unmute_volume);
		g_signal_handler_block (volume_button, volume_value_changed_id);
		gtk_scale_button_set_value (GTK_SCALE_BUTTON (volume_button), (gdouble) mixer->get_volume () / 100);
		g_signal_handler_unblock (volume_button, volume_value_changed_id);

		gtk_widget_set_sensitive (volume_button, TRUE);
	}
}

GList *
get_device_list (void)
{
	void       *md;
	const char *p = NULL;
	GList      *result = NULL;
	
	md = discover_media_devices ();
	do {
		p = get_associated_device (md, p, MEDIA_V4L_RADIO, NULL, NONE);
		if (!p)
			break;
		char *device;

		device = g_strdup_printf ("/dev/%s", p);
		result = g_list_append (result, device);
	} while (p);

	free_media_devices (md);
	
	return result;
}

GList *
get_sound_card_list (void)
{
	void       *md;
	const char *p = NULL;
	GList      *result = NULL;
	
	md = discover_media_devices ();
	do {
		p = get_not_associated_device (md, p, MEDIA_SND_CARD, MEDIA_V4L_RADIO);
		if (p) {
			char *device;

			device = g_strdup (p);
			result = g_list_append (result, device);
		}
	} while (p);

	free_media_devices (md);
	
	return result;
}

GList *
get_mixer_list (void)
{
	int   i;
	char  **array, *device;
	GList *result = NULL;
	
	array = mixer->get_channel ();
	if (!array)
		return NULL;
	
	i = 0;	
	device = array[i];
	while (device) {
		char *channel;

		channel = g_strdup (device);
		result = g_list_append (result, channel);
		free (device);
		device = array[++i];
	}			
	free (array);
	
	return result;
}

static gboolean
redraw_status_window (void)
{
	GdkWindow *window;
	cairo_t   *cr;
	int        width, val, freq[5], signal_strength, is_stereo;
	
	val = (int) (rint (gtk_adjustment_get_value (adj) / STEPS * 100.0));
	
	freq[0] = val / 10000;
	freq[1] = (val % 10000) / 1000;
	freq[2] = (val % 1000) / 100; 
	freq[3] = (val % 100) / 10;
	freq[4] = val % 10;

	signal_strength = radio_get_signal ();
	is_stereo = radio_get_stereo ();
	
	if (signal_strength > 3) signal_strength = 3;
	if (signal_strength < 0) signal_strength = 0;
	is_stereo = (is_stereo == 1) ? 1 : 0;
	
	window = gtk_widget_get_window (drawing_area);
	if (window == NULL)
		/* UI has not been realized yet */
		return TRUE;

	width = gdk_window_get_width (window);
	
	cr = gdk_cairo_create (window);
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);

	cairo_paint (cr);
	
	width -= 5;
	
	if (freq[0]) {
		gdk_cairo_set_source_pixbuf (cr, digits, width - DIGIT_WIDTH * 6 - freq[0] * DIGIT_WIDTH, 5);
		cairo_rectangle (cr, width - DIGIT_WIDTH * 6, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
		cairo_fill (cr);
	} else {
		cairo_rectangle (cr, width - DIGIT_WIDTH * 6, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
		cairo_fill (cr);
	}

	gdk_cairo_set_source_pixbuf (cr, digits, width - DIGIT_WIDTH * 5 - freq[1] * DIGIT_WIDTH, 5);
	cairo_rectangle (cr, width - DIGIT_WIDTH * 5, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
	cairo_fill (cr);
	gdk_cairo_set_source_pixbuf (cr, digits, width - DIGIT_WIDTH * 4 - freq[2] * DIGIT_WIDTH, 5);
	cairo_rectangle (cr, width - DIGIT_WIDTH * 4, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
	cairo_fill (cr);
	gdk_cairo_set_source_pixbuf (cr, digits, width - DIGIT_WIDTH * 3 - 10 * DIGIT_WIDTH, 5);
	cairo_rectangle (cr, width - DIGIT_WIDTH * 3, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
	cairo_fill (cr);
	gdk_cairo_set_source_pixbuf (cr, digits, width - DIGIT_WIDTH * 2 - freq[3] * DIGIT_WIDTH, 5);
	cairo_rectangle (cr, width - DIGIT_WIDTH * 2, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
	cairo_fill (cr);
	gdk_cairo_set_source_pixbuf (cr, digits, width - DIGIT_WIDTH * 1 - freq[4] * DIGIT_WIDTH, 5);
	cairo_rectangle (cr, width - DIGIT_WIDTH * 1, 5, DIGIT_WIDTH, DIGIT_HEIGTH);
	cairo_fill (cr);
	gdk_cairo_set_source_pixbuf (cr, signal_s, width - DIGIT_WIDTH * 6 - SIGNAL_WIDTH - signal_strength * SIGNAL_WIDTH, 5);
	cairo_rectangle (cr, width - DIGIT_WIDTH * 6 - SIGNAL_WIDTH, 5, SIGNAL_WIDTH, DIGIT_HEIGTH);
	cairo_fill (cr);
	gdk_cairo_set_source_pixbuf (cr, stereo,
				     width - DIGIT_WIDTH * 6 - SIGNAL_WIDTH - STEREO_WIDTH - is_stereo * STEREO_WIDTH, 5);
	cairo_rectangle (cr, width - DIGIT_WIDTH * 6 - SIGNAL_WIDTH - STEREO_WIDTH, 5, STEREO_WIDTH, DIGIT_HEIGTH);
	cairo_fill (cr);

	cairo_destroy (cr);

	return TRUE;	
}

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   data)
{
	redraw_status_window ();

	return TRUE;
}

void
exit_gnomeradio (void)
{
	if (settings.mute_on_exit)
		radio_stop ();

	mixer->close_device ();
	save_settings ();
	gtk_widget_destroy (GTK_WIDGET (app));
}

const char *
get_preset (float  freq,
            int   *num)
{
	GList *node = settings.presets;

	int i = *num = -1;
	for (;node;) {
		++i;
		preset *ps = (preset *)node->data;
		if (fabs (ps->freq - freq) < 0.01) {
			*num = i;
			return ps->title;
		}
		node = node->next;
	}

	return NULL;
}

static void
adj_value_changed_cb (GtkAdjustment *data,
                      gpointer       app)
{
	float       freq;
	const char *preset_title;
	char       *buffer;

	preset_combo_set_item (mom_ps);
	redraw_status_window ();

	freq = rint (gtk_adjustment_get_value (adj))/STEPS;
	preset_title = get_preset (freq, &mom_ps);
	
	if (preset_title)
		buffer = g_strdup_printf (_("Gnomeradio - %s"), preset_title);
	else
		buffer = g_strdup_printf (_("Gnomeradio - %.2f MHz"), freq);

	gtk_window_set_title (GTK_WINDOW (app), buffer);
	tray_icon_set_title (buffer);
	g_free (buffer);
	
	buffer = g_strdup_printf (_("Frequency: %.2f MHz"), freq);
	gtk_widget_set_tooltip_text (freq_scale, buffer);	
	g_free (buffer);

	radio_set_freq (gtk_adjustment_get_value (adj)/STEPS);
}

static void
change_freq (gpointer data)
{
	gboolean increase;
	gint     value;

	increase = (gboolean) GPOINTER_TO_INT (data);
	value = gtk_adjustment_get_value (adj);
	
	if (increase) {
		if (value >= FREQ_MAX*STEPS)
			gtk_adjustment_set_value (adj, FREQ_MIN*STEPS);
		else
			gtk_adjustment_set_value (adj, value+1);
	} else {
		if (value <= FREQ_MIN*STEPS)
			gtk_adjustment_set_value (adj, FREQ_MAX*STEPS);
		else
			gtk_adjustment_set_value (adj, value-1);
	}
}

static gboolean
change_freq_timeout (gpointer data)
{
	change_freq (data);

	if (bp_timeout_steps < 10) {
		g_source_remove (bp_timeout_id);
		bp_timeout_id = g_timeout_add (200 - 20*bp_timeout_steps,
                                               (GSourceFunc) change_freq_timeout,
                                               data);
		bp_timeout_steps++;
	}

	return TRUE;
}	

static void
step_button_pressed_cb (GtkButton *button,
                        gpointer   data)
{
	bp_timeout_id = g_timeout_add (500,
                                       (GSourceFunc) change_freq_timeout,
                                       data);
}

static void
step_button_clicked_cb (GtkButton *button,
                        gpointer   data)
{
	change_freq (data);
}

static void
step_button_released_cb (GtkButton *button,
                         gpointer   data)
{
	if (bp_timeout_id > -1) {
		g_source_remove (bp_timeout_id);
		bp_timeout_id = -1;
		bp_timeout_steps = 0;
	}
}

static gboolean
scan_freq (gpointer data)
{
	static gint start, max, mom;
	gint        direction;

	direction = GPOINTER_TO_INT (data);
	
	if (!max)
		max = (FREQ_MAX - FREQ_MIN) * STEPS;
		
	if (radio_check_station (gtk_adjustment_get_value (adj)/STEPS) ||
            (start > max)) {
		start = mom = 0;
		radio_unmute ();
		timeout_id = 0;
		return FALSE;
	}

	if (!mom)
		mom = gtk_adjustment_get_value (adj);
		
	if (mom > FREQ_MAX*STEPS) 
		mom = FREQ_MIN*STEPS;
	else if (mom < FREQ_MIN*STEPS)
		mom = FREQ_MAX*STEPS;
	else	
		mom = mom + direction;
	start += 1;
	gtk_adjustment_set_value (adj, mom);

	return TRUE;
}

void
scfw_button_clicked_cb (GtkButton *button,
                        gpointer   data)
{
	if (timeout_id) {
		g_source_remove (timeout_id);
		timeout_id = 0;
		radio_unmute ();
		return;
	}

	radio_mute ();
	timeout_id = g_timeout_add (1000/SCAN_SPEED,
                                    (GSourceFunc) scan_freq,
                                    (gpointer) 1);
}

void
scbw_button_clicked_cb (GtkButton *button,
                        gpointer   data)
{
	if (timeout_id) {
		g_source_remove (timeout_id);
		timeout_id = 0;
		radio_unmute ();
		return;
	}

	radio_mute ();
	timeout_id = g_timeout_add (1000/SCAN_SPEED,
                                    (GSourceFunc) scan_freq,
                                    (gpointer) -1);
}

void
preset_combo_set_item (gint index)
{
	if (index < -1)
		return;
	if (preset_combo == NULL)
		return;

	gtk_combo_box_set_active (GTK_COMBO_BOX (preset_combo), index + 1);
}

static void
preset_combo_change_cb (GtkWidget *combo,
                        gpointer   data)
{
	preset *ps;

	mom_ps = gtk_combo_box_get_active (GTK_COMBO_BOX (combo)) - 1;
	
	if (mom_ps < 0)
		return;
	
	ps = (preset *) g_list_nth_data (settings.presets, mom_ps);
	gtk_adjustment_set_value (adj, ps->freq * STEPS);

	if (dialog_visible)
		treeview_switch_to_preset (mom_ps);
}

void
change_preset (gboolean next)
{
	preset *ps;
	int     len;

	len = g_list_length (settings.presets);
	if (len < 1)
		return;

	if (next)
		mom_ps = (mom_ps + 1) % len;
	else
		mom_ps = (mom_ps - 1 + len) % len;

	ps = g_list_nth_data (settings.presets, mom_ps);
	gtk_adjustment_set_value (adj, ps->freq*STEPS);
	preset_combo_set_item (mom_ps);

	if (dialog_visible)
		treeview_switch_to_preset (mom_ps);
}

void
switch_to_preset (gint index)
{
	if (0 <= index && index < g_list_length (settings.presets)) {
		preset *ps;

		ps = g_list_nth_data (settings.presets, index);
		gtk_adjustment_set_value (adj, ps->freq*STEPS);
		mom_ps = index;
		preset_combo_set_item (mom_ps);

		if (dialog_visible)
			treeview_switch_to_preset (mom_ps);
	}
}

static void
quit_button_clicked_cb (GtkButton *button,
                        gpointer   data)
{
	exit_gnomeradio ();
}

void
recording_set_sensible (gboolean sensible)
{
	gtk_widget_set_sensitive (preset_combo, sensible);
	gtk_widget_set_sensitive (freq_scale, sensible);
	gtk_widget_set_sensitive (scfw_button, sensible);
	gtk_widget_set_sensitive (scbw_button, sensible);
	gtk_widget_set_sensitive (stfw_button, sensible);
	gtk_widget_set_sensitive (stbw_button, sensible);
	gtk_widget_set_sensitive (rec_button, sensible);
	gtk_widget_set_sensitive (prefs_button, sensible);
}

static int
start_recording (const gchar *destination,
                 const char  *station,
                 const char  *time)
{
	Recording *recording;
	GFile     *file;
	GFileInfo *info;
	gchar     *caption;

	if (!mixer->set_record_device ()) {
		gchar *caption;

		caption = g_strdup_printf (_("Could not set \"%s\" as recording source!"),
                                           settings.mixer_channel);
		show_error_message (caption,
                                    _("You will not able to record radio from this source."));
		g_free (caption);
		return -1;
	}

	file = g_file_new_for_path (rec_settings.destination);
	info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                                  G_FILE_QUERY_INFO_NONE,
                                  NULL,
                                  NULL);

	if (info != NULL) {
		gboolean can_write;

		can_write = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
		if (!can_write) {
			caption = g_strdup_printf (_("Could not write file to \"%s\" location!"),
                                                   rec_settings.destination);
			show_error_message (caption,
                                           _("Check your settings and make sure that you have write access to it."));
			g_free (caption);

			g_object_unref (info);
			g_object_unref (file);

			return -1;
		}
		g_object_unref (info);
	}
	g_object_unref (file);

	if (check_missing_plugins (get_encoding_profile (rec_settings.profile),
                                   NULL,
                                   NULL)) {
		gchar *extension, *format;

		extension = g_utf8_strup (media_type_to_extension (rec_settings.profile), -1);
		format = g_strdup_printf ("%s", extension);
		caption = g_strdup_printf (_("Could not write file in \"%s\" format!"), format);
		show_error_message (caption, _("You need to install additional software required to use this format."));

		g_free (extension);
		g_free (format);
		g_free (caption);

		return -1;
	}

	/* You can translate the filename for a recording:
	 * args for this format are: path, station title, time
	 */
	char *filename;

	filename = g_strdup_printf (_("%s/%s_%s"), destination, station, time);
	recording = recording_start (filename);
	g_free (filename);

	if (!recording)
		return -1;

	tray_menu_items_set_sensible (FALSE);
	recording_set_sensible (FALSE);

	recording->station = g_strdup (station);
	record_status_window (recording);

	run_status_window (recording);

	return 1;
}

void
record_button_clicked_cb (GtkButton *button,
                          gpointer   app)
{
	GDateTime *date;
	char      *station;
	gchar     *time;
	
	date = g_date_time_new_now_local ();
	g_assert (date);
	/* consult man strftime to translate this. This is a filename, so don't use "/" or ":", please */
	time = g_date_time_format (date, _("%Y%m%d-%H%M%S"));
	g_date_time_unref (date);
	
	if (mom_ps < 0) {
		station = g_strdup_printf (_("%.2f MHz"), rint (gtk_adjustment_get_value (adj))/STEPS);
	} else {
		preset* ps;

		g_assert (mom_ps < g_list_length (settings.presets));
		ps = g_list_nth_data (settings.presets, mom_ps);
		g_assert (ps);
	
		station = g_strdup (ps->title);
	}	
		
	start_recording (rec_settings.destination, station, time);
	g_free (station);
	g_free (time);
}

static void
about_button_clicked_cb (GtkButton *button,
                         gpointer   data)
{
	static GtkWidget *about;
	const char       *authors[] = {
		"J\xc3\xb6rgen Scheibengruber <mfcn@gmx.de>",
		NULL
	};	
	/* Feel free to put your names here translators :-) */
	char             *translators = _("TRANSLATORS");

	if (about) {
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	gtk_show_about_dialog (NULL,
                               "version", VERSION,
                               "copyright", _("Copyright \xc2\xa9 2001 - 2006 J\xc3\xb6rgen Scheibengruber"),
                               "comments", _("Listen to FM radio"),
                               "authors", authors,
                               "translator-credits", strcmp ("TRANSLATORS", translators) ? translators : NULL,
                               "logo-icon-name", "gnomeradio",
                               "license-type", GTK_LICENSE_GPL_2_0,
                               "wrap-license", TRUE,
                               "website-label", _("Gnomeradio Website"),
                               "website", "http://www.gnome.org/projects/gnomeradio/",
                               NULL);
}

static gint
delete_event_cb (GtkWidget   *window,
                 GdkEventAny *event,
                 gpointer     data)
{
	exit_gnomeradio ();

	return TRUE;
}

void
display_help_cb (char *topic)
{
	GError *error = NULL;
	char   *uri;

	if (topic)
		uri = g_strdup_printf ("help:gnomeradio/%s", topic);
	else
		uri = g_strdup ("help:gnomeradio");

	gtk_show_uri (NULL, uri, GDK_CURRENT_TIME, &error);

	if (error) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (NULL,
						 DIALOG_FLAGS,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Could not open %s: %s"),
						 uri, error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_error_free (error);
		error = NULL;
	}

	g_free (uri);
}

void
toggle_mainwindow_visibility (GtkWidget *app)
{
	static gint posx;
	static gint posy;

	if (gtk_widget_get_visible (app)) {
		gtk_window_get_position (GTK_WINDOW (app), &posx, &posy);
		gtk_widget_hide (app);
	} else {
		if ((posx >= 0) && (posy >= 0))
			gtk_window_move (GTK_WINDOW (app), posx, posy);
		gtk_window_present (GTK_WINDOW (app));
	}
}	
	
static GtkWidget *
gnomeradio_gui (void)
{
	GtkWidget *vbox;
	GtkWidget *box;
	GtkWidget *frame;
	GtkWidget *menubox;
	GtkWidget *label;
	GtkWidget *pixmap;
	GdkPixbuf *pixbuf;
	GtkWidget *vseparator;
	GtkWidget *about_button;
	GtkWidget *image;
	gchar     *text;
	
	app = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_resizable (GTK_WINDOW (app), FALSE);
	gtk_window_set_wmclass (GTK_WINDOW (app), "gnomeradio", "Gnomeradio");

	gtk_widget_realize (app);
	gtk_widget_get_window (app);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 2);

	drawing_area = gtk_drawing_area_new ();
	gtk_widget_set_size_request (drawing_area,
                                     DIGIT_WIDTH*6+10+SIGNAL_WIDTH+STEREO_WIDTH,
                                     DIGIT_HEIGTH+10);

	gtk_container_add (GTK_CONTAINER (frame), drawing_area);

	digits = gdk_pixbuf_new_from_xpm_data ((const char**) digits_xpm);
	signal_s = gdk_pixbuf_new_from_xpm_data ((const char**) signal_xpm);
	stereo = gdk_pixbuf_new_from_xpm_data ((const char**) stereo_xpm);

	menubox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	label = gtk_label_new (_("Presets:"));

	preset_combo = gtk_combo_box_text_new ();
	gtk_widget_set_size_request (preset_combo, 10, -1);

	gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 3);
	gtk_box_pack_start (GTK_BOX (box), menubox, TRUE, TRUE, 3);
	gtk_box_pack_start (GTK_BOX (menubox), label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (menubox), preset_combo, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 4);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	pixbuf = gdk_pixbuf_new_from_xpm_data ((const char**) freq_down_xpm);
	pixmap = gtk_image_new_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);

	gtk_box_pack_start (GTK_BOX (box), pixmap, FALSE, FALSE, 2);

	adj = GTK_ADJUSTMENT (gtk_adjustment_new (SUNSHINE*STEPS, FREQ_MIN*STEPS, FREQ_MAX*STEPS+1, 1, STEPS, 1));
	freq_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adj);
	gtk_scale_set_digits (GTK_SCALE (freq_scale), 0);
	gtk_scale_set_draw_value (GTK_SCALE (freq_scale), FALSE);

	gtk_box_pack_start (GTK_BOX (box), freq_scale, TRUE, TRUE, 0);

	pixbuf = gdk_pixbuf_new_from_xpm_data ((const char**) freq_up_xpm);
	pixmap = gtk_image_new_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);

	gtk_box_pack_start (GTK_BOX (box), pixmap, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 2);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	scbw_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("media-seek-backward", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (scbw_button), image);
	gtk_button_set_relief (GTK_BUTTON (scbw_button), GTK_RELIEF_NONE);

	gtk_box_pack_start (GTK_BOX (box), scbw_button, FALSE, FALSE, 2);

	stbw_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("media-skip-backward", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (stbw_button), image);
	gtk_button_set_relief (GTK_BUTTON (stbw_button), GTK_RELIEF_NONE);

	gtk_box_pack_start (GTK_BOX (box), stbw_button, FALSE, FALSE, 2);

	stfw_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("media-skip-forward", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (stfw_button), image);
	gtk_button_set_relief (GTK_BUTTON (stfw_button), GTK_RELIEF_NONE);

	gtk_box_pack_start (GTK_BOX (box), stfw_button, FALSE, FALSE, 2);

	scfw_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("media-seek-forward", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (scfw_button), image);
	gtk_button_set_relief (GTK_BUTTON (scfw_button), GTK_RELIEF_NONE);

	gtk_box_pack_start (GTK_BOX (box), scfw_button, FALSE, FALSE, 2);

	vseparator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);

	gtk_box_pack_start (GTK_BOX (box), vseparator, FALSE, FALSE, 2);

	volume_button = gtk_volume_button_new ();
	g_object_set (volume_button, "size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
	g_object_set (volume_button, "use-symbolic", FALSE, NULL);
	gtk_button_set_relief (GTK_BUTTON (volume_button), GTK_RELIEF_NONE);
	gtk_widget_set_sensitive (volume_button, FALSE);

	gtk_box_pack_start (GTK_BOX (box), volume_button, FALSE, FALSE, 2);

	rec_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("media-record", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (rec_button), image);
	gtk_button_set_relief (GTK_BUTTON (rec_button), GTK_RELIEF_NONE);

	gtk_box_pack_start (GTK_BOX (box), rec_button, FALSE, FALSE, 2);

	vseparator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);

	gtk_box_pack_start (GTK_BOX (box), vseparator, FALSE, FALSE, 2);

	prefs_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("document-properties", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (prefs_button), image);
	gtk_button_set_relief (GTK_BUTTON (prefs_button), GTK_RELIEF_NONE);

	gtk_box_pack_start (GTK_BOX (box), prefs_button, FALSE, FALSE, 2);

	about_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("help-about", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (about_button), image);
	gtk_button_set_relief (GTK_BUTTON (about_button), GTK_RELIEF_NONE);

	gtk_box_pack_start (GTK_BOX (box), about_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 4);

	g_signal_connect (app, "delete_event", G_CALLBACK (delete_event_cb), NULL);
	g_signal_connect (drawing_area, "draw", G_CALLBACK (draw_cb), NULL);
	g_signal_connect (preset_combo, "changed", G_CALLBACK (preset_combo_change_cb), NULL);
	g_signal_connect (adj, "value-changed", G_CALLBACK (adj_value_changed_cb), (gpointer) app);
	g_signal_connect (scbw_button, "clicked", G_CALLBACK (scbw_button_clicked_cb), NULL);
	g_signal_connect (stbw_button, "pressed", G_CALLBACK (step_button_pressed_cb), (gpointer) FALSE);
	g_signal_connect (stbw_button, "clicked", G_CALLBACK (step_button_clicked_cb), (gpointer) FALSE);
	g_signal_connect (stbw_button, "released", G_CALLBACK (step_button_released_cb), NULL);
	g_signal_connect (stfw_button, "pressed", G_CALLBACK (step_button_pressed_cb), (gpointer) TRUE);
	g_signal_connect (stfw_button, "clicked", G_CALLBACK (step_button_clicked_cb), (gpointer) TRUE);
	g_signal_connect (stfw_button, "released", G_CALLBACK (step_button_released_cb), NULL);
	g_signal_connect (scfw_button, "clicked", G_CALLBACK (scfw_button_clicked_cb), NULL);
	volume_value_changed_id = g_signal_connect (volume_button, "value-changed", G_CALLBACK (volume_value_changed_cb), NULL);
	g_signal_connect (rec_button, "clicked", G_CALLBACK (record_button_clicked_cb), (gpointer) app);
	g_signal_connect (prefs_button, "clicked", G_CALLBACK (prefs_button_clicked_cb), (gpointer) app);
	g_signal_connect (about_button, "clicked", G_CALLBACK (about_button_clicked_cb), NULL);

	gtk_widget_set_tooltip_text (scbw_button, _("Scan Backwards"));
	gtk_widget_set_tooltip_text (stbw_button, _("0.05 MHz Backwards"));
	gtk_widget_set_tooltip_text (stfw_button, _("0.05 MHz Forwards"));
	gtk_widget_set_tooltip_text (scfw_button, _("Scan Forwards"));
	gtk_widget_set_tooltip_text (volume_button, _("Adjust the Volume"));
	gtk_widget_set_tooltip_text (rec_button, _("Record radio as MP3, OGG, FLAC or M4A"));
	gtk_widget_set_tooltip_text (prefs_button, _("Preferences"));
	gtk_widget_set_tooltip_text (about_button, _("About"));

	text = g_strdup_printf (_("Frequency: %.2f MHz"), gtk_adjustment_get_value (adj)/STEPS);
	gtk_widget_set_tooltip_text (freq_scale, text);
	g_free (text);

	gtk_container_add (GTK_CONTAINER (app), vbox);
	gtk_widget_show_all (vbox);

	return app;
}

gboolean key_press_event_cb (GtkWidget *app, GdkEventKey *event, gpointer data)
{
	switch (event->keyval) {
		case GDK_KEY_F1: display_help_cb (NULL);
				break;
		case GDK_KEY_m: 
				toggle_volume ();
				break;
		case GDK_KEY_q: 
				exit_gnomeradio ();
				break;
		case GDK_KEY_r: 
				record_button_clicked_cb (NULL, app);
				break;
		case GDK_KEY_s:
				stop_record_button_clicked_cb (NULL, data);
				break;
		case GDK_KEY_f: 
				scfw_button_clicked_cb (NULL, NULL);
				break;
		case GDK_KEY_b: 
				scbw_button_clicked_cb (NULL, NULL);
				break;
		case GDK_KEY_n: 
				change_preset (TRUE);
				break;
		case GDK_KEY_p: 
				change_preset (FALSE);
				break;
		case GDK_KEY_KP_Add:
		case GDK_KEY_plus:	
				volume_up ();
				break;
		case GDK_KEY_minus:
		case GDK_KEY_KP_Subtract: 
				volume_down ();
				break;
	}

	return FALSE;
}

static void
startup_cb (GApplication *application,
	    gpointer      user_data)
{
	DriverType  driver = DRIVER_ANY;
	void       *md;
	GList      *l;

	gsettings = g_settings_new ("org.gnome.gnomeradio");

	main_visible = FALSE;
	app = gnomeradio_gui ();

	load_settings ();

	if (settings.driver) {
		if (g_strcmp0 (settings.driver, "v4l1") == 0)
			driver = DRIVER_V4L1;
		if (g_strcmp0 (settings.driver, "v4l2") == 0)
			driver = DRIVER_V4L2;
	}

	md = discover_media_devices ();

	for (l = get_device_list (); l != NULL; l = l->next) {
		device *dev;
		const char *p = NULL;
		gboolean audio = FALSE;

		dev = g_malloc0 (sizeof (device));

		dev->device = g_strdup (l->data);
		dev->name = radio_get_name (l->data, driver);
		p = get_associated_device (md,
                                           NULL,
                                           MEDIA_SND_CAP,
                                           l->data,
                                           MEDIA_V4L_RADIO);
		if (p)
			audio = TRUE;
		dev->audio = audio;
		
		settings.devices = g_list_append (settings.devices, dev);
		g_free (l->data);
	}

	free_media_devices (md);

	start_radio (FALSE, app);
	start_mixer (FALSE, app);

	if (is_first_run () || do_scan) {
		if (!radio_is_init ()) {
			g_message (_("Could not scan. Radio is not initialized."));
		} else {
			scan (app);
			set_first_run_flag ();
		}
	}

	create_tray_menu (app);
	
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (preset_combo), _("manual"));
	for (l = settings.presets; l != NULL; l = l->next) {
		preset *ps;

		ps = l->data;
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (preset_combo),
                                               ps->title);
	}
	preset_combo_set_item (mom_ps);

	gtk_application_add_window (GTK_APPLICATION (application), GTK_WINDOW (app));
	gtk_widget_show_all (app);
	main_visible = TRUE;

	create_tray_icon (app);

	adj_value_changed_cb (NULL, (gpointer) app);
	
#ifdef HAVE_LIRC
	if (init_lirc ())
		start_lirc ();
#endif

	connect_to_session ();

	g_signal_connect (app,  "key-press-event",  G_CALLBACK (key_press_event_cb),  NULL);

	g_timeout_add_seconds (3, (GSourceFunc)redraw_status_window, NULL);
}

static void
activate_cb (GApplication *application,
	     gpointer      user_data)
{
	gtk_window_present (GTK_WINDOW (app));
}

enum
{
	ARG_AUDIO_INPUT,
	ARG_AUDIO_OUTPUT,
	ARG_AUDIO_LATENCY
};

static gboolean
parse_one_option (gint         opt,
                  const gchar *arg,
                  GError      **err)
{
	void *md;
	const char *p = NULL;

	switch (opt)
	{
		case ARG_AUDIO_INPUT:
		{
			const char *device = NULL;

			md = discover_media_devices ();
			while (1) {
				device = get_associated_device (md,
                                                                device,
                                                                MEDIA_V4L_RADIO,
                                                                NULL,
                                                                NONE);
				if (device)
					break;
			}

			do {
				p = get_associated_device (md,
                                                           p,
                                                           MEDIA_SND_CAP,
                                                           device,
                                                           MEDIA_V4L_RADIO);
				if (p) {
					if ((strcmp (arg, p) == 0) ||
                                    	    (strcmp (arg, "disabled") == 0)) {
						audio_input = strdup (arg);
						break;
					} else {
						g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
					                     _("Invalid argument name '%s'.\n"
                                                               "\tAvailable options: --audio-input=[disabled|%s]"),
                                                             arg, p);
						free_media_devices (md);

						return FALSE;
					}
				}
			} while (p);
			free_media_devices (md);
			break;
		}
		case ARG_AUDIO_OUTPUT:
		{
			md = discover_media_devices ();
			do {
				p = get_not_associated_device (md,
                                                           p,
                                                           MEDIA_SND_OUT,
                                                           MEDIA_V4L_RADIO);
				if (p) {
					if ((strcmp (arg, p) == 0) ||
                                            (strcmp (arg, "default") == 0) ||
                                            (strcmp (arg, "disabled") == 0)) {
						audio_output = strdup (arg);
						break;
					} else {
						g_set_error (err, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
					                     _("Invalid argument name '%s'.\n"
                                                               "\tAvailable options: --audio-output=[disabled|default|%s]"),
                                                             arg, p);
						free_media_devices (md);

						return FALSE;
					}
				}
			} while (p);
			free_media_devices (md);
			break;
		}
		case ARG_AUDIO_LATENCY:
		{
			audio_latency = atoi (arg);
			break;
		}
		default:
			return FALSE;
	}

	return TRUE;
}

static gboolean option_audio_cb (const gchar *opt,
                                 const gchar *arg,
                                 gpointer     data,
                                 GError      **err)
{
	static const struct
	{
		const gchar *opt;
		int          val;
	} options[] = {
		{
		"--audio-input",      ARG_AUDIO_INPUT}, {
		"--audio-output",     ARG_AUDIO_OUTPUT}, {
		"--audio-latency",    ARG_AUDIO_LATENCY}, {
		NULL}
	};
	int val = 0, n;

	for (n = 0; options[n].opt; n++) {
		if (strcmp (opt, options[n].opt) == 0) {
			val = options[n].val;
			break;
		}
	}

	return parse_one_option (val, arg, err);
}

static GOptionGroup *
audio_get_option_group (void)
{
	GOptionGroup *group;

	static const GOptionEntry audio_options[] = {
		{ "audio-input", 0, 0, G_OPTION_ARG_CALLBACK, option_audio_cb,
                 N_("Set the audio device to read input on."), N_("DEVICE") },
		{ "audio-output", 0, 0, G_OPTION_ARG_CALLBACK, option_audio_cb,
                 N_("Set the audio device to write output to."), N_("DEVICE") },
		{ "audio-latency", 0, 0, G_OPTION_ARG_CALLBACK, option_audio_cb,
                 N_("Set the audio latency in ms."), N_("MSEC") },
		{ NULL }
	};

	group = g_option_group_new ("audio", _("Audio Options"),
				    _("Show Audio Options"), NULL, NULL);
	g_option_group_add_entries (group, audio_options);

	return group;
}

G_GNUC_NORETURN static gboolean
option_version_cb (const gchar *opt,
                   const gchar *arg,
                   gpointer     data,
                   GError      **error)
{
	g_print ("%s %s\n", PACKAGE, VERSION);
	exit (0);
}

int
main (int argc, char **argv)
{
	GtkApplication *application;
	GError         *error = NULL;
	GOptionContext *context = NULL;
	const GOptionEntry entries[] = {
		{ "scan", 's', 0, G_OPTION_ARG_NONE, &do_scan,
		 N_("Scan for available stations."), NULL },
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
		 N_("Enable verbose output."), NULL },
		{ "version", 0, G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, option_version_cb,
		 NULL, NULL},
		{ NULL }
	};

	guint status = 0;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_application_name (_("Gnomeradio"));
	g_setenv ("PULSE_PROP_media.role", "production", TRUE);

	context = g_option_context_new (N_("- Listen to FM radio"));
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_add_group (context, gst_init_get_option_group ());
	g_option_context_add_group (context, audio_get_option_group ());

	g_option_context_parse (context, &argc, &argv, &error);
	if (error) {
		g_printerr (_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
			    error->message, argv[0]);
		exit (1);
	}
	g_option_context_free (context);

	gtk_window_set_default_icon_name ("gnomeradio");

	application = gtk_application_new ("org.gnome.gnomeradio",
					   G_APPLICATION_FLAGS_NONE);

	g_signal_connect (application, "startup",
                          G_CALLBACK (startup_cb), NULL);
	g_signal_connect (application, "activate",
                          G_CALLBACK (activate_cb), NULL);

	status = g_application_run (G_APPLICATION (application), argc, argv);

	g_object_unref (application);

	if (gsettings)
		g_object_unref (gsettings);
	if (proxy)
		g_object_unref (proxy);
	if (digits)
		g_object_unref (digits);
	if (signal_s)
		g_object_unref (signal_s);
	if (stereo)
		g_object_unref (stereo);

#ifdef HAVE_LIRC
	deinit_lirc ();
#endif

	return status;
}
