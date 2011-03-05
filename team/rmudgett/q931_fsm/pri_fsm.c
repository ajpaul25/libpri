/*
 * libpri: An implementation of Primary Rate ISDN
 *
 * Copyright (C) 2011 Digium, Inc.
 *
 * Richard Mudgett <rmudgett@digium.com>
 *
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

/*!
 * \file
 * \brief Common FSM driver routines.
 *
 * \author Richard Mudgett <rmudgett@digium.com>
 */


#include "compat.h"
#include "libpri.h"
#include "pri_internal.h"
#include "pri_fsm.h"	//BUGBUG


/* ------------------------------------------------------------------- */

/*!
 * \brief Convert the given FSM event code to string.
 *
 * \param event FSM event code to convert.
 *
 * \return String equivalent of event code.
 */
const char *fsm_ev2str(enum fsm_ev event)
{
	switch (event) {
	case FSM_EV_GET_SUPERSTATE:
		return "FSM_EV_GET_SUPERSTATE";
	case FSM_EV_GET_STATE_NAME:
		return "FSM_EV_GET_STATE_NAME";
	case FSM_EV_GET_EV_NAME:
		return "FSM_EV_GET_EV_NAME";
	case FSM_EV_GET_DEBUG:
		return "FSM_EV_GET_DEBUG";
	case FSM_EV_INIT:
		return "FSM_EV_INIT";
	case FSM_EV_PROLOG:
		return "FSM_EV_PROLOG";
	case FSM_EV_EPILOG:
		return "FSM_EV_EPILOG";
	case FSM_EV_FIRST_USER_EV:
		/* Not a common event code. */
		break;
	}
	return "FSM_EV_unknown";
}

/*!
 * \brief Push an event on the head of the event queue.
 *
 * \param ctrl D channel controller.
 * \param event Event to push.
 *
 * \return Nothing
 */
void fsm_event_push(struct pri *ctrl, struct fsm_event *event)
{
	unsigned next_head;
	struct fsm_queue *que;
	struct fsm_event dbg_event;
	fsm_state state;

	state = event->fsm->state;

	//dbg_event.fsm = event->fsm;
	dbg_event.code = FSM_EV_GET_DEBUG;
	if (state(ctrl, &dbg_event)) {
		dbg_event.code = FSM_EV_GET_EV_NAME;
		dbg_event.parms.num = event->code;
		pri_message(ctrl, "%s: Push event %s\n", event->fsm->name,
			(char *) state(ctrl, &dbg_event));
	}

	que = event->fsm->que;

	/* Determine previous head index. */
	if (que->head) {
		next_head = que->head - 1;
	} else {
		next_head = ARRAY_LEN(que->events) - 1;
	}
	if (next_head == que->tail) {
		pri_error(ctrl, DBGHEAD "Queue overflow!\n", DBGINFO);
		return;
	}

	/* Put event in the queue. */
	que->head = next_head;
	que->events[que->head] = *event;
}

/*!
 * \brief Post an event on the tail of the event queue.
 *
 * \param ctrl D channel controller.
 * \param event Event to post.
 *
 * \return Nothing
 */
void fsm_event_post(struct pri *ctrl, struct fsm_event *event)
{
	unsigned next_tail;
	struct fsm_queue *que;
	struct fsm_event dbg_event;
	fsm_state state;

	state = event->fsm->state;

	//dbg_event.fsm = event->fsm;
	dbg_event.code = FSM_EV_GET_DEBUG;
	if (state(ctrl, &dbg_event)) {
		dbg_event.code = FSM_EV_GET_EV_NAME;
		dbg_event.parms.num = event->code;
		pri_message(ctrl, "%s: Post event %s\n", event->fsm->name,
			(char *) state(ctrl, &dbg_event));
	}

	que = event->fsm->que;

	/* Determine next tail index. */
	next_tail = que->tail + 1;
	if (ARRAY_LEN(que->events) <= next_tail) {
		next_tail = 0;
	}
	if (next_tail == que->head) {
		pri_error(ctrl, DBGHEAD "Queue overflow!\n", DBGINFO);
		return;
	}

	/* Put event in the queue. */
	que->events[que->tail] = *event;
	que->tail = next_tail;
}

/*!
 * \brief Top state of all FSMs.
 *
 * \param ctrl D channel controller.
 * \param event Event to process.
 *
 * \return The value has various meanings depending upon what
 * event was passed in.
 * \see enum fsm_ev event descriptions.
 *
 * \retval NULL For normal events: The state handled the event.
 *
 * \retval non-NULL For normal events: The superstate to pass
 * the event to next.
 */
void *fsm_top_state(struct pri *ctrl, struct fsm_event *event)
{
	switch (event->code) {
	case FSM_EV_GET_STATE_NAME:
		return (void *) __PRETTY_FUNCTION__;
	case FSM_EV_GET_EV_NAME:
		return (void *) fsm_ev2str(event->parms.num);
	case FSM_EV_GET_DEBUG:
		/* Noone should be doing this since how are we to know? */
		pri_error(ctrl, DBGHEAD "Asking for FSM debug output enable!\n", DBGINFO);
		return (void *) 1;
	default:
		break;
	}
	return NULL;
}

/*!
 * \brief Perform a FSM state transition.
 *
 * \param ctrl D channel controller.
 * \param debug TRUE if FSM debug output enabled.
 * \param fsm FSM that is transitioning states.
 * \param from Transitioning from state. (NULL if initial)
 * \param to Transitioning to state. (NULL if terminal)
 *
 * \return Nothing
 */
void fsm_transition(struct pri *ctrl, int debug, struct fsm_ctrl *fsm, fsm_state from, fsm_state to)
{
	struct fsm_event local_event;
	fsm_state epilog_state[FSM_MAX_SUPERSTATE_NESTING];
	fsm_state prolog_state[FSM_MAX_SUPERSTATE_NESTING];
	const char *epilog_name;
	const char *prolog_name;
	const char *init_name;
	const char *from_name;
	const char *to_name;
	int epilog_index;
	int prolog_index;
	int idx;

	local_event.fsm = fsm;

	/* Get original state names. */
	local_event.code = FSM_EV_GET_STATE_NAME;
	if (from) {
		from_name = from(ctrl, &local_event);
	} else {
		fsm->state = fsm_top_state;
		from_name = "*";
	}
	if (to) {
		to_name = to(ctrl, &local_event);
	} else {
		to_name = "*";
	}

	if (debug) {
		pri_message(ctrl, "%s:  Next-state %s\n", fsm->name, to_name);
	}

	local_event.code = FSM_EV_GET_SUPERSTATE;

	/* Build from superstate nesting stack. */
	epilog_index = 0;
	epilog_state[epilog_index++] = from;
	while (from) {
		if (FSM_MAX_SUPERSTATE_NESTING <= epilog_index) {
			pri_error(ctrl, "%s: FSM 'from' state %s nested too deep!\n", fsm->name,
				from_name);
			return;
		}
		from = from(ctrl, &local_event);
		epilog_state[epilog_index++] = from;
	}

	/* Build to superstate nesting stack. */
	prolog_index = 0;
	prolog_state[prolog_index++] = to;
	while (to) {
		if (FSM_MAX_SUPERSTATE_NESTING <= prolog_index) {
			pri_error(ctrl, "%s: FSM 'to' state %s nested too deep!\n", fsm->name,
				to_name);
			return;
		}
		to = to(ctrl, &local_event);
		prolog_state[prolog_index++] = to;
	}

	/* Find first non-common superstate level. */
	for (;;) {
		--epilog_index;
		--prolog_index;
		if (!epilog_index || !prolog_index) {
			/* No more nested superstates. */
			break;
		}
		if (epilog_state[epilog_index] != prolog_state[prolog_index]) {
			break;
		}
	}

	/* Execute state epilogs */
	if (epilog_state[epilog_index]) {
		epilog_name = fsm_ev2str(FSM_EV_EPILOG);

		for (idx = 0; idx <= epilog_index; ++idx) {
			from = epilog_state[idx];
			if (debug) {
				local_event.code = FSM_EV_GET_STATE_NAME;
				from_name = (const char *) from(ctrl, &local_event);

				pri_message(ctrl, "%s: Event %s in state %s\n", fsm->name, epilog_name,
					from_name);
			}
			local_event.code = FSM_EV_EPILOG;
			from(ctrl, &local_event);
		}
	}

	/* Execute known state prologs */
	if (prolog_state[prolog_index]) {
		prolog_name = fsm_ev2str(FSM_EV_PROLOG);

		for (idx = prolog_index; 0 <= idx; --idx) {
			to = prolog_state[idx];
			if (debug) {
				local_event.code = FSM_EV_GET_STATE_NAME;
				to_name = (const char *) to(ctrl, &local_event);

				pri_message(ctrl, "%s: Event %s in state %s\n", fsm->name, prolog_name,
					to_name);
			}
			local_event.code = FSM_EV_PROLOG;
			to(ctrl, &local_event);
		}
	} else {
		/* Termination transition. */
		if (fsm->destructor) {
			if (debug) {
				pri_message(ctrl, "%s: Destroying\n", fsm->name);
			}
			fsm->destructor(ctrl, fsm->parms);
		}
		return;
	}

	/* Drill down into possible further nested states. */
	init_name = fsm_ev2str(FSM_EV_INIT);
	to = prolog_state[0];
	for (;;) {
		if (debug) {
			local_event.code = FSM_EV_GET_STATE_NAME;
			to_name = (const char *) to(ctrl, &local_event);

			pri_message(ctrl, "%s: Event %s in state %s\n", fsm->name, init_name,
				to_name);
		}
		local_event.code = FSM_EV_INIT;
		to = to(ctrl, &local_event);
		if (!to) {
			/* We made it to a leaf state. */
			fsm->state = prolog_state[0];
			break;
		}
		prolog_state[0] = to;
		if (debug) {
			local_event.code = FSM_EV_GET_STATE_NAME;
			to_name = (const char *) to(ctrl, &local_event);

			pri_message(ctrl, "%s: Event %s in state %s\n", fsm->name, prolog_name,
				to_name);
		}
		local_event.code = FSM_EV_PROLOG;
		to(ctrl, &local_event);
	}
}

/*!
 * \internal
 * \brief Send a real event to the FSM.
 *
 * \param ctrl D channel controller.
 * \param event Event to process.
 *
 * \return Nothing
 */
static void fsm_event_process(struct pri *ctrl, struct fsm_event *event)
{
	int debug;
	struct fsm_event local_event;
	const char *name_event;
	fsm_state state;

	state = event->fsm->state;

	//local_event.fsm = event->fsm;
	local_event.code = FSM_EV_GET_DEBUG;
	if (state(ctrl, &local_event)) {
		debug = 1;
	} else {
		debug = 0;
	}

	local_event.code = FSM_EV_GET_EV_NAME;
	local_event.parms.num = event->code;
	name_event = (const char *) state(ctrl, &local_event);

	do {
		if (debug) {
			const char *name_state;

			local_event.code = FSM_EV_GET_STATE_NAME;
			name_state = (const char *) state(ctrl, &local_event);

			pri_message(ctrl, "%s: Event %s in state %s\n", event->fsm->name, name_event,
				name_state);
		}

		state = state(ctrl, event);
	} while (state);
}

/*!
 * \brief Process all events in the event Q before returning.
 *
 * \param ctrl D channel controller.
 * \param que Event Q to process.
 *
 * \return Nothing
 */
void fsm_run(struct pri *ctrl, struct fsm_queue *que)
{
	struct fsm_event event;

	while (que->head != que->tail) {
		/* Pull the next event off the head of the queue. */
		event = que->events[que->head];

		/* Advance the queue head. */
		++que->head;
		if (ARRAY_LEN(que->events) <= que->head) {
			que->head = 0;
		}

		fsm_event_process(ctrl, &event);
	}
}

/*!
 * \brief Do the initial transition to start the FSM.
 *
 * \param ctrl D channel controller.
 * \param fsm Filled in FSM control structure set to the initial FSM state.
 *
 * \return Nothing
 */
void fsm_init(struct pri *ctrl, struct fsm_ctrl *fsm)
{
	int debug;
	struct fsm_event dbg_event;

	dbg_event.code = FSM_EV_GET_DEBUG;
	if (fsm->state(ctrl, &dbg_event)) {
		debug = 1;
	} else {
		debug = 0;
	}

	if (debug) {
		pri_message(ctrl, "%s: Initial transition\n", fsm->name);
	}
	fsm_transition(ctrl, debug, fsm, NULL, fsm->state);
}

/* ------------------------------------------------------------------- */
/* end pri_fsm.c */
