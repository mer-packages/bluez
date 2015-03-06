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

/* Wakelock interface
 *
 * Wakelocks ensure system does not suspend/enter power save mode
 * before an ongoing operation completes. This header declares a
 * wakelock API that can be implemented by a OS-specific plugin.
 */

struct wakelock; /* Opaque */

struct wakelock_table {
	int (*create)(const gchar *name, struct wakelock **);
	int (*free)(struct wakelock *);
	int (*acquire)(struct wakelock *);
	int (*release)(struct wakelock *);
};

/*** Functions for wakelock users ***/

/*
 * Create a wakelock. Multiple wakelocks can be created; if any one of
 * them is activated, system will be prevented from going to suspend.
 * This makes it possible to overlap locks to hand over from subsystem
 * to subsystem, each with their own wakelock-guarded sections,
 * without falling to suspend in between.
 */
int wakelock_create(const gchar *name, struct wakelock **wakelock);

/*
 * Free a wakelock, releasing all acquisitions at the same time
 * and deallocating the lock.
 */
int wakelock_free(struct wakelock *wakelock);

/*
 * Acquire a wakelock. Multiple acquisitions are possible, meaning
 * that the wakelock needs to be released the same number of times
 * until it is actually deactivated.
 */
int wakelock_acquire(struct wakelock *wakelock);

/*
 * Release a wakelock, deactivating it if all acquisitions are
 * released, letting system suspend.
 */
int wakelock_release(struct wakelock *wakelock);

/*** Functions for wakelock implementors ***/

/*
 * Register a wakelock implementation. Only one implementation may be
 * registered at a time. In the absence of an implementation, all
 * wakelock functions are no-ops.
 */
int wakelock_plugin_register(const gchar *name, struct wakelock_table *fns);

int wakelock_plugin_unregister(void);

