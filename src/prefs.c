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
#include <gst/pbutils/pbutils.h>
#include <gst/pbutils/install-plugins.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <string.h>
#include <sys/stat.h>

#include "prefs.h"
#include "trayicon.h"
#include "gui.h"
#include "radio.h"
#include "rec_tech.h"
#include "media_types.h"
#include "missing_plugins.h"
#include "utils.h"

#define GNOMERADIO_XML_FILENAME "gnomeradio.xml"
#define GNOMERADIO_DTD_RESOURCENAME "/org/gnome/gnomeradio/gnomeradio.dtd"

extern gnomeradio_settings settings;
extern GSettings *gsettings;
extern gboolean         main_visible;

extern int              mom_ps;
extern int              audio_loopback;

extern GtkAdjustment    *adj;
extern GtkWidget        *preset_combo;

static GtkWidget        *radio_combo;
static GtkWidget        *mixer_combo;
static GtkWidget        *audio_box;
static GtkWidget        *audio_switch;
static GtkWidget        *mute_on_exit;
static GtkWidget        *treeview;
static GtkWidget        *save_button;
static GtkWidget        *move_up_button;
static GtkWidget        *move_down_button;
static GtkWidget        *add_button;
static GtkWidget        *remove_button;
static GtkWidget        *audio_profile_combo;
static GtkWidget        *install_button;
static GtkTreeSelection *selection;
static GtkListStore     *store;

static gboolean         save_presets;
static gboolean         load_presets;

static void presets_save (void);
static void presets_load (void);

void
save_settings (void)
{
	gint count;
	
	/* Store general settings */
	g_settings_set_string (gsettings, "device", settings.device);
	g_settings_set_string (gsettings, "driver", settings.driver);
	g_settings_set_string (gsettings, "mixer-device", settings.mixer_device);
	g_settings_set_string (gsettings, "mixer-channel", settings.mixer_channel);
	g_settings_set_boolean (gsettings, "muted", settings.muted);
	g_settings_set_double (gsettings, "unmute-volume", settings.unmute_volume);
	g_settings_set_boolean (gsettings, "mute-on-exit", settings.mute_on_exit);
	g_settings_set_boolean (gsettings, "audio-loopback", settings.audio_loopback);
	g_settings_set_double (gsettings, "last-freq", gtk_adjustment_get_value (adj)/STEPS);

	/* Store recording settings */
	g_settings_set_string (gsettings, "destination", rec_settings.destination);
	g_settings_set_string (gsettings, "profile", rec_settings.profile);

	/* Store the presets */
	presets_save ();

	count = g_list_length (settings.presets);
	g_settings_set_int (gsettings, "presets", count);
	g_settings_set_int (gsettings, "last", mom_ps);
}			

void
load_settings (void)
{
	double freq;
	gint   count;

	/* Load general settings */
	settings.driver = g_settings_get_string (gsettings, "driver");
	if (settings.driver == NULL)
		settings.driver = g_strdup ("any");
	settings.device = g_settings_get_string (gsettings, "device");
        if (settings.device) {
		GList    *l;
                gboolean  found = FALSE;
		
                for (l = get_device_list (); l && !found; l = l->next) {
                        if (g_strcmp0 (settings.device, l->data) == 0)
                                found = TRUE;
			g_free (l->data);
                }

                if (!found) {
                        g_warning ("Invalid value for \"device\" key: %s",
                                   settings.device);
			settings.device = g_strdup ("/dev/radio0");
                }
        }
	settings.mixer_device = g_settings_get_string (gsettings, "mixer-device");
        if (settings.mixer_device) {
		GList    *l;
                gboolean  found = FALSE;
		
                for (l = get_sound_card_list (); l && !found; l = l->next) {
                        if (g_strcmp0 (settings.mixer_device, l->data) == 0)
                                found = TRUE;
			g_free (l->data);
                }

                if (!found) {
                        g_warning ("Invalid value for \"mixer-device\" key: %s",
                                   settings.mixer_device);
			settings.mixer_device = g_strdup ("hw:0");
                }
        }
	settings.mixer_channel = g_settings_get_string (gsettings, "mixer-channel");
	if (rec_settings.profile == NULL)
		rec_settings.profile = g_strdup ("Line");
	settings.muted = g_settings_get_boolean (gsettings, "muted");
	settings.unmute_volume = g_settings_get_double (gsettings, "unmute-volume");
	settings.mute_on_exit = g_settings_get_boolean (gsettings, "mute-on-exit");
	settings.audio_loopback = g_settings_get_boolean (gsettings, "audio-loopback");
	freq = g_settings_get_double (gsettings, "last-freq");
	if (freq < FREQ_MIN) freq = FREQ_MIN;
	if (freq > FREQ_MAX) freq = FREQ_MAX;
	gtk_adjustment_set_value (adj, freq * STEPS);
	
	/* Load recording settings */
	rec_settings.destination = g_settings_get_string (gsettings, "destination");
	if ((rec_settings.destination == NULL) || (g_strcmp0 (rec_settings.destination, "") == 0))
		rec_settings.destination = g_strdup (g_get_home_dir ());
	rec_settings.profile = g_settings_get_string (gsettings, "profile");
	if (rec_settings.profile == NULL)
		rec_settings.profile = g_strdup ("audio/x-vorbis");
	
	/* Load the presets */
	presets_load ();

	count = g_settings_get_int (gsettings, "presets");
	mom_ps = g_settings_get_int (gsettings, "last");
	if (mom_ps >= count)
		mom_ps = -1;
}

static void
mute_on_exit_toggled_cb (GtkWidget *widget,
                         gpointer  data)
{
	settings.mute_on_exit = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mute_on_exit));
}

static void
audio_box_set_visible (void)
{
	device *dev;
	int active;

	active = gtk_combo_box_get_active (GTK_COMBO_BOX (radio_combo));
	
	dev = (device *)g_list_nth_data (settings.devices, active);
	gtk_widget_hide (audio_box);
	if (dev->audio) {
		gtk_widget_set_no_show_all (audio_box, FALSE);
		gtk_widget_show_all (audio_box);
	}
}

static void
audio_switch_activate_cb (GtkWidget* widget,
                          gpointer data)
{
	settings.audio_loopback = gtk_switch_get_active (GTK_SWITCH (audio_switch));

	start_radio (TRUE, data);
}

static gboolean
radio_combo_change_cb (GtkWidget *widget,
                       gpointer    data)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;
	char         *device = NULL;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));	

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter) == FALSE)
		return FALSE;
	gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
			   0, &device,
			   -1);

	if (g_strcmp0 (settings.device, device) == 0) {
		g_free (device);
		return FALSE;
	}

	if (settings.device) {
		g_free (settings.device);
		settings.device = g_strdup (device);
		g_free (device);
	}

	start_radio (TRUE, data);

	audio_box_set_visible ();

	return FALSE;
}

static gboolean
mixer_combo_change_cb (GtkComboBox *combo,
                       gpointer data)
{
	gchar *channel;
	GList *l;
	int    active;

	
	g_assert (combo);
	l = g_object_get_data (G_OBJECT (combo), "channel");
	active = gtk_combo_box_get_active (combo);
	g_assert (active > -1);
	
	channel = (gchar*)g_list_nth_data (l, active);
	g_assert (channel);
	
	if (g_strcmp0 (settings.mixer_channel, channel) == 0)
		return FALSE;

	if (settings.mixer_channel) g_free (settings.mixer_channel);
	settings.mixer_channel = g_strdup (channel);
	
	start_mixer (TRUE, data);
	
	return FALSE;
}

static void
audio_profile_combo_change_cb (GtkWidget *widget,
                               gpointer data)
{
	GstEncodingProfile *profile;
	GtkTreeModel       *model;
	GtkTreeIter         iter;
	char               *media_type = NULL;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter) == FALSE)
		return;
	gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
			   0, &media_type,
			   2, &profile,
			   -1);

	if (g_strcmp0 (rec_settings.profile, media_type) == 0)
		return;
	rec_settings.profile = g_strdup (media_type);
	g_free (media_type);

	if (check_missing_plugins (profile, NULL, NULL)) {
		gtk_widget_set_visible (install_button, TRUE);

		gtk_widget_set_sensitive (install_button,
					 gst_install_plugins_supported ());
	} else {
		gtk_widget_set_visible (install_button, FALSE);
	}
}

static void
audio_profile_combo_set_active (GtkWidget  *widget,
                                  const char *profile)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;
	gboolean      done;

	done = FALSE;
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			char *media_type;

			gtk_tree_model_get (model, &iter, 0, &media_type, -1);
			if (g_strcmp0 (media_type, profile) == 0) {
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget),
                                                              &iter);
				done = TRUE;
			}
			g_free (media_type);
		} while (done == FALSE && gtk_tree_model_iter_next (model, &iter));
	}

	if (done == FALSE)
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), NULL);
}

static GtkWidget *
audio_profile_combo_new (void)
{
	GstEncodingTarget *target;
	GtkCellRenderer   *renderer;
	GtkTreeModel      *model;
	GtkWidget         *combo;
	const GList       *p;

	model = GTK_TREE_MODEL (gtk_tree_store_new (3,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING,
                                                  G_TYPE_POINTER));

	target = get_default_encoding_target ();
	for (p = gst_encoding_target_get_profiles (target); p != NULL; p = p->next) {
		GstEncodingProfile *profile = GST_ENCODING_PROFILE (p->data);
		char *media_type;

		media_type = encoding_profile_get_media_type (profile);
		if (media_type == NULL)
			continue;
		gtk_tree_store_insert_with_values (GTK_TREE_STORE (model),
			          		   NULL, NULL, -1,
			          		   0, media_type,
			          		   1, gst_encoding_profile_get_description (profile),
			          		   2, profile, -1);
		g_free (media_type);
	}

	combo = gtk_combo_box_new_with_model (model);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo),
                                       renderer,
                                       "text",
                                       1,
                                       NULL);

	return GTK_WIDGET (combo);
}

static void
audio_plugin_install_done_cb (gpointer inst,
                              gboolean retry,
                              gpointer data)
{
	audio_profile_combo_change_cb (audio_profile_combo, data);
}

static void
audio_profile_install_plugins_cb (GtkWidget *widget,
                                  gpointer   data)
{
	GstEncodingProfile *profile;
	GClosure           *closure;
	char               **details;

	profile = get_encoding_profile (rec_settings.profile);
	if (profile == NULL) {
		g_free (rec_settings.profile);
		return;
	}
	g_free (rec_settings.profile);

	if (check_missing_plugins (profile, &details, NULL) == FALSE)
		return;

	closure = g_cclosure_new ((GCallback) audio_plugin_install_done_cb,
				 g_object_ref (audio_profile_combo),
				 (GClosureNotify) g_object_unref);
	g_closure_set_marshal (closure, g_cclosure_marshal_VOID__BOOLEAN);

	missing_plugins_install ((const char **)details, TRUE, closure);

	g_closure_sink (closure);
	g_strfreev (details);
}

static void
update_button (void)
{
	GtkTreeModel *model;
	GtkTreeIter  iter;
	gboolean     can_remove = FALSE, can_save = FALSE, can_move_up = FALSE, can_move_down = FALSE;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GtkTreePath *path;
		int selected;

		path = gtk_tree_model_get_path (model, &iter);	
		selected = gtk_tree_path_get_indices (path)[0];

		can_remove = TRUE;
		can_save = TRUE;
		can_move_up = selected > 0;
		can_move_down = selected < gtk_tree_model_iter_n_children (model, NULL) - 1;

		gtk_tree_path_free (path);
	}

	gtk_widget_set_sensitive (remove_button, can_remove);
	gtk_widget_set_sensitive (save_button, can_save);
	gtk_widget_set_sensitive (move_up_button, can_move_up);
	gtk_widget_set_sensitive (move_down_button, can_move_down);
}

static void add_button_clicked_cb (GtkWidget *widget,
                                   gpointer   data)
{
	GtkTreePath    *path = NULL;
	GtkTreeIter     iter = {0};
	GtkAdjustment  *v;
	preset         *ps;
	gchar          *buffer;
	
	ps = g_malloc0 (sizeof (preset));
	ps->title = g_strdup (_("unnamed"));
	ps->freq = rint (gtk_adjustment_get_value (adj)) / STEPS;
	settings.presets = g_list_append (settings.presets, (gpointer) ps);
	buffer = g_strdup_printf ("%.2f", ps->freq);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (GTK_LIST_STORE (store), &iter,
                            0, ps->title,
                            1, buffer,
                            -1);

	g_free (buffer);
	gtk_tree_selection_unselect_all (selection);
	
	v = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (treeview));
	gtk_adjustment_set_value (v, gtk_adjustment_get_upper (v));
	
	if (main_visible) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (preset_combo),
                                               ps->title);
		mom_ps = g_list_length (settings.presets) - 1;
		preset_combo_set_item (mom_ps);

		tray_menu_add_preset (ps, mom_ps);
	}

	buffer = g_strdup_printf ("%d", g_list_length (settings.presets) - 1);
	path = gtk_tree_path_new_from_string (buffer);
	g_free (buffer);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
	gtk_tree_path_free (path);

	update_button ();
}

static void
remove_button_clicked_cb (GtkWidget *widget,
                          gpointer   data)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GtkTreePath *path;
		preset      *ps;
		int         *row;

		path = gtk_tree_model_get_path (model, &iter);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

		row = gtk_tree_path_get_indices (path);
		g_assert (row);
		g_assert (*row < g_list_length (settings.presets));

		ps = g_list_nth_data (settings.presets, *row);
		g_assert (ps);	
		settings.presets = g_list_remove (settings.presets, (gpointer)ps);
		g_free (ps->title);
		g_free (ps);
	
		if (main_visible) {
			gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (preset_combo),
                                                  *row + 1);
			if (--mom_ps < 0) mom_ps = 0;
			if (settings.presets == NULL) mom_ps = -1;
			preset_combo_set_item (mom_ps);

			tray_menu_remove_preset (*row);
		}
	
		gtk_tree_path_prev (path);
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview),
                                         path,
                                         NULL,
                                         FALSE);
		gtk_tree_path_free (path);

		update_button ();
	}
}

static void
move_up_button_clicked_cb (GtkWidget *widget,
                           gpointer   data)
{
	GtkTreeModel *model;
	GtkTreeIter   iter, iter_prev;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GtkTreePath *path;
		preset      *ps;
		GList       *l;
		int         *row;

		path = gtk_tree_model_get_path (model, &iter);

		if (!gtk_tree_path_prev (path)) {
			gtk_tree_path_free (path);
			return;
		}

		gtk_tree_model_get_iter (model, &iter_prev, path);

		gtk_list_store_swap (GTK_LIST_STORE (model), &iter_prev, &iter);

		row = gtk_tree_path_get_indices (path);
		g_assert (row);
		g_assert (*row < g_list_length (settings.presets));

		ps = g_list_nth_data (settings.presets, *row);
		g_assert (ps);

		l = g_list_find (settings.presets, (gpointer)ps);
		settings.presets = g_list_swap_next (settings.presets, l);

		if (main_visible) {
			gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (preset_combo),
                                                  *row + 1);
			gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (preset_combo),
                                                       (*row + 1) + 1,
                                                       ps->title);
			mom_ps = *row;
			preset_combo_set_item (mom_ps);

			tray_menu_move_up_preset (ps, mom_ps);
		}

		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview),
                                             path,
                                             NULL,
                                             FALSE,
                                             0,
                                             0);
		gtk_tree_path_free (path);
	}

	update_button ();
}

static void
move_down_button_clicked_cb (GtkWidget *widget,
                             gpointer   data)
{
	GtkTreeModel *model;
	GtkTreeIter   iter, iter_next;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GtkTreePath *path;
		GList *l;
		preset *ps;
		int *row;

		path = gtk_tree_model_get_path (model, &iter);

		gtk_tree_path_next (path);

		gtk_tree_model_get_iter (model, &iter_next, path);

		gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &iter_next);

		row = gtk_tree_path_get_indices (path);
		g_assert (row);
		g_assert (*row < g_list_length (settings.presets));

		ps = g_list_nth_data (settings.presets, *row);
		g_assert (ps);

		l = g_list_find (settings.presets, (gpointer)ps);
		settings.presets = g_list_swap_prev (settings.presets, l);

		if (main_visible) {
			gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (preset_combo),
                                                  *row + 1);
			gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (preset_combo),
                                                       (*row + 1) - 1,
                                                       ps->title);
			mom_ps = *row;
			preset_combo_set_item (mom_ps);

			tray_menu_move_down_preset (ps, mom_ps);
		}

		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview),
                                             path,
                                             NULL,
                                             FALSE,
                                             0,
                                             0);
		gtk_tree_path_free (path);
	}

	update_button ();
}

static void
name_cell_edited_cb (GtkCellRendererText *renderertext,
                     gchar               *path_str,
                     gchar               *new_val,
                     gpointer             data)
{
	GtkTreePath *path = NULL;
	GtkTreeIter  iter;
	preset      *ps;
	int         *row;
	
	path = gtk_tree_path_new_from_string (path_str);

	row = gtk_tree_path_get_indices (path);
	g_assert (row);
	g_assert (*row < g_list_length (settings.presets));

	ps = g_list_nth_data (settings.presets, *row);
	g_assert (ps);	
	if (ps->title) g_free (ps->title);
	ps->title = g_strdup (new_val);

	if (main_visible) {
		gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (preset_combo), *row + 1);
		gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (preset_combo), *row + 1, ps->title);
		mom_ps = *row;
		preset_combo_set_item (mom_ps);

		tray_menu_update_preset (ps, mom_ps);
	}
	
	gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
	gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, new_val, -1);
	gtk_tree_path_free (path);	
}	

static void
freq_cell_edited_cb (GtkCellRendererText *renderertext,
                     gchar               *path_str,
                     gchar               *new_val,
                     gpointer             data)
{
	GtkTreePath *path = NULL;
	GtkTreeIter  iter;
	preset      *ps;
	double       value;
	gchar       *freq;
	int         *row;
	
	if (sscanf (new_val, "%lf", &value) != 1)
		return;
	
	if (value < FREQ_MIN) value = FREQ_MIN;
	if (value > FREQ_MAX) value = FREQ_MAX;
	value = rint (value * STEPS) / STEPS;
	
	freq = g_strdup_printf ("%.2f", value);
	
	path = gtk_tree_path_new_from_string (path_str);
	
	row = gtk_tree_path_get_indices (path);
	g_assert (row);
	g_assert (*row < g_list_length (settings.presets));

	ps = g_list_nth_data (settings.presets, *row);
	g_assert (ps);	
	ps->freq = value;

	gtk_adjustment_set_value (adj, value * STEPS);
	mom_ps = *row;
	preset_combo_set_item (mom_ps);
	
	gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
	gtk_list_store_set (GTK_LIST_STORE (store), &iter, 1, freq, -1);
	g_free (freq);
	gtk_tree_path_free (path);	
}

static void
treeview_cursor_changed_cb (GtkWidget *widget,
                            gpointer   data)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GtkTreePath *path;
		int *row;

		path = gtk_tree_model_get_path (model, &iter);

		row = gtk_tree_path_get_indices (path);
		g_assert (row);

		mom_ps = *row;
		preset_combo_set_item (mom_ps);

		gtk_tree_path_free (path);
	}

	update_button ();
}

static gboolean
treeview_key_press_event_cb (GtkWidget   *widget,
                             GdkEventKey *event,
                             gpointer     data)
{
	if (event->keyval == GDK_KEY_Delete)
		remove_button_clicked_cb (widget, data);
	if (event->keyval == GDK_KEY_Insert)
		add_button_clicked_cb (widget, data);
	
	return FALSE;
}		

void 
treeview_switch_to_preset (gint active)
{
	GtkTreeIter iter;
	gboolean    valid;

	valid = gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter,
                                              NULL,
                                              active);
	if (valid) {
		GtkTreePath *path;

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (store),
                                               &iter);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview), path, NULL, FALSE, 0, 0);
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
		gtk_tree_path_free (path);

		gtk_widget_grab_focus (treeview);
	}
}

static void
treeview_scroll_to_active_preset_cb (GtkWidget *widget,
                                     gpointer   data)
{
	gint active;

	if (settings.presets == NULL) {
		gtk_widget_grab_focus (add_button);
		return;
	}

	active = gtk_combo_box_get_active (GTK_COMBO_BOX (preset_combo)) - 1;

	if (active < 0) {
		gtk_widget_grab_focus (add_button);
		return;
	}

	treeview_switch_to_preset (active);
}

static preset *
presets_list_new (const gchar *title,
                  const gchar *freq)
{
 	preset *ps;
	double  value;

	ps = g_malloc0 (sizeof (preset));
	if (g_strcmp0 (title, "") == 0)
		ps->title = g_strdup (_("unnamed"));
	else
		ps->title = g_strdup (title);

	value = g_ascii_strtod (freq, NULL);
	if (value < FREQ_MIN) value = FREQ_MIN;
	if (value > FREQ_MAX) value = FREQ_MAX;
	ps->freq = value;

	return ps;
}

static void
presets_list_free (gpointer data)
{
	preset *ps;

	ps = (preset *)data;
	g_free (ps->title);
	g_free (ps);
}

static gboolean
gnomeradio_xml_validate_from_resource (xmlDoc      *doc,
                                       const gchar *dtd_resourcename)
{
	GBytes                  *resourcecontents;
	gconstpointer            resourcedata;
	gsize                    resourcesize;
	xmlParserInputBufferPtr  buffer;
	xmlValidCtxt             cvp;
	xmlDtd                  *dtd;
	GError                  *error = NULL;
	gboolean                 ret;

	resourcecontents = g_resources_lookup_data (dtd_resourcename,
                                                    G_RESOURCE_LOOKUP_FLAGS_NONE,
                                                    &error);
	if (error != NULL) {
		g_warning ("Unable to load dtd resource '%s': %s",
                           dtd_resourcename, error->message);
		g_error_free (error);
		return FALSE;
	}
	resourcedata = g_bytes_get_data (resourcecontents, &resourcesize);
	buffer = xmlParserInputBufferCreateStatic (resourcedata, resourcesize, XML_CHAR_ENCODING_UTF8);

	memset (&cvp, 0, sizeof (cvp));
	dtd = xmlIOParseDTD (NULL, buffer, XML_CHAR_ENCODING_UTF8);
	ret = xmlValidateDtd (&cvp, doc, dtd);

	xmlFreeDtd (dtd);
	g_bytes_unref (resourcecontents);

	return ret;
}

static gboolean
presets_file_save (const gchar *filename)
{
	xmlDocPtr   doc;
	xmlNodePtr  comment;
	xmlNodePtr  root;
	xmlNodePtr  node;
	GList      *l;

	doc = xmlNewDoc ((const xmlChar *) "1.0");

	if (save_presets)
		comment = xmlNewDocComment (doc, (const xmlChar *)
                                            _("\n"
                                            "          Listen to FM radio\n"));
	else
		comment = xmlNewDocComment (doc, (const xmlChar *)
                                            _("\n"
                                            "                      Listen to FM radio\n\n"
                                            "            This file was automatically generated.\n"
                                            "  Please MAKE SURE TO BACKUP THIS FILE before making changes.\n"));
	xmlAddChild ((xmlNodePtr) doc, comment);
	root = xmlNewNode (NULL, (const xmlChar *) "gnomeradio");
	xmlDocSetRootElement (doc, root);
	xmlAddPrevSibling (root, comment);

	for (l = settings.presets; l; l = l->next) {
		preset *ps;
		gchar  *buffer;

		ps = l->data;

		node = xmlNewChild (root, NULL, (const xmlChar *) "station", NULL);

		xmlNewProp (node, (const xmlChar *) "name", (const xmlChar *) ps->title);
		buffer = g_strdup_printf ("%0.2f", ps->freq);
		xmlNewProp (node, (const xmlChar *) "freq", (const xmlChar *) buffer);
		g_free (buffer);
	}

	xmlIndentTreeOutput = 1;

	xmlSaveFormatFileEnc (filename, doc, "UTF-8", 1);
	xmlFreeDoc (doc);

	xmlMemoryDump ();

	return TRUE;
}

static void
presets_file_parse (const gchar *file)
{
	xmlParserCtxtPtr  ctxt;
	xmlDocPtr         doc;
	xmlNodePtr        root;
	xmlNodePtr        node;
	preset           *ps;

	ctxt = xmlNewParserCtxt ();

	doc = xmlCtxtReadFile (ctxt, file, NULL, 0);
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", file);
		xmlFreeParserCtxt (ctxt);
		return;
	}

	if (!gnomeradio_xml_validate_from_resource (doc, GNOMERADIO_DTD_RESOURCENAME)) {
		g_warning ("Failed to validate file:'%s'", file);
		xmlFreeDoc (doc);
		xmlFreeParserCtxt (ctxt);
		return;
	}

	if (settings.presets != NULL) {
		if (load_presets) {
			gtk_list_store_clear (GTK_LIST_STORE (store));

			if (main_visible) {
				gint i, count;

				count = g_list_length (settings.presets);
				for (i = 0; i < count; i++) {
					gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (preset_combo), 1);
					preset_combo_set_item (-1);

					tray_menu_remove_preset (0);
				}
			}
		}

		g_list_free_full (settings.presets, presets_list_free);
		settings.presets = NULL;
	}

	root = xmlDocGetRootElement (doc);

	node = root->children;
	while (node) {
		if (strcmp ((gchar *) node->name, "station") == 0) {
			gchar *title;
			gchar *freq;

			title = (gchar *) xmlGetProp (node, (const xmlChar *) "name");
			freq = (gchar *) xmlGetProp (node, (const xmlChar *) "freq");

			if (title && freq) {
				ps = presets_list_new (title, freq);
				settings.presets = g_list_append (settings.presets, ps);
			}

			xmlFree (title);
			xmlFree (freq);
		}

		node = node->next;
	}

	xmlFreeDoc (doc);
	xmlFreeParserCtxt (ctxt);

	if (load_presets) {
		GList *l;

		for (l = settings.presets; l != NULL; l = l->next) {
			GtkTreeIter iter = {0};
			gchar *buffer;

			ps = (preset *)l->data;
			buffer = g_strdup_printf ("%0.2f", ps->freq);
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (GTK_LIST_STORE (store), &iter,
                                            0, ps->title,
                                            1, buffer,
                                            -1);
			g_free (buffer);

			if (main_visible) {
				gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (preset_combo), ps->title);
				mom_ps = g_list_length (settings.presets) - 1;
				preset_combo_set_item (mom_ps);

				tray_menu_add_preset (ps, mom_ps);
			}
		}

		treeview_scroll_to_active_preset_cb (treeview, NULL);
		treeview_cursor_changed_cb (treeview, NULL);

		update_button ();
	}
}

static void
presets_save (void)
{
	gchar *dir;
	gchar *path;

	dir = g_build_filename (g_get_user_config_dir (), PACKAGE_NAME, NULL);
	g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	path = g_build_filename (dir, GNOMERADIO_XML_FILENAME, NULL);
	g_free (dir);

	presets_file_save (path);

	g_free (path);
}

static void
presets_load (void)
{
	gchar *dir;
	gchar *path;

	dir = g_build_filename (g_get_user_config_dir (), PACKAGE_NAME, NULL);
	path = g_build_filename (dir, GNOMERADIO_XML_FILENAME, NULL);
	g_free (dir);

	if (g_file_test (path, G_FILE_TEST_EXISTS))
		presets_file_parse (path);
	g_free (path);
}

static void
add_default_file_filters (GtkWidget *chooser)
{
	GtkFileFilter *all;
	GtkFileFilter *xml;

	all = gtk_file_filter_new ();
	gtk_file_filter_set_name (all, _("All files"));
	gtk_file_filter_add_pattern (all, "*");

	xml = gtk_file_filter_new ();
	gtk_file_filter_set_name (xml, _("XML files"));
	gtk_file_filter_add_pattern (xml, "*.xml");
	gtk_file_filter_add_pattern (xml, "*.XML");
	gtk_file_filter_add_mime_type (xml, "text/xml");

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), all);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), xml);

        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), xml);
}

static void
save_presets_to_file_cb (GtkWidget *button,
                         gpointer   data)
{
	GtkWidget *dialog;

	save_presets = TRUE;

	dialog = gtk_file_chooser_dialog_new (_("Select file name"),
						NULL,
						GTK_FILE_CHOOSER_ACTION_SAVE,
						_("_Save"), GTK_RESPONSE_ACCEPT,
						_("_Cancel"), GTK_RESPONSE_CANCEL,
						NULL);

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
	add_default_file_filters (dialog);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "gnomeradio.xml");

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *file;

		file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if (!g_str_has_suffix (file, ".xml")) {
			gchar *tmp_file;

			tmp_file = g_strdup (file);
			g_free (file);
        		file = g_strdup_printf ("%s.xml", tmp_file);
			g_free (tmp_file);
		}
		presets_file_save (file);
		g_free (file);
	}

	gtk_widget_destroy (dialog);

	save_presets = FALSE;
}

static void
load_presets_from_file_cb (GtkWidget *button,
                           gpointer   data)
{
	GtkWidget *dialog;

	load_presets = TRUE;

	dialog = gtk_file_chooser_dialog_new (_("Select file name"),
					      NULL,
					      GTK_FILE_CHOOSER_ACTION_OPEN,
					      _("_Open"), GTK_RESPONSE_ACCEPT,
					      _("_Cancel"), GTK_RESPONSE_CANCEL,
					      NULL);

	add_default_file_filters (dialog);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		presets_file_parse (filename);
		g_free (filename);
	}

	gtk_widget_destroy (dialog);

	load_presets = FALSE;
}

static void
destination_button_clicked_cb (GtkWidget *button,
                               gpointer   data)
{
	if (rec_settings.destination)
		g_free (rec_settings.destination);

	rec_settings.destination = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (button));
}

static void
free_list (GList *list)
{
	if (!list)
		return;

	g_list_free_full (list, g_free);
}

GtkWidget *
prefs_window (GtkWidget *app)
{
	GtkWidget          *dialog;
	GtkWidget          *content_area;
	GtkWidget          *box;
	GtkWidget          *grid;
	GtkWidget          *label;
	GtkWidget          *mixer_event_box;
	GtkWidget          *scrolled_window;
	GtkCellRenderer    *renderer;
	GtkTreeViewColumn  *list_column;
	GtkWidget          *button_box;
	GtkWidget          *open_button;
	GtkWidget          *destination_button;
	GtkWidget          *image;
	GstEncodingProfile *profile;
	gchar              *markup;
	GList              *channel, *l;
	gint                i, active;
	
	dialog = gtk_dialog_new_with_buttons (_("Gnomeradio Settings"), GTK_WINDOW (app),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      _("_Close"), GTK_RESPONSE_CLOSE,
					      _("_Help"), GTK_RESPONSE_HELP,
					      NULL);
	
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_set_border_width (GTK_CONTAINER (box), 10);
	gtk_box_pack_start (GTK_BOX (content_area), box, TRUE, TRUE, 0);
	
	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 15);
	gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);
	gtk_box_pack_start (GTK_BOX (box), grid, TRUE, TRUE, 0);

	/* The general settings part */
	label = gtk_label_new (NULL);
 	gtk_widget_set_halign (label, GTK_ALIGN_START);
	markup = g_strdup_printf ("<span weight=\"bold\">%s</span>", _("General Settings"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

	label = gtk_label_new (_("Radio device:"));
 	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_margin_start (label, 10);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

	store = gtk_list_store_new (3,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_BOOLEAN);

	radio_combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (radio_combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (radio_combo), renderer,
                                        "text", 1,
                                        NULL);

	for (l = settings.devices; l != NULL; l = l->next) {
		GtkTreeIter  iter = {0};
		device      *dev;

		dev = l->data;
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
                                    0, dev->device,
                                    1, dev->name,
                                    2, dev->audio,
                                    -1);
		if (g_strcmp0 (dev->device, settings.device) == 0)
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (radio_combo), &iter);
	}

	gtk_grid_attach (GTK_GRID (grid), radio_combo, 1, 1, 1, 1);

	label = gtk_label_new (_("Mixer channel:"));
 	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_margin_start (label, 10);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

	mixer_event_box = gtk_event_box_new ();
	mixer_combo = gtk_combo_box_text_new ();
	gtk_container_add (GTK_CONTAINER (mixer_event_box), mixer_combo);

	l = channel = get_mixer_list ();
	for (i = 0, active = 0; l; l = l->next) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (mixer_combo), l->data);
		if (g_strcmp0 (l->data, settings.mixer_channel) == 0)
			active = i;
		++i;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (mixer_combo), active);
	g_object_set_data_full (G_OBJECT (mixer_combo), "channel", channel, (GDestroyNotify)free_list);
	gtk_grid_attach (GTK_GRID (grid), mixer_event_box, 1, 2, 1, 1);

	audio_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_no_show_all (audio_box, TRUE);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	label = gtk_label_new (_("Audio loopback mode"));
 	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_margin_start (label, 10);

	audio_switch = gtk_switch_new ();

	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (box), audio_switch, FALSE, FALSE, 0);

	label = gtk_label_new (NULL);
 	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_START);
 	gtk_widget_set_margin_start (label, 10);
	markup = g_strconcat ("<span size=\"small\" style=\"italic\"><b>", _("Note:    "), "</b>",
			    _("The audio loopback mode is required when radio card is not connected "
			      "to the sound card\nvia a cable. In this case, Gnomeradio needs to map the audio "
			      "from the internal digital capture out\nto sound card."),
			      "</span>", NULL);
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	gtk_box_pack_start (GTK_BOX (audio_box), box, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (audio_box), label, FALSE, FALSE, 0);
	gtk_grid_attach (GTK_GRID (grid), audio_box, 0, 3, 2, 1);

	/* Enabled audio loopback from switch|commandline */
	if (settings.audio_loopback)
		gtk_switch_set_active (GTK_SWITCH (audio_switch), TRUE);
	else
		gtk_switch_set_active (GTK_SWITCH (audio_switch), FALSE);

	audio_box_set_visible ();

	mute_on_exit = gtk_check_button_new_with_label (_("Mute on exit"));
	gtk_widget_set_margin_start (mute_on_exit, 10);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_on_exit), settings.mute_on_exit);
	gtk_grid_attach (GTK_GRID (grid), mute_on_exit, 0, 4, 2, 1);

	g_signal_connect (radio_combo, "changed",
                          G_CALLBACK (radio_combo_change_cb), app);
	g_signal_connect (mixer_combo, "changed",
                          G_CALLBACK (mixer_combo_change_cb), app);
	g_signal_connect (audio_switch, "notify::active",
                          G_CALLBACK (audio_switch_activate_cb), NULL);
	g_signal_connect (mute_on_exit, "toggled",
                          G_CALLBACK (mute_on_exit_toggled_cb), NULL);

	gtk_widget_set_tooltip_text (radio_combo, _("The radio device to use"));
	gtk_widget_set_tooltip_text (mixer_event_box, _("The mixer channel to use"));
	gtk_widget_set_tooltip_text (mute_on_exit, _("Mute radio device on exit"));

	/* The presets part */
	label = gtk_label_new (NULL);
 	gtk_widget_set_halign (label, GTK_ALIGN_START);
	markup = g_strdup_printf ("<span weight=\"bold\">%s</span>", _("Presets"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 5, 1, 1);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_margin_start (scrolled_window, 10);
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), 75);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_container_add (GTK_CONTAINER (scrolled_window), treeview);

	g_signal_connect (treeview, "key-press-event",
                          G_CALLBACK (treeview_key_press_event_cb), NULL);
	g_signal_connect (treeview, "cursor-changed",
                          G_CALLBACK (treeview_cursor_changed_cb), NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
	g_object_set (renderer, "editable", TRUE, NULL);
	list_column = gtk_tree_view_column_new_with_attributes (NULL, renderer,
                                                                "text", 0,
                                                                NULL);
	gtk_tree_view_column_set_reorderable (list_column, TRUE);
	gtk_tree_view_column_set_expand (list_column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), list_column);

	g_signal_connect (renderer, "edited",
                          G_CALLBACK (name_cell_edited_cb), NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
	g_object_set (renderer, "xalign", 1.0, NULL);
	g_object_set (renderer, "editable", TRUE, NULL);
	list_column = gtk_tree_view_column_new_with_attributes (NULL, renderer,
                                                                "text", 1,
                                                                NULL);
	gtk_tree_view_column_set_reorderable (list_column, TRUE);
	gtk_tree_view_column_set_expand (list_column, FALSE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), list_column);

	g_signal_connect (renderer, "edited",
                          G_CALLBACK (freq_cell_edited_cb), NULL);

	for (l = settings.presets; l != NULL; l = l->next) {
		GtkTreeIter  iter = {0};
		preset      *ps;
		gchar       *buffer;

		ps = l->data;
		buffer = g_strdup_printf ("%0.2f", ps->freq);
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
                                    0, ps->title,
                                    1, buffer,
                                    -1);
		g_free (buffer);
	}

	gtk_grid_attach (GTK_GRID (grid), scrolled_window, 0, 6, 2, 1);

	button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	move_up_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("go-up", GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (move_up_button), image);
	gtk_button_set_relief (GTK_BUTTON (move_up_button), GTK_RELIEF_NONE);
	gtk_widget_set_sensitive (move_up_button, FALSE);
	gtk_widget_set_tooltip_text (move_up_button, _("Move preset up"));

	g_signal_connect (move_up_button, "clicked",
                          G_CALLBACK (move_up_button_clicked_cb), NULL);

	move_down_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("go-down", GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (move_down_button), image);
	gtk_button_set_relief (GTK_BUTTON (move_down_button), GTK_RELIEF_NONE);
	gtk_widget_set_sensitive (move_down_button, FALSE);
	gtk_widget_set_tooltip_text (move_down_button, _("Move preset down"));

	g_signal_connect (move_down_button, "clicked",
                          G_CALLBACK (move_down_button_clicked_cb), NULL);

	add_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (add_button), image);
	gtk_button_set_relief (GTK_BUTTON (add_button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (add_button, _("Add preset"));

	g_signal_connect (add_button, "clicked",
                          G_CALLBACK (add_button_clicked_cb), NULL);

 	remove_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("list-remove", GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (remove_button), image);
	gtk_button_set_relief (GTK_BUTTON (remove_button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (remove_button, _("Remove preset"));
 	gtk_widget_set_sensitive (remove_button, FALSE);

	g_signal_connect (remove_button, "clicked",
                          G_CALLBACK (remove_button_clicked_cb), NULL);

 	save_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("document-save", GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (save_button), image);
	gtk_button_set_relief (GTK_BUTTON (save_button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (save_button, _("Save to file\xE2\x80\xA6"));

	if (settings.presets == NULL)
		gtk_widget_set_sensitive (save_button, FALSE);
	else
		gtk_widget_set_sensitive (save_button, TRUE);

	g_signal_connect (save_button, "clicked",
                          G_CALLBACK (save_presets_to_file_cb), NULL);

 	open_button = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("document-open", GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (open_button), image);
	gtk_button_set_relief (GTK_BUTTON (open_button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (open_button, _("Load from file\xE2\x80\xA6"));
	gtk_widget_set_sensitive (open_button, TRUE);

	g_signal_connect (open_button, "clicked",
                          G_CALLBACK (load_presets_from_file_cb), NULL);

	gtk_box_pack_end (GTK_BOX (button_box), move_down_button, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (button_box), move_up_button, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (button_box), remove_button, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (button_box), add_button, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (button_box), save_button, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (button_box), open_button, FALSE, FALSE, 0);

	gtk_grid_attach (GTK_GRID (grid), button_box, 1, 7, 1, 1);

	/* The record settings part */
	label = gtk_label_new (NULL);
 	gtk_widget_set_halign (label, GTK_ALIGN_START);
	markup = g_strdup_printf ("<span weight=\"bold\">%s</span>", _("Record Settings"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 8, 1, 1);

	label = gtk_label_new (_("Destination:"));
 	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_margin_start (label, 10);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 9, 1, 1);

	destination_button = gtk_file_chooser_button_new (_("Select a folder"),
                                                          GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (destination_button),
                                       rec_settings.destination);
	gtk_grid_attach (GTK_GRID (grid), destination_button, 1, 9, 1, 1);

	label = gtk_label_new (_("File format:"));
 	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_margin_start (label, 10);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 10, 1, 1);

	audio_profile_combo = audio_profile_combo_new ();
	audio_profile_combo_set_active (audio_profile_combo, rec_settings.profile);
	gtk_grid_attach (GTK_GRID (grid), audio_profile_combo, 1, 10, 1, 1);

	install_button = gtk_button_new_with_label (_("Install additional software required to use this format\xE2\x80\xA6"));
	gtk_widget_set_no_show_all (install_button, TRUE);
	gtk_grid_attach (GTK_GRID (grid), install_button, 1, 11, 1, 1);

	profile = get_encoding_profile (rec_settings.profile);
	if (check_missing_plugins (profile, NULL, NULL)) {
		gtk_widget_set_visible (install_button, TRUE);
		gtk_widget_set_sensitive (install_button,
					  gst_install_plugins_supported ());
	} else {
		gtk_widget_set_visible (install_button, FALSE);
	}

	g_signal_connect (destination_button, "selection-changed",
                          G_CALLBACK (destination_button_clicked_cb), NULL);
	g_signal_connect (audio_profile_combo, "changed",
                          G_CALLBACK (audio_profile_combo_change_cb), NULL);
	g_signal_connect (install_button, "clicked",
                          G_CALLBACK (audio_profile_install_plugins_cb), NULL);

	gtk_widget_set_tooltip_text (destination_button, _("The default location to use for the recorded files"));
	gtk_widget_set_tooltip_text (audio_profile_combo, _("The media type for encoding audio when recording"));

	treeview_scroll_to_active_preset_cb (treeview, NULL);

	gtk_widget_show_all (dialog);

	return dialog;
}
