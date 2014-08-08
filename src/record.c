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

#include <config.h>

#include <sys/types.h>
#include <signal.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gui.h"
#include "rec_tech.h"
#include "prefs.h"
#include "trayicon.h"

extern GtkWidget *level;

static GtkWidget *status_dialog;
static GtkWidget *file_lbl;
static GtkWidget *type_lbl;
static GtkWidget *size_lbl;
static GtkWidget *length_lbl;

static int timeout_id = -1;

void close_status_window(void)
{
	if (timeout_id >= 0) {
		g_source_remove(timeout_id);
		timeout_id = -1;
	}

	if(status_dialog) {
		gtk_widget_destroy(GTK_WIDGET(status_dialog));
		status_dialog = NULL;
	}
	
	tray_menu_items_set_sensible(TRUE);
	recording_set_sensible(TRUE);
}

static char *seconds_to_full_string(guint seconds)
{
	long days;
	long hours;
	long minutes;
	char *time = NULL;
	const char *minutefmt;
	const char *hourfmt;
	const char *secondfmt;

	days    = seconds /(60 * 60 * 24);
	hours   =(seconds /(60 * 60));
	minutes =(seconds / 60) -((days * 24 * 60) +(hours * 60));
	seconds = seconds % 60;

	minutefmt = ngettext("%ld minute", "%ld minutes", minutes);
	hourfmt = ngettext("%ld hour", "%ld hours", hours);
	secondfmt = ngettext("%ld second", "%ld seconds", seconds);

	if (hours > 0) {
		if (minutes > 0)
			if (seconds > 0) {
				char *fmt;
				/* Translators: the format is "X hours X minutes X seconds" */
				fmt = g_strdup_printf(_("%s %s %s"),
                                                      hourfmt,
                                                      minutefmt,
                                                      secondfmt);
				time = g_strdup_printf(fmt,
                                                       hours,
                                                       minutes,
                                                       seconds);
				g_free(fmt);
			} else {
				char *fmt;
				/* Translators: the format is "X hours X minutes" */
				fmt = g_strdup_printf(_("%s %s"),
                                                      hourfmt,
                                                      minutefmt);
				time = g_strdup_printf(fmt, hours, minutes);
				g_free(fmt);
			}
		else
			if (seconds > 0) {
				char *fmt;
				/* Translators: the format is "X minutes X seconds" */
				fmt = g_strdup_printf(_("%s %s"),
                                                      minutefmt,
                                                      secondfmt);
				time = g_strdup_printf(fmt, minutes, seconds);
				g_free(fmt);
			} else {
				time = g_strdup_printf(minutefmt, minutes);
			}
	} else {
		if (minutes > 0) {
			if (seconds > 0) {
				char *fmt;
				/* Translators: the format is "X minutes X seconds" */
				fmt = g_strdup_printf(_("%s %s"),
                                                      minutefmt,
                                                      secondfmt);
				time = g_strdup_printf(fmt, minutes, seconds);
				g_free(fmt);
			} else {
				time = g_strdup_printf(minutefmt, minutes);
			}

		} else {
			time = g_strdup_printf(secondfmt, seconds);
		}
	}

	return time;
}

static gboolean timeout_cb(gpointer data)
{
	Recording *recording = data;

	g_assert(recording);

	if(!gtk_widget_get_visible(status_dialog))
		gtk_widget_show_all(status_dialog);

	GFileInfo *info;

	info = g_file_query_info(recording->file,
				  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				  G_FILE_ATTRIBUTE_STANDARD_SIZE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  NULL);

	if(info != NULL) {
		const gchar *display_name, *content_type;
		gchar *description, *mime, *type, *size;
		gint64 file_size;

		display_name = g_file_info_get_display_name(info);

		gtk_label_set_text(GTK_LABEL(file_lbl), display_name);

		content_type = g_file_info_get_content_type(info);
		description = g_content_type_get_description(content_type);
		mime = g_content_type_get_mime_type(content_type);
		type = g_strdup_printf("%s(%s)", description, mime);

		gtk_label_set_text(GTK_LABEL(type_lbl), type);

		g_free(description);
		g_free(mime);
		g_free(type);

		file_size = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_STANDARD_SIZE);

		size = g_format_size_full(file_size, G_FORMAT_SIZE_LONG_FORMAT);
		gtk_label_set_text(GTK_LABEL(size_lbl), size);

		g_free(size);

		g_object_unref(info);

		gint64 position;

		if(gst_element_query_position(recording->pipeline, GST_FORMAT_TIME, &position)) {
			gchar* length;
			gint secs;

			secs = position / GST_SECOND;
			length = seconds_to_full_string(secs);

			gtk_label_set_text(GTK_LABEL(length_lbl), length);

			g_free(length);
		}
	}
	
	return TRUE;
}	
	
void run_status_window(Recording *recording)
{
	timeout_id = g_timeout_add(500,(GSourceFunc) timeout_cb, recording);
}

void stop_record_button_clicked_cb(GtkButton *button, gpointer data)
{
	Recording *recording = data;
	close_status_window();
	recording_stop(recording);
}		

static gint delete_event_cb(GtkWidget* window, GdkEventAny* e, gpointer data)
{
	stop_record_button_clicked_cb(NULL, data);
	return TRUE;
}

GtkWidget* record_status_window(Recording *recording)
{
	GtkWidget *vbox;
	GtkWidget *grid;
	GtkWidget *image;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *expander;
	GtkWidget *button;
	gchar *text, *markup;

	status_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(status_dialog),
                             _("Gnomeradio recording status"));
	gtk_window_set_resizable(GTK_WINDOW(status_dialog), FALSE);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

 	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 15);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 5);

	image = gtk_image_new_from_icon_name("gnomeradio", GTK_ICON_SIZE_DIALOG);
	gtk_image_set_pixel_size(GTK_IMAGE(image), 42);
	gtk_widget_set_valign(image, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), image, 0, 0, 1, 3);

	label = gtk_label_new(NULL);
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	text = g_markup_printf_escaped(_("Recording from station %s"),
                                       recording->station);
	markup = g_strdup_printf("<span size=\"larger\" weight=\"bold\">%s</span>",
                                 text);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(text);
	g_free(markup);
	gtk_grid_attach(GTK_GRID(grid), label, 1, 0, 1, 1);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	level = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(box), level, FALSE, TRUE, 0);
	gtk_grid_attach(GTK_GRID(grid), box, 1, 1, 1, 1);

	expander = gtk_expander_new(_("Details"));
	gtk_grid_attach(GTK_GRID(grid), expander, 1, 2, 1, 1);

	gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);

	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 15);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 5);

	label = gtk_label_new(_("Name:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

	label = gtk_label_new(_("Type:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);

	label = gtk_label_new(_("Size:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);

	label = gtk_label_new(_("Length:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 1, 1);

	file_lbl = gtk_label_new(NULL);
	gtk_label_set_ellipsize(GTK_LABEL(file_lbl), PANGO_ELLIPSIZE_START);
	gtk_widget_set_halign(file_lbl, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), file_lbl, 1, 0, 1, 1);

	type_lbl = gtk_label_new(NULL);
	gtk_widget_set_halign(type_lbl, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), type_lbl, 1, 1, 1, 1);

	size_lbl = gtk_label_new(NULL);
	gtk_widget_set_halign(size_lbl, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), size_lbl, 1, 2, 1, 1);

	length_lbl = gtk_label_new(NULL);
 	gtk_widget_set_halign(length_lbl, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), length_lbl, 1, 3, 1, 1);

	gtk_container_add(GTK_CONTAINER(expander), grid);

	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	button = gtk_button_new();
	label = gtk_label_new(_("Stop Recording"));
	image = gtk_image_new_from_icon_name("process-stop",
                                             GTK_ICON_SIZE_BUTTON);
	
	gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 2);

	gtk_container_add(GTK_CONTAINER(button), box);
	
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	gtk_box_pack_end(GTK_BOX(box), button, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(status_dialog), vbox);
	gtk_widget_grab_focus(button);

	g_signal_connect(button,
                         "clicked",
                         G_CALLBACK(stop_record_button_clicked_cb),
                         recording);
	g_signal_connect(status_dialog,
                         "delete_event",
                         G_CALLBACK(delete_event_cb),
                         recording);
	g_signal_connect(status_dialog,
                         "key-press-event",
                         G_CALLBACK(key_press_event_cb),
                         recording);

	gtk_window_set_position(GTK_WINDOW(status_dialog), GTK_WIN_POS_CENTER);

	return status_dialog;
}
