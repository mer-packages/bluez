/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2015 Jolla Ltd. All rights reserved.
 *  Contact: Hannu Mallat <hannu.mallat@jollamobile.com>
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

/* Plugin that implements wakelocks. Only one real wakelock is ever
   used; API can create several "virtual" wakelocks which are mapped
   on the real one: 

   - no virtual wakelock active -> real wakelock not active
   - at least one virtual wakelock active -> real wakelock active
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib.h>

#include "plugin.h"
#include "log.h"
#include "wakelock.h"

#define LOCK_FILE	"/sys/power/wake_lock"
#define UNLOCK_FILE	"/sys/power/wake_unlock"
#define LOCK_NAME	"bluetoothd"
#define LOCK_NAME_LEN   10

struct wakelock {
	gchar *name;
	gsize acquisitions;
	gboolean stale;
};

static GSList *locks = NULL;

static gboolean initialized = FALSE;

static gboolean real_lock_acquired = FALSE;

static int sysfs_write(const gchar *file, const gchar *data_ptr, gsize data_len,
			gboolean silent)
{
	int fd = -1;
	int r;

	fd = open(file, O_WRONLY);
	if (fd < 0) {
		if (!silent)
			error("Failed to open: %s(%d)", strerror(errno), errno);
		r = -errno;
		goto out;
	}

	if (write(fd, data_ptr, data_len) != data_len) {
		if (!silent)
			error("Atomic write failed");
		r = -EIO;
		goto out;
	}

	r = 0;

out:
	if (fd >= 0)
		close(fd);

	return r;
}

static int jolla_wakelock_acquire(struct wakelock *lock)
{
	int r;

	DBG("lock: %p, name: %s, acquisitions: %u, stale: %s",
		lock, lock->name, lock->acquisitions,
		lock->stale ? "yes" : "no");

	if (lock->stale)
		return -EBADF;

	if (real_lock_acquired)
		goto done;

	r = sysfs_write(LOCK_FILE, LOCK_NAME, LOCK_NAME_LEN, FALSE);
	if (r < 0) {
		error("Failed to acquire real lock for lock: %p, name %s",
			lock, lock->name);
		return r;
	}

	real_lock_acquired = TRUE;
	DBG("Real wakelock acquired for lock: %p, name %s", lock, lock->name);

done:
	lock->acquisitions++;
	DBG("lock: %p, name %s acquired", lock, lock->name);

	return 0;
}

static int jolla_wakelock_release(struct wakelock *lock)
{
	GSList *l;
	int r;

	DBG("lock: %p, name: %s, acquisitions: %u, stale: %s",
		lock, lock->name, lock->acquisitions,
		lock->stale ? "yes" : "no");

	if (lock->stale)
		return -EBADF;

	if (!lock->acquisitions) {
		warn("Attempted to release already released lock: %p", lock);
		return -EINVAL;
	}

	lock->acquisitions--;
	if (lock->acquisitions)
		goto done;

	/* See if this was the last active lock, only release real lock if so */
	for (l = locks; l; l = l->next) {
		struct wakelock *check = l->data;
		if (check->acquisitions)
			goto done;
	}

	r = sysfs_write(UNLOCK_FILE, LOCK_NAME, LOCK_NAME_LEN, FALSE);
	if (r < 0)
		warn("Failed to release real lock for lock: %p, name: %s",
			lock, lock->name);

	real_lock_acquired = FALSE;
	DBG("Real wakelock released for lock: %p, name %s", lock, lock->name);

done:
	DBG("lock: %p, name %s released", lock, lock->name);
	return 0;
}

static int jolla_wakelock_create(const gchar *name, struct wakelock **lock)
{
	DBG("name: %s", name);

	*lock = g_new0(struct wakelock, 1);
	(*lock)->name = g_strdup(name);
	(*lock)->acquisitions = 0;
	(*lock)->stale = FALSE;

	locks = g_slist_prepend(locks, *lock);

	DBG("lock: %p created", *lock);
	return 0;
}

static int jolla_wakelock_free(struct wakelock *lock)
{
	DBG("lock: %p, name: %s, acquisitions: %u, stale: %s",
		lock, lock->name, lock->acquisitions,
		lock->stale ? "yes" : "no");

	if (lock->acquisitions) { /* Force release */
		warn("Freeing unreleased lock: %p", lock);
		lock->acquisitions = 1;
		jolla_wakelock_release(lock);
	}

	locks = g_slist_remove(locks, lock);
	g_free(lock->name);
	g_free(lock);

	DBG("lock: %p freed", lock);
	return 0;
}

static void jolla_wakelock_exit(void)
{
	DBG("");

	if (!initialized)
		return;

	/* May happen due to plugin exit ordering */
	if (locks) {
		DBG("Marking remaining wakelocks as stale");
		while (locks) {
			struct wakelock *lock = locks->data;
			if (lock->acquisitions) { /* Force release */
				lock->acquisitions = 1;
				jolla_wakelock_release(lock);
			}
			lock->stale = TRUE;
			locks = g_slist_remove(locks, lock);
		}
	}

	wakelock_plugin_unregister();
	initialized = FALSE;
	DBG("Wakelocks uninitialized");

	/* Sanity: force release just in case our internal state got messed up */
	sysfs_write(UNLOCK_FILE, LOCK_NAME, LOCK_NAME_LEN, TRUE);
}

static GKeyFile *load_config(const char *file)
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

static int jolla_wakelock_init(void)
{
	static struct wakelock_table table = {
		.create = jolla_wakelock_create,
		.free = jolla_wakelock_free,
		.acquire = jolla_wakelock_acquire,
		.release = jolla_wakelock_release
	};
	gboolean use_wakelocks = FALSE;
	GKeyFile *config;
	int r;

	DBG("");

	if (initialized)
		return -EALREADY;

	config = load_config(CONFIGDIR "/jolla.conf");
	if (config) {
		use_wakelocks = g_key_file_get_boolean(config,
							"General",
							"Wakelocks",
							NULL);
		g_key_file_free(config);
	}

	if (!use_wakelocks) /* No need to initialize or uninitialize */
		return 0;

	/* Sanity: release any left over wakelock from previous processes
	   (e.g. after a crash-restart) */
	sysfs_write(UNLOCK_FILE, LOCK_NAME, LOCK_NAME_LEN, TRUE);

	r = wakelock_plugin_register("jolla-wakelock", &table);
	if (r < 0)
		return r;

	initialized = TRUE;
	DBG("Wakelocks initialized");
	return 0;
}

BLUETOOTH_PLUGIN_DEFINE(jolla_wakelock, VERSION,
			BLUETOOTH_PLUGIN_PRIORITY_DEFAULT,
			jolla_wakelock_init, jolla_wakelock_exit)
