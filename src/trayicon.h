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

#ifndef _TRAYICON_H
#define _TRAYICON_H

#include "gui.h"

void tray_icon_set_title(gchar *title);

void tray_menu_items_set_sensible(gboolean sensible);

void tray_menu_enable_mute_button(gboolean enable);

void tray_menu_add_preset(preset *ps, gint index);

void tray_menu_remove_preset(gint index);

void tray_menu_move_up_preset(preset *ps, gint index);

void tray_menu_move_down_preset(preset *ps, gint index);

void tray_menu_update_preset(preset *ps, gint index);

void create_tray_icon(GtkWidget *app);

void create_tray_menu(GtkWidget *app);

#endif
