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

#ifndef _MESSAGES_H
#define _MESSAGES_H

#define DIALOG_FLAGS (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT)

/**
 * show_error_message() - Show an error message to the user
 *
 * @error:		error message
 * @details:		extra message details
 */
void
show_error_message(const gchar* error, const gchar* details);

/**
 * show_warning_message() - Show a warning message to the user
 *
 * @error:		warning message
 * @details:		extra message details
 */
void
show_warning_message(const gchar* warning, const gchar* details);

#endif
