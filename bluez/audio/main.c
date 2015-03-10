/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2006-2010  Nokia Corporation
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <glib.h>
#include <dbus/dbus.h>

#include "glib-helper.h"
#include "btio.h"
#include "plugin.h"
#include "log.h"
#include "device.h"
#include "headset.h"
#include "manager.h"
#include "gateway.h"
#include "wakelock.h"
#include "main.h"

#define AUDIO_WAKELOCK_DURATION 5

static GIOChannel *sco_server = NULL;

static struct wakelock *audio_wakelock = NULL;

static guint audio_wakelock_source = 0;

static gboolean audio_wakelock_put(gpointer user_data)
{
	wakelock_release(audio_wakelock);
	audio_wakelock_source = 0;
	return FALSE;
}

void audio_wakelock_get(void)
{
	guint old_source = audio_wakelock_source;

	audio_wakelock_source = g_timeout_add_seconds(AUDIO_WAKELOCK_DURATION,
							audio_wakelock_put,
							NULL);
	if (audio_wakelock_source)
		wakelock_acquire(audio_wakelock);

	if (old_source) {
		g_source_remove(old_source);
		wakelock_release(audio_wakelock);
	}
}

static GKeyFile *load_config_file(const char *file)
{
	GError *err = NULL;
	GKeyFile *keyfile;

	keyfile = g_key_file_new();

	g_key_file_set_list_separator(keyfile, ',');

	if (!g_key_file_load_from_file(keyfile, file, 0, &err)) {
		error("Parsing %s failed: %s", file, err->message);
		g_error_free(err);
		g_key_file_free(keyfile);
		return NULL;
	}

	return keyfile;
}

static void sco_server_cb(GIOChannel *chan, GError *err, gpointer data)
{
	int sk;
	struct audio_device *device;
	char addr[18];
	bdaddr_t src, dst;

	if (err) {
		error("sco_server_cb: %s", err->message);
		return;
	}

	bt_io_get(chan, BT_IO_SCO, &err,
			BT_IO_OPT_SOURCE_BDADDR, &src,
			BT_IO_OPT_DEST_BDADDR, &dst,
			BT_IO_OPT_DEST, addr,
			BT_IO_OPT_INVALID);
	if (err) {
		error("bt_io_get: %s", err->message);
		goto drop;
	}

	device = manager_find_device(NULL, &src, &dst, AUDIO_HEADSET_INTERFACE,
					FALSE);
	if (!device)
		device = manager_find_device(NULL, &src, &dst,
						AUDIO_GATEWAY_INTERFACE,
						FALSE);

	if (!device)
		goto drop;

	if (device->headset) {
		if (headset_get_state(device) < HEADSET_STATE_CONNECTED) {
			DBG("Refusing SCO from non-connected headset");
			goto drop;
		}

		if (!headset_get_hfp_active(device)) {
			error("Refusing non-HFP SCO connect attempt from %s",
									addr);
			goto drop;
		}

		if (headset_connect_sco(device, chan) < 0)
			goto drop;

		headset_set_state(device, HEADSET_STATE_PLAYING);
	} else if (device->gateway) {
		if (!gateway_is_connected(device)) {
			DBG("Refusing SCO from non-connected AG");
			goto drop;
		}

		if (gateway_connect_sco(device, chan) < 0)
			goto drop;
	} else
		goto drop;

	sk = g_io_channel_unix_get_fd(chan);
	fcntl(sk, F_SETFL, 0);

	DBG("Accepted SCO connection from %s", addr);

	return;

drop:
	g_io_channel_shutdown(chan, TRUE, NULL);
}

static DBusConnection *connection;

static int audio_init(void)
{
	GKeyFile *config;
	gboolean enable_sco;

	connection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (connection == NULL)
		return -EIO;

	if (wakelock_create("audio", &audio_wakelock) < 0)
		goto failed;

	config = load_config_file(CONFIGDIR "/audio.conf");

	if (audio_manager_init(connection, config, &enable_sco) < 0)
		goto failed;

	if (!enable_sco)
		return 0;

	sco_server = bt_io_listen(BT_IO_SCO, sco_server_cb, NULL, NULL,
					NULL, NULL,
					BT_IO_OPT_INVALID);
	if (!sco_server) {
		error("Unable to start SCO server socket");
		goto failed;
	}

	return 0;

failed:
	audio_manager_exit();

	if (audio_wakelock) {
		wakelock_free(audio_wakelock);
		audio_wakelock = NULL;
	}

	if (connection) {
		dbus_connection_unref(connection);
		connection = NULL;
	}

	return -EIO;
}

static void audio_exit(void)
{
	if (sco_server) {
		g_io_channel_shutdown(sco_server, TRUE, NULL);
		g_io_channel_unref(sco_server);
		sco_server = NULL;
	}

	audio_manager_exit();

	if (audio_wakelock) {
		if (audio_wakelock_source) {
			g_source_remove(audio_wakelock_source);
			audio_wakelock_source = 0;
		}
		wakelock_free(audio_wakelock);
		audio_wakelock = NULL;
	}

	dbus_connection_unref(connection);
}

BLUETOOTH_PLUGIN_DEFINE(audio, VERSION,
			BLUETOOTH_PLUGIN_PRIORITY_DEFAULT, audio_init, audio_exit)
