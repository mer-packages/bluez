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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <glib.h>

#include "wakelock.h"

static gchar *impl = NULL;
static struct wakelock_table table;

int wakelock_create(const gchar *name, struct wakelock **wakelock)
{
	if (!impl) {
		*wakelock = NULL;
		return 0;
	}
	return (table.create)(name, wakelock);
}

int wakelock_free(struct wakelock *wakelock)
{
	return impl ? (table.free)(wakelock) : 0;
}

int wakelock_acquire(struct wakelock *wakelock)
{
	return impl ? (table.acquire)(wakelock) : 0;
}

int wakelock_release(struct wakelock *wakelock)
{
	return impl ? (table.release)(wakelock) : 0;
}

int wakelock_plugin_register(const gchar *name, struct wakelock_table *fns)
{
	if (impl)
		return -EALREADY;

	impl = g_strdup(name);
	memcpy(&table, fns, sizeof(struct wakelock_table));
	return 0;
}

int wakelock_plugin_unregister(void)
{
	if (!impl)
		return -ENOENT;

	memset(&table, 0, sizeof(struct wakelock_table));
	g_free(impl);
	impl = NULL;

	return 0;
}

