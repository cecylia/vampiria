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

#include <gtk/gtk.h>

#include "messages.h"

static void
show_message (GtkMessageType type, const char *text, const char *details)
{
	GtkWidget *dialog;
	
	g_assert(text);
	
	dialog = gtk_message_dialog_new (NULL,
					 DIALOG_FLAGS,
					 type,
					 GTK_BUTTONS_CLOSE,
					 "%s",
					 text);
	if (details)
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  "%s",
							  details);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}	

void
show_error_message (const char *error, const char *details)
{
	show_message (GTK_MESSAGE_ERROR, error, details);
}	

void
show_warning_message (const char *warning, const char *details)
{
	show_message (GTK_MESSAGE_WARNING, warning, details);
}
