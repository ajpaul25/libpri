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
 * \param fsm FSM controller.
 * \param event Event to push.
 *
 * \return Nothing
 */
void fsm_event_push(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event)
{
	unsigned next_head;
	struct fsm_queue *que;
	struct fsm_event dbg_event;
	fsm_state state;

	state = fsm->state;

	dbg_event.code = FSM_EV_GET_DEBUG;
	if (state(ctrl, fsm, &dbg_event)) {
		dbg_event.code = FSM_EV_GET_EV_NAME;
		dbg_event.parms.num = event->code;
		pri_message(ctrl, "%s: Push event %s\n", fsm->name,
			(char *) state(ctrl, fsm, &dbg_event));
	}

	que = fsm->que;

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
	que->events[que->head].fsm = fsm;
	que->events[que->head].event = *event;
}

/*!
 * \brief Post an event on the tail of the event queue.
 *
 * \param ctrl D channel controller.
 * \param fsm FSM controller.
 * \param event Event to post.
 *
 * \return Nothing
 */
void fsm_event_post(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event)
{
	unsigned next_tail;
	struct fsm_queue *que;
	struct fsm_event dbg_event;
	fsm_state state;

	state = fsm->state;

	dbg_event.code = FSM_EV_GET_DEBUG;
	if (state(ctrl, fsm, &dbg_event)) {
		dbg_event.code = FSM_EV_GET_EV_NAME;
		dbg_event.parms.num = event->code;
		pri_message(ctrl, "%s: Post event %s\n", fsm->name,
			(char *) state(ctrl, fsm, &dbg_event));
	}

	que = fsm->que;

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
	que->events[que->tail].fsm = fsm;
	que->events[que->tail].event = *event;
	que->tail = next_tail;
}

/*!
 * \brief Top state of all FSMs.
 *
 * \param ctrl D channel controller.
 * \param fsm FSM controller.
 * \param event Event to process.
 *
 * \return The value has various meanings depending upon what
 * event was passed in.
 * \see enum fsm_ev event descriptions for return value.
 */
void *fsm_top_state(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event)
{
	switch (event->code) {
	case FSM_EV_GET_SUPERSTATE:
		break;
	case FSM_EV_GET_STATE_NAME:
		return (void *) __PRETTY_FUNCTION__;
	case FSM_EV_GET_EV_NAME:
		/* This is not the best state to use to get an event name. */
		return (void *) fsm_ev2str(event->parms.num);
	case FSM_EV_GET_DEBUG:
		/* Noone should be doing this since how are we to know? */
		pri_error(ctrl, DBGHEAD "Asking for FSM debug output enable!\n", DBGINFO);
		return FSM_IS_DEBUG(1);
	case FSM_EV_PROLOG:
	case FSM_EV_INIT:
	case FSM_EV_EPILOG:
	default:
		pri_error(ctrl, DBGHEAD "%s: Unhandled event: %s(%d)!\n", DBGINFO,
			fsm->name, fsm_ev2str(event->code), event->code);
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
 * \param dest Transitioning to state. (NULL if terminal)
 *
 * \return Nothing
 */
void fsm_transition(struct pri *ctrl, int debug, struct fsm_ctrl *fsm, fsm_state dest)
{
	struct fsm_event local_event;
	fsm_state epilog_state[FSM_MAX_SUPERSTATE_NESTING + 1];/* Plus top state. */
	fsm_state prolog_state[FSM_MAX_SUPERSTATE_NESTING + 1];/* Plus top state. */
	fsm_state src;
	const char *epilog_name;
	const char *prolog_name;
	const char *init_name;
	const char *src_name;
	const char *dest_name;
	int epilog_index;/* Must be signed. */
	int prolog_index;/* Must be signed. */
	int idx;/* Must be signed. */

	src = fsm->state;
	if (!src || src == fsm_top_state) {
		/* This is the initial transition to start the FSM. */
		fsm->state = fsm_top_state;
		src = NULL;
	}

	/* Get original destination state name. */
	local_event.code = FSM_EV_GET_STATE_NAME;
	if (dest && dest != fsm_top_state) {
		dest_name = dest(ctrl, fsm, &local_event);
	} else {
		/* This is the terminal transition to end the FSM. */
		dest_name = fsm_top_state(ctrl, fsm, &local_event);
		dest = NULL;
	}

	if (debug) {
		pri_message(ctrl, "%s:  Next-state %s\n", fsm->name, dest_name);
	}

	local_event.code = FSM_EV_GET_SUPERSTATE;

	/* Build src superstate nesting stack. */
	epilog_index = 0;
	epilog_state[epilog_index] = src;
	while (src) {
		if (FSM_MAX_SUPERSTATE_NESTING <= epilog_index) {
			local_event.code = FSM_EV_GET_STATE_NAME;
			src_name = fsm->state(ctrl, fsm, &local_event);

			pri_error(ctrl, "%s: FSM source state %s nested too deep!\n", fsm->name,
				src_name);
			return;
		}
		src = src(ctrl, fsm, &local_event);
		if (src == fsm_top_state) {
			src = NULL;
		}
		epilog_state[++epilog_index] = src;
	}

	/* Build dest superstate nesting stack. */
	prolog_index = 0;
	prolog_state[prolog_index] = dest;
	while (dest) {
		if (FSM_MAX_SUPERSTATE_NESTING <= prolog_index) {
			pri_error(ctrl, "%s: FSM destination state %s nested too deep!\n", fsm->name,
				dest_name);
			return;
		}
		dest = dest(ctrl, fsm, &local_event);
		if (dest == fsm_top_state) {
			dest = NULL;
		}
		prolog_state[++prolog_index] = dest;
	}

	if (!epilog_index && !prolog_index) {
		pri_error(ctrl, "%s: FSM initial transition is termination transition!\n",
			fsm->name);
		return;
	}

	/* Find first non-common superstate level. */
	for (;;) {
		--epilog_index;
		--prolog_index;
		if (epilog_index < 0 || prolog_index < 0) {
			/* No more epilogs or prologs in stack. */
			if (epilog_index == prolog_index) {
				/* This is a state transition to self. */
				epilog_index = 0;
				prolog_index = 0;
			}
			break;
		}
		if (epilog_state[epilog_index] != prolog_state[prolog_index]) {
			break;
		}
	}

	/* Execute state epilogs */
	if (0 <= epilog_index) {
		epilog_name = fsm_ev2str(FSM_EV_EPILOG);

		for (idx = 0; idx <= epilog_index; ++idx) {
			src = epilog_state[idx];
			if (debug) {
				local_event.code = FSM_EV_GET_STATE_NAME;
				src_name = src(ctrl, fsm, &local_event);

				pri_message(ctrl, "%s: Event %s in state %s\n", fsm->name, epilog_name,
					src_name);
			}
			local_event.code = FSM_EV_EPILOG;
			src(ctrl, fsm, &local_event);
		}
	}

	/* Execute known state prologs */
	if (0 <= prolog_index) {
		prolog_name = fsm_ev2str(FSM_EV_PROLOG);

		for (idx = prolog_index; 0 <= idx; --idx) {
			dest = prolog_state[idx];
			if (debug) {
				local_event.code = FSM_EV_GET_STATE_NAME;
				dest_name = dest(ctrl, fsm, &local_event);

				pri_message(ctrl, "%s: Event %s in state %s\n", fsm->name, prolog_name,
					dest_name);
			}
			local_event.code = FSM_EV_PROLOG;
			dest(ctrl, fsm, &local_event);
		}
	} else if (prolog_state[0]) {
		/* Transitioned to a state's superstate. */
		prolog_name = fsm_ev2str(FSM_EV_PROLOG);
		dest = prolog_state[0];
	} else {
		/* Termination transition. */
		fsm->state = fsm_top_state;
		if (fsm->destructor) {
			if (debug) {
				pri_message(ctrl, "%s: Destroying\n", fsm->name);
			}
			fsm->destructor(ctrl, fsm, fsm->parms);
		}
		return;
	}

	/* We reached the specified destination state. */
	fsm->state = dest;

	/* Drill down into possible further nested states. */
	init_name = fsm_ev2str(FSM_EV_INIT);
	for (;;) {
		if (debug) {
			pri_message(ctrl, "%s: Event %s in state %s\n", fsm->name, init_name,
				dest_name);
		}
		local_event.code = FSM_EV_INIT;
		dest = dest(ctrl, fsm, &local_event);
		if (!dest) {
			/* We made it to a leaf state. */
			break;
		}

		if (debug) {
			local_event.code = FSM_EV_GET_STATE_NAME;
			dest_name = dest(ctrl, fsm, &local_event);

			pri_message(ctrl, "%s: Event %s in state %s\n", fsm->name, prolog_name,
				dest_name);
		}
		local_event.code = FSM_EV_PROLOG;
		dest(ctrl, fsm, &local_event);

		/* We drilled one level deeper. */
		fsm->state = dest;
	}
}

/*!
 * \internal
 * \brief Send a real event to the FSM.
 *
 * \param ctrl D channel controller.
 * \param ev Event Q entry to process.
 *
 * \return Nothing
 */
static void fsm_event_q_process(struct pri *ctrl, struct fsm_event_q *ev)
{
	int debug;
	struct fsm_event local_event;
	const char *name_event;
	fsm_state state;

	state = ev->fsm->state;

	local_event.code = FSM_EV_GET_DEBUG;
	if (state(ctrl, ev->fsm, &local_event)) {
		debug = 1;
	} else {
		debug = 0;
	}

	local_event.code = FSM_EV_GET_EV_NAME;
	local_event.parms.num = ev->event.code;
	name_event = (const char *) state(ctrl, ev->fsm, &local_event);

	do {
		if (debug) {
			const char *name_state;

			local_event.code = FSM_EV_GET_STATE_NAME;
			name_state = (const char *) state(ctrl, ev->fsm, &local_event);

			pri_message(ctrl, "%s: Event %s in state %s\n", ev->fsm->name, name_event,
				name_state);
		}

		state = state(ctrl, ev->fsm, &ev->event);
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
	struct fsm_event_q ev;

	while (que->head != que->tail) {
		/* Pull the next event off the head of the queue. */
		ev = que->events[que->head];

		/* Advance the queue head. */
		++que->head;
		if (ARRAY_LEN(que->events) <= que->head) {
			que->head = 0;
		}

		fsm_event_q_process(ctrl, &ev);
	}
}

/*!
 * \brief Do the initial transition to start the FSM.
 *
 * \param ctrl D channel controller.
 * \param fsm Filled in FSM control structure set to the initial FSM state.
 * \param init Initial FSM state.
 *
 * \return Nothing
 */
void fsm_init(struct pri *ctrl, struct fsm_ctrl *fsm, fsm_state init)
{
	int debug;
	struct fsm_event dbg_event;

	dbg_event.code = FSM_EV_GET_DEBUG;
	if (init(ctrl, fsm, &dbg_event)) {
		debug = 1;
	} else {
		debug = 0;
	}

	if (debug) {
		pri_message(ctrl, "%s: Initial transition\n", fsm->name);
	}
	fsm->state = fsm_top_state;
	fsm_transition(ctrl, debug, fsm, init);
}

/* ------------------------------------------------------------------- */
/* end pri_fsm.c */
