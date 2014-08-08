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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gui.h"
#include "trayicon.h"
#include "mixer.h"

extern gnomeradio_settings settings;

static GtkWidget     *mute_menuitem;
static GtkWidget     *showwindow_menuitem;
static GtkWidget     *tray_menu;
static GtkStatusIcon *tray_icon;

int mute_menuitem_toggled_cb_id;

void
tray_icon_set_title (gchar *title)
{
        if (tray_icon)
                gtk_status_icon_set_tooltip_text (GTK_STATUS_ICON (tray_icon),
                                                  title);
}

void
tray_menu_items_set_sensible (gboolean sensible)
{
	GList *menuitems;
	GtkWidget *menuitem;
	int count;
	int index;


	menuitems = gtk_container_get_children (GTK_CONTAINER (tray_menu));
	count = g_list_length (settings.presets);

	g_assert (count + 6 == g_list_length (menuitems));

	for (index = 0; index < count; index++) {
		menuitem = g_list_nth_data (menuitems, index);
		gtk_widget_set_sensitive (menuitem, sensible);
	}

	menuitem = g_list_nth_data (menuitems, count + 1);
	gtk_widget_set_sensitive (menuitem, sensible);

	menuitem = g_list_nth_data (menuitems, count + 2);
	gtk_widget_set_sensitive (menuitem, sensible);

	menuitem = g_list_nth_data (menuitems, count + 5);
	gtk_widget_set_sensitive (menuitem, sensible);
}

void
tray_menu_enable_mute_button (gboolean enable)
{
	if (tray_menu) {
		g_signal_handler_block (mute_menuitem,
                                        mute_menuitem_toggled_cb_id);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mute_menuitem),
                                                enable);
		g_signal_handler_unblock (mute_menuitem,
                                          mute_menuitem_toggled_cb_id);
	}
}

void tray_menu_add_preset(preset *ps, gint index)
{
	GtkWidget *menuitem;

	menuitem = gtk_menu_item_new_with_label (ps->title);

	gtk_menu_shell_insert (GTK_MENU_SHELL (tray_menu), menuitem, index);

	g_signal_connect (menuitem,
                          "activate",
                          G_CALLBACK (preset_menuitem_activate_cb),
                          GINT_TO_POINTER (index));

	gtk_widget_show (menuitem);
}

void
tray_menu_remove_preset (gint index)
{
	GList     *menuitems;
	GtkWidget *menuitem;

	menuitems = gtk_container_get_children (GTK_CONTAINER (tray_menu));
	g_assert (index < g_list_length (menuitems));
	menuitem = g_list_nth_data (menuitems, index);
	gtk_widget_destroy (menuitem);
}

void
tray_menu_move_up_preset (preset *ps,
                          gint    index)
{
	GList     *menuitems;
	GtkWidget *menuitem;

	menuitems = gtk_container_get_children (GTK_CONTAINER (tray_menu));
	g_assert (index < g_list_length (menuitems));
	menuitem = g_list_nth_data (menuitems, index);

	gtk_menu_reorder_child (GTK_MENU(tray_menu),
                                menuitem,
                                GPOINTER_TO_INT (index + 1));

	g_signal_connect(menuitem,
                         "activate",
                         G_CALLBACK (preset_menuitem_activate_cb),
                         GINT_TO_POINTER (index + 1));

	menuitems = gtk_container_get_children (GTK_CONTAINER (tray_menu));
	menuitem = g_list_nth_data (menuitems, index);

	g_signal_connect (menuitem,
                          "activate",
                          G_CALLBACK (preset_menuitem_activate_cb),
                          GINT_TO_POINTER (index));
}

void
tray_menu_move_down_preset(preset *ps,
                           gint    index)
{
	GList     *menuitems;
	GtkWidget *menuitem;

	menuitems = gtk_container_get_children (GTK_CONTAINER (tray_menu));
	g_assert(index < g_list_length (menuitems));
	menuitem = g_list_nth_data (menuitems, index);

	gtk_menu_reorder_child (GTK_MENU (tray_menu),
                               menuitem,
                               GPOINTER_TO_INT (index - 1));

	g_signal_connect (menuitem,
                          "activate",
                          G_CALLBACK (preset_menuitem_activate_cb),
                          GINT_TO_POINTER (index - 1));

	menuitems = gtk_container_get_children (GTK_CONTAINER (tray_menu));
	menuitem = g_list_nth_data (menuitems, index);

	g_signal_connect (menuitem,
                          "activate",
                          G_CALLBACK (preset_menuitem_activate_cb),
                          GINT_TO_POINTER (index));
}

void
tray_menu_update_preset (preset *ps,
                         gint    index)
{
	tray_menu_remove_preset (index);
	tray_menu_add_preset (ps, index);
}

static void
mute_menuitem_toggled_cb (GtkCheckMenuItem *checkmenuitem,
                          gpointer user_data)
{
	toggle_volume ();
}

static void
record_menuitem_activate_cb (GtkMenuItem *menuitem,
                             gpointer user_data)
{
	record_button_clicked_cb (NULL, user_data);
}

static void
showwindow_menuitem_toggled_cb (GtkCheckMenuItem *checkmenuitem,
                                gpointer          user_data)
{
	GtkWidget *app = GTK_WIDGET(user_data);
	toggle_mainwindow_visibility(app);
}

static void
quit_menuitem_activate_cb (GtkMenuItem *menuitem,
                           gpointer user_data)
{
	exit_gnomeradio();
}

void
preset_menuitem_activate_cb (GtkMenuItem *menuitem,
                             gpointer     user_data)
{
	switch_to_preset (GPOINTER_TO_INT (user_data));
}

void
create_tray_menu (GtkWidget *app)
{
	GtkWidget *menuitem;
	GList *node;
	int index;
	
	node = settings.presets;
	tray_menu = gtk_menu_new();

	for (index = 0; node; index++, node = node->next) {
		preset *ps;

		ps = (preset *) node->data;
		menuitem = gtk_menu_item_new_with_label (ps->title);
		
		gtk_menu_shell_insert (GTK_MENU_SHELL (tray_menu), menuitem, index);
		
		g_signal_connect (menuitem,
                                  "activate",
                                  G_CALLBACK(preset_menuitem_activate_cb),
                                  GINT_TO_POINTER (index));

		gtk_widget_show (menuitem);
	}
	
	gtk_menu_shell_append (GTK_MENU_SHELL (tray_menu),
                               gtk_separator_menu_item_new ());

	mute_menuitem = gtk_check_menu_item_new_with_label (_("Muted"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mute_menuitem),
                                        mixer->get_volume() == 0);
	gtk_menu_shell_append (GTK_MENU_SHELL (tray_menu), mute_menuitem);

	mute_menuitem_toggled_cb_id = g_signal_connect (mute_menuitem,
                                                        "toggled",
                                                        G_CALLBACK (mute_menuitem_toggled_cb),
                                                        (gpointer) app);
	gtk_widget_show (mute_menuitem);

	menuitem = gtk_menu_item_new_with_mnemonic (_("_Record"));
	gtk_menu_shell_append (GTK_MENU_SHELL(tray_menu), menuitem);
	g_signal_connect (menuitem,
                          "activate",
                          G_CALLBACK(record_menuitem_activate_cb),
                          app);
	gtk_widget_show (menuitem);

	gtk_menu_shell_append (GTK_MENU_SHELL (tray_menu),
                               gtk_separator_menu_item_new ());
	
	showwindow_menuitem = gtk_check_menu_item_new_with_label (_("Show Window"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (showwindow_menuitem),
                                        TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (tray_menu), showwindow_menuitem);
	g_signal_connect (showwindow_menuitem,
                          "activate",
                          G_CALLBACK (showwindow_menuitem_toggled_cb),
                          (gpointer) app);
	gtk_widget_show(showwindow_menuitem);

	menuitem = gtk_menu_item_new_with_mnemonic (_("_Quit"));
	gtk_menu_shell_append (GTK_MENU_SHELL (tray_menu), menuitem);
	g_signal_connect (menuitem,
                          "activate",
                          G_CALLBACK(quit_menuitem_activate_cb),
                          NULL);
	gtk_widget_show (menuitem);

	gtk_widget_show_all (tray_menu);
}

static void
tray_activate_cb (GtkStatusIcon *icon,
                  gpointer       data)
{
	gboolean active;

	active = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (showwindow_menuitem));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (showwindow_menuitem),
					!active);
}

static void
tray_popup_menu (GtkStatusIcon *icon,
                 guint          button,
                 guint32        time,
                 gpointer       data)
{
	gtk_menu_popup(GTK_MENU(tray_menu), NULL, NULL, NULL, icon, button, time);
}	

void create_tray_icon(GtkWidget *app)
{
	GtkIconTheme *icontheme;
	GdkPixbuf *pixbuf;

	
	icontheme = gtk_icon_theme_get_default ();
	pixbuf = gtk_icon_theme_load_icon (icontheme, "gnomeradio", 16, 0, NULL);
	g_return_if_fail(pixbuf);
	tray_icon = gtk_status_icon_new_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);

	g_signal_connect (tray_icon,
                          "activate",
                          G_CALLBACK (tray_activate_cb),
                          (gpointer) app);
	g_signal_connect (tray_icon,
                          "popup-menu",
                          G_CALLBACK (tray_popup_menu),
                          (gpointer) app);
}
