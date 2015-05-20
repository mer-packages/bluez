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

/* Plugin that implements wakelocks using nemomobile-keepalive
   library. Any number of virtual wakelocks can be defined, mapping to
   one cpu keepalive object as follows:

   - no virtual wakelock active -> cpu keepalive not active
   - at least one virtual wakelock active -> cpu keepalive active
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
#include <dlfcn.h>

#include "plugin.h"
#include "log.h"
#include "wakelock.h"

struct wakelock {
	gchar *name;
	gsize acquisitions;
	gboolean stale;
};

typedef struct cpukeepalive_t cpukeepalive_t;
static cpukeepalive_t *(*cpukeepalive_new)(void) = NULL;
static void (*cpukeepalive_unref)(cpukeepalive_t *self) = NULL;
static void (*cpukeepalive_start)(cpukeepalive_t *self) = NULL;
static void (*cpukeepalive_stop)(cpukeepalive_t *self) = NULL;
static void *cpukeepalive_lib = NULL;

static GSList *locks = NULL;

static gboolean initialized = FALSE;

static cpukeepalive_t *keepalive = NULL;

static gboolean keepalive_started = FALSE;

static int jolla_wakelock_acquire(struct wakelock *lock)
{
	DBG("lock: %p, name: %s, acquisitions: %u, stale: %s",
		lock, lock->name, lock->acquisitions,
		lock->stale ? "yes" : "no");

	if (lock->stale)
		return -EBADF;

	if (keepalive_started)
		goto done;

	(*cpukeepalive_start)(keepalive);
	keepalive_started = TRUE;
	DBG("Keepalive started for lock: %p, name %s", lock, lock->name);

done:
	lock->acquisitions++;
	DBG("lock: %p, name %s acquired", lock, lock->name);

	return 0;
}

static int jolla_wakelock_release(struct wakelock *lock)
{
	GSList *l;

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

	/* See if this was the last active lock, only stop keepalive if so */
	for (l = locks; l; l = l->next) {
		struct wakelock *check = l->data;
		if (check->acquisitions)
			goto done;
	}

	(*cpukeepalive_stop)(keepalive);
	keepalive_started = FALSE;
	DBG("Keepalive stopped for lock: %p, name %s", lock, lock->name);

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
	(*cpukeepalive_unref)(keepalive);
	keepalive = NULL;
	dlclose(cpukeepalive_lib);
	cpukeepalive_lib = NULL;
	initialized = FALSE;
	DBG("Wakelocks uninitialized");
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

	cpukeepalive_lib = dlopen("libkeepalive-glib.so.1", RTLD_NOW);
	if (!cpukeepalive_lib) {
		DBG("dlopen() failed: %s", dlerror());
		r = -EIO;
		goto failed;
	}
	dlerror();

	cpukeepalive_new = dlsym(cpukeepalive_lib, "cpukeepalive_new");
	cpukeepalive_unref = dlsym(cpukeepalive_lib, "cpukeepalive_unref");
	cpukeepalive_start = dlsym(cpukeepalive_lib, "cpukeepalive_start");
	cpukeepalive_stop = dlsym(cpukeepalive_lib, "cpukeepalive_stop");
	if (!cpukeepalive_new || !cpukeepalive_unref || !cpukeepalive_start ||
							!cpukeepalive_stop) {
		DBG("dlsym() failed: %s", dlerror());
		r = -ENOENT;
		goto failed;
	}

	keepalive = (*cpukeepalive_new)();
	if (!keepalive) {
		r = -EIO;
		goto failed;
	}

	r = wakelock_plugin_register("jolla-wakelock", &table);
	if (r < 0)
		goto failed;

	initialized = TRUE;
	DBG("Wakelocks initialized");
	return 0;

failed:
	if (keepalive) {
		(*cpukeepalive_unref)(keepalive);
		keepalive = NULL;
	}

	if (cpukeepalive_lib) {
		dlclose(cpukeepalive_lib);
		cpukeepalive_lib = NULL;
	}

	warn("Failed to initialize wakelocks");

	return r;
}

BLUETOOTH_PLUGIN_DEFINE(jolla_wakelock, VERSION,
			BLUETOOTH_PLUGIN_PRIORITY_DEFAULT,
			jolla_wakelock_init, jolla_wakelock_exit)
