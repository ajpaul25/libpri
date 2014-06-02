/*
 * libpri: An implementation of Primary Rate ISDN
 *
 * Written by Mark Spencer <markster@digium.com>
 *
 * Copyright (C) 2001-2005, Digium, Inc.
 * All Rights Reserved.
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 *
 * In addition, when this program is distributed with Asterisk in
 * any form that would qualify as a 'combined work' or as a
 * 'derivative work' (but not mere aggregation), you can redistribute
 * and/or modify the combination under the terms of the license
 * provided with that copy of Asterisk, instead of the license
 * terms granted here.
 */

#include <stdio.h>

#include "libpri.h"
#include "pri_internal.h"


/*! \brief The maximum number of timers that were active at once. */
static int maxsched = 0;

/* Scheduler routines */

/*!
 * \brief Start a timer to schedule an event.
 *
 * \param ctrl D channel controller.
 * \param ms Number of milliseconds to scheduled event.
 * \param function Callback function to call when timeout.
 * \param data Value to give callback function when timeout.
 *
 * \retval 0 if scheduler table is full and could not schedule the event.
 * \retval id Scheduled event id.
 */
int pri_schedule_event(struct pri *ctrl, int ms, void (*function)(void *data), void *data)
{
	int x;
	struct timeval tv;

	/* Scheduling runs on master channels only */
	while (ctrl->master) {
		ctrl = ctrl->master;
	}
	for (x = 0; x < MAX_SCHED; ++x) {
		if (!ctrl->pri_sched[x].callback) {
			break;
		}
	}
	if (x == MAX_SCHED) {
		pri_error(ctrl, "No more room in scheduler\n");
		return 0;
	}
	if (x >= maxsched) {
		maxsched = x + 1;
	}
	gettimeofday(&tv, NULL);
	tv.tv_sec += ms / 1000;
	tv.tv_usec += (ms % 1000) * 1000;
	if (tv.tv_usec > 1000000) {
		tv.tv_usec -= 1000000;
		tv.tv_sec += 1;
	}
	ctrl->pri_sched[x].when = tv;
	ctrl->pri_sched[x].callback = function;
	ctrl->pri_sched[x].data = data;
	return x + 1;
}

/*!
 * \brief Determine the time of the next scheduled event to expire.
 *
 * \param ctrl D channel controller.
 *
 * \return Time of the next scheduled event to expire or NULL if no timers active.
 */
struct timeval *pri_schedule_next(struct pri *ctrl)
{
	struct timeval *closest = NULL;
	int x;

	/* Scheduling runs on master channels only */
	while (ctrl->master) {
		ctrl = ctrl->master;
	}
	for (x = 0; x < MAX_SCHED; ++x) {
		if (ctrl->pri_sched[x].callback && (!closest
			|| (closest->tv_sec > ctrl->pri_sched[x].when.tv_sec)
			|| ((closest->tv_sec == ctrl->pri_sched[x].when.tv_sec)
			&& (closest->tv_usec > ctrl->pri_sched[x].when.tv_usec)))) {
			closest = &ctrl->pri_sched[x].when;
		}
	}
	return closest;
}

/*!
 * \internal
 * \brief Run all expired timers or return an event generated by an expired timer.
 *
 * \param ctrl D channel controller.
 * \param tv Current time.
 *
 * \return Event for upper layer to process or NULL if all expired timers run.
 */
static pri_event *__pri_schedule_run(struct pri *ctrl, struct timeval *tv)
{
	int x;
	void (*callback)(void *);
	void *data;

	/* Scheduling runs on master channels only */
	while (ctrl->master) {
		ctrl = ctrl->master;
	}
	for (x = 0; x < MAX_SCHED; ++x) {
		if (ctrl->pri_sched[x].callback && ((ctrl->pri_sched[x].when.tv_sec < tv->tv_sec)
			|| ((ctrl->pri_sched[x].when.tv_sec == tv->tv_sec)
			&& (ctrl->pri_sched[x].when.tv_usec <= tv->tv_usec)))) {
			/* This timer has expired. */
			ctrl->schedev = 0;
			callback = ctrl->pri_sched[x].callback;
			data = ctrl->pri_sched[x].data;
			ctrl->pri_sched[x].callback = NULL;
			callback(data);
			if (ctrl->schedev) {
				return &ctrl->ev;
			}
		}
	}
	return NULL;
}

/*!
 * \brief Run all expired timers or return an event generated by an expired timer.
 *
 * \param ctrl D channel controller.
 *
 * \return Event for upper layer to process or NULL if all expired timers run.
 */
pri_event *pri_schedule_run(struct pri *ctrl)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return __pri_schedule_run(ctrl, &tv);
}

/*!
 * \brief Delete a scheduled event.
 *
 * \param ctrl D channel controller.
 * \param id Scheduled event id to delete.
 * 0 is a disabled/unscheduled event id that is ignored.
 * 1 - MAX_SCHED is a valid event id.
 *
 * \return Nothing
 */
void pri_schedule_del(struct pri *ctrl, int id)
{
	/* Scheduling runs on master channels only */
	while (ctrl->master) {
		ctrl = ctrl->master;
	}
	if (0 < id && id <= MAX_SCHED) {
		ctrl->pri_sched[id - 1].callback = NULL;
	} else if (id) {
		pri_error(ctrl, "Asked to delete sched id %d???\n", id);
	}
}