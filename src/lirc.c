/*
 * Copyright © 2001  Jörgen Scheibengruber  <mfcn@gmx.de>
 * Copyright © 2014  POJAR GEORGE  <geoubuntu@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or  (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#ifdef HAVE_LIRC

#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <lirc/lirc_client.h>

#include "lirc.h"
#include "gui.h"

extern GtkWidget *app;
extern int        verbose;

static int    fd = -1;
static struct lirc_config *config = NULL;

static void
execute_lirc_command (char *cmd)
{
	if (verbose)
		fprintf (stderr,"lirc: cmd %s\n", cmd);

	if (strcasecmp (cmd, "tune up") == 0)
		scfw_button_clicked_cb (NULL, NULL);
	else if (strcasecmp (cmd, "tune down") == 0)
		scbw_button_clicked_cb (NULL, NULL);
	else if (strcasecmp (cmd, "volume up") == 0) 
		volume_up ();
	else if (strcasecmp (cmd, "volume down") == 0) 
		volume_down ();
	else if (strcasecmp (cmd, "mute") == 0)
		toggle_volume ();
	else if (strcasecmp (cmd, "tv") == 0) {
		guint i;
		const char *tv_app[] = {
			"tvtime",
			"xawtv",
			"zapping"
		};

		exit_gnomeradio ();
		for (i = 0; i < G_N_ELEMENTS (tv_app); i++) {
			gchar **argv;

			g_shell_parse_argv (tv_app[i], NULL, &argv, NULL);
			if (g_spawn_async (NULL,
                                           argv,
                                           NULL,
                                           G_SPAWN_SEARCH_PATH,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL))
				break;
			g_strfreev  (argv);
		}
	}
	else if (strcasecmp (cmd, "quit") == 0)
		exit_gnomeradio ();
	else if (strcasecmp (cmd, "preset up") == 0)
		change_preset (TRUE);
	else if (strcasecmp (cmd, "preset down") == 0)
		change_preset (FALSE);
	else if (strcasecmp (cmd, "toggle visible") == 0)
		toggle_mainwindow_visibility (app);
	else if (strncasecmp (cmd, "preset ", 7) == 0) {
		int tmp = 0;
		if (sscanf (cmd, "preset %i", &tmp))
			switch_to_preset (tmp);
	}
	else
		if (verbose)
    			fprintf (stderr, "unrecognized lirccmd: %s\n", cmd);
}

static char *
map_code_to_default (char *code)
{
	char         event[21];
	unsigned int dummy, repeat = 0;
	int          key = 0;
	
	if (sscanf (code,"%x %x %20s", &dummy, &repeat, event) != 3) {
		if (verbose)
			fprintf (stderr,"lirc: oops, parse error: %s\n", code);
		return NULL;
	}
	
	if (!repeat && !strcasecmp ("KEY_KPPLUS", event))
		return (char*)strdup ("tune up");
	else if (!repeat && !strcasecmp ("KEY_KPMINUS", event))
		return (char*)strdup ("tune down");
	else if (!strcasecmp ("KEY_VOLUMEUP", event))
		return (char*)strdup ("volume up");
	else if (!strcasecmp ("KEY_VOLUMEDOWN", event))
		return (char*)strdup ("volume down");
	else if (!repeat && !strcasecmp ("KEY_MUTE", event))
		return (char*)strdup ("mute");
	else if (!repeat && !strcasecmp ("KEY_TUNER", event))
		return (char*)strdup ("tv");
	else if (!repeat && !strcasecmp ("KEY_POWER", event))
		return (char*)strdup ("quit");
	else if (!repeat && !strcasecmp ("KEY_CHANNELUP", event))
		return (char*)strdup ("preset up");
	else if (!repeat && !strcasecmp ("KEY_CHANNELDOWN", event))
		return (char*)strdup ("preset down");
	else if (!repeat && !strcasecmp ("KEY_TEXT", event))
		return (char*)strdup ("toggle visible");
	
	if (sscanf (event, "KEY_%d", &key) == 1) {
		char *ret;
		if (repeat ||  (key < 0) ||  (key > 9))
			return NULL;
		ret = malloc (strlen ("preset xx"));
		sprintf (ret, "preset %1.1d", key);
		if (verbose)  
			fprintf (stderr,"lirc: key=%s repeat=%d\n", event+9, repeat);  
		return ret;
	}
	
	return NULL;
}
	
int
init_lirc (void)
{
	if (verbose)
		fprintf (stderr, "Setting up LIRC support...\n");

	if ((fd = lirc_init ("gnomeradio", 0)) <= 0) {
		if (verbose)
			fprintf (stderr, "Failed to open LIRC support. "
                                 "You will not be able to use remote control.\n");
		return 0;
	}
  
	if (lirc_readconfig  (NULL, &config, NULL) != 0) {
		config = NULL;
		if (verbose)  
			fprintf (stderr, "lirc: configfile  (~/.lircrc) could not be opened.\n"
                                 "Using default config.\n");
	}
  
	fcntl (fd, F_SETFL, O_NONBLOCK);
	fcntl (fd, F_SETFD, FD_CLOEXEC);
  
	return 1;
}

void
deinit_lirc (void)
{
	if (fd <= 0)
		return;

	lirc_freeconfig (config);
	lirc_deinit  ();
}	

static gboolean
lirc_has_data_cb (GIOChannel   *source,
                  GIOCondition  condition,
                  gpointer      data)
{
	char *code, *cmd;
	int   ret = -1;
  
	while  (lirc_nextcode  (&code) == 0 && code != NULL) {
		ret = 0;
		if (config) {
			while  (lirc_code2char  (config, code, &cmd) == 0 &&
                               cmd != NULL)
				execute_lirc_command (cmd);
		} else {
			cmd = map_code_to_default (code);
			if (cmd) {
				execute_lirc_command (cmd);
				free (cmd);
			}
		}	
		free  (code);
	}

	if (ret == -1) {
		if (verbose)
			fprintf (stderr, "lirc: an lirc error occured.\n");
		lirc_freeconfig  (config);
		config = NULL;
		lirc_deinit  ();
		return FALSE;
	}

	return TRUE;
}

void
start_lirc (void)
{
	GIOChannel *ioc;

	if (fd < 0)
		return;

	ioc = g_io_channel_unix_new (fd);
	g_io_add_watch (ioc, G_IO_IN, lirc_has_data_cb, NULL);
}

#endif
