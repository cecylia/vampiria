/*
 * Copyright © 2007  Tim-Philipp Müller  <tim@centricular.net>
 * Copyright © 2007  Jonathan Matthew  <jonathan@d14n.org>
 * Copyright © 2014  POJAR GEORGE  <geoubuntu@gmail.com>
 * 
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or(at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MISSING_PLUGINS_H
#define _MISSING_PLUGINS_H

#include <gtk/gtk.h>

void missing_plugins_init(GtkWindow *parent_window);

gboolean missing_plugins_install(const char **details,
                                 gboolean ignore_blacklist,
                                 GClosure *closure);

#endif
