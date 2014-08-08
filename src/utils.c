/*
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

#include <glib.h>

#include "utils.h"

GList *
g_list_swap_next (GList *list, GList *l)
{
	GList *t;

	if (!l)
		return list;

	if (!l->next)
		return list;

	if (l->prev)
		l->prev->next = l->next;
	t = l->prev;
	l->prev = l->next;
	l->next->prev = t;
	if (l->next->next)
		l->next->next->prev = l;
	t = l->next->next;
	l->next->next = l;
	l->next = t;

	if (list == l)
		return l->prev;

	return list;
}

GList *
g_list_swap_prev (GList *list, GList *l)
{
	GList *t;

	if (!l)
		return list;

	if (!l->prev)
		return list;

	if (l->next)
		l->next->prev = l->prev;
	t = l->next;
	l->next = l->prev;
	l->prev->next = t;
	if (l->prev->prev)
		l->prev->prev->next = l;
	t = l->prev->prev;
	l->prev->prev = l;
	l->prev = t;

	if (list == l->next)
		return l;

	return list;
}
