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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gst/gst.h>
#include <gst/pbutils/encoding-profile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "rec_tech.h"
#include "media_types.h"
#include "missing_plugins.h"

GtkWidget *level;

static guint bus_watch_id;

static void show_error_dialog(GtkWindow *win,
                              const gchar *dbg,
                              const gchar * format,
                              ...)
{
	GtkWidget *dialog;
	va_list args;
	gchar *s;

	va_start(args, format);
	s = g_strdup_vprintf(format, args);
	va_end(args);

	dialog = gtk_message_dialog_new(win,
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        "%s",
                                        s);

	if(dbg != NULL)
		g_printerr("ERROR: %s\nDEBUG MESSAGE: %s\n", s, dbg);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	g_free(s);
}

static gboolean bus_watch_cb(GstBus *bus, GstMessage *message, gpointer data)
{
	GError *error = NULL;
	gchar *debug = NULL;

	switch(GST_MESSAGE_TYPE(message))
	{
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(message, &error, &debug);
			g_print("ERROR: %s\nDEBUG MESSAGE: %s\n",
                                error->message,
                                debug);
			g_error_free(error);
			g_free(debug);
			break;
		case GST_MESSAGE_WARNING:
			gst_message_parse_warning(message, &error, &debug);
			g_print("WARNING: %s\nDEBUG MESSAGE: %s\n",
                                error->message,
                                debug);
			g_error_free(error);
			g_free(debug);
			break;
		case GST_MESSAGE_EOS:
			break;
		case GST_MESSAGE_ELEMENT:
		{
			const GstStructure *structure;
			const gchar *name;

			structure = gst_message_get_structure(message);
			name = gst_structure_get_name(structure);

			if(g_strcmp0(name, "level") == 0) {
				gint channels;
				gdouble peak_dB;
				gdouble peak;
				const GValue *arr_value;

				GValueArray *rms_arr, *peak_arr;
				gint i;

				arr_value = gst_structure_get_value(structure,
                                                                    "rms");
				rms_arr =(GValueArray *) g_value_get_boxed(arr_value);

				arr_value = gst_structure_get_value(structure,
                                                                    "peak");
				peak_arr =(GValueArray *) g_value_get_boxed(arr_value);

				channels = rms_arr->n_values;

				for(i = 0; i < channels; ++i) {
					const GValue *value;

					value = peak_arr->values + i;
					peak_dB = g_value_get_double(value);

					peak = pow(10, peak_dB / 20);

					gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(level),
                                                                      peak);
				}
			}
			break;
		}
		default:
			break;
	}

	return TRUE;
}

Recording* recording_start(const char* filename)
{
	GstElement *pipeline;
	GstElement *source;
	GstElement *level;
	GstElement *encodebin;
	GstElement *filesink;
	GstBus *bus;
	GstStateChangeReturn ret;
	GstEncodingProfile *profile;
	GFile *file;
	const gchar *extension;
	gchar *path;

	pipeline = gst_pipeline_new("pipeline");
	g_return_val_if_fail(pipeline != NULL, NULL);

	if(bus_watch_id != 0) {
		g_source_remove(bus_watch_id);
		bus_watch_id = 0;
	}

	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	bus_watch_id = gst_bus_add_watch(bus, bus_watch_cb, pipeline);
	g_object_unref(bus);

	source = gst_element_factory_make("autoaudiosrc", NULL);
	g_return_val_if_fail(source != NULL, NULL);

	ret = gst_element_set_state(source, GST_STATE_READY);
	if(ret == GST_STATE_CHANGE_FAILURE) {
		gst_element_set_state(source, GST_STATE_NULL);
		gst_object_unref(source);
		return NULL;
	}

	level = gst_element_factory_make("level", NULL);
	g_return_val_if_fail(level != NULL, NULL);
	g_object_set(level, "message", TRUE, NULL);

	filesink = gst_element_factory_make("filesink", NULL);
	g_return_val_if_fail(filesink != NULL, NULL);

	profile = get_encoding_profile(rec_settings.profile);
	encodebin = gst_element_factory_make("encodebin", NULL);
	g_return_val_if_fail(encodebin != NULL, NULL);

	g_object_set(encodebin,
		      "profile", profile,
		      "queue-time-max", 120 * GST_SECOND,
		      NULL);

	gst_bin_add_many(GST_BIN(pipeline), source, level, encodebin, filesink, NULL);

	if(!gst_element_link_many(source, level, encodebin, filesink, NULL)) {
		gst_object_unref(pipeline);
		return NULL;
	}

	extension = media_type_to_extension(rec_settings.profile);
	path = g_strdup_printf("%s.%s", filename, extension);

	g_object_set(filesink, "location", path, NULL);

	file = g_file_new_for_path(path);
	g_free(path);

	ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
	if(ret == GST_STATE_CHANGE_FAILURE) {
		gst_element_set_state(pipeline, GST_STATE_NULL);
		gst_object_unref(pipeline);
		return NULL;
	}
	
	Recording *recording;
	recording = g_malloc0(sizeof(Recording));
	recording->file = file;
	recording->pipeline = pipeline;
	
	return recording;
}		

void
recording_stop(Recording *recording)
{
	GstState state;

	if (!recording)
		return;

	gst_element_get_state(recording->pipeline, &state, NULL, GST_CLOCK_TIME_NONE);

	if (state == GST_STATE_PLAYING) {
		GstMessage *msg;
		gst_element_send_event(recording->pipeline, gst_event_new_eos());
		/* wait one second for EOS message on the pipeline bus */
		msg = gst_bus_timed_pop_filtered(GST_ELEMENT_BUS(recording->pipeline),
                                                 GST_SECOND,
                                                 GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
		gst_message_unref(msg);
		gst_element_set_state(recording->pipeline, GST_STATE_NULL);
	}

	if(bus_watch_id != 0) {
		g_source_remove(bus_watch_id);
		bus_watch_id = 0;
	}

	gst_object_unref(recording->pipeline);
	g_object_unref(recording->file);
	g_free(recording->station);
	g_free(recording);
}
