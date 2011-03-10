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
 * \brief Common declarations for FSM driver.
 *
 * \author Richard Mudgett <rmudgett@digium.com>
 */

#ifndef _LIBPRI_PRI_FSM_H
#define _LIBPRI_PRI_FSM_H

#ifdef __cplusplus
extern "C" {
#endif

struct pri;

/* ------------------------------------------------------------------- */

#define FSM_MAX_Q_EVENTS			10		/*!< Max number of events the common Q can contain. */
#define FSM_MAX_SUPERSTATE_NESTING	10		/*!< Max number of nested superstates. */

struct fsm_ctrl;
struct fsm_event;
struct fsm_queue;

/*! Commont FSM event codes. */
enum fsm_ev {
	/*!
	 * \brief Event to get the superstate of the FSM state.
	 *
	 * \note
	 * Used to determine which PROLOG/EPILOG events need to run in a
	 * state transition.
	 *
	 * \return The superstate of the current state.
	 */
	FSM_EV_GET_SUPERSTATE,
	/*! \brief Event to get the __PRETTY_FUNCTION__ string of the FSM state. */
	FSM_EV_GET_STATE_NAME,
	/*!
	 * \brief Event to get the event name string.
	 *
	 * \note This event takes an event code parameter: event.parms.num
	 */
	FSM_EV_GET_EV_NAME,
	/*!
	 * \brief Event to get the FSM debug output enable flag.
	 * 
	 * \note Return the FSM_IS_DEBUG() value.
	 *
	 * \retval NULL if debug output is disabled.
	 */
	FSM_EV_GET_DEBUG,
	/*!
	 * \brief Event to get the initial sub-state of the FSM state.
	 *
	 * \note
	 * Used to drill down into the FSM to find the initial FSM leaf
	 * state.
	 *
	 * \retval NULL The state is a leaf state.  There is no default substate.
	 *
	 * \retval substate The default substate to drill down into the FSM.
	 */
	FSM_EV_INIT,
	/*!
	 * \brief Event to construct the FSM state.
	 *
	 * \note
	 * Used to construct the FSM state when an event causes a
	 * transition into the state.
	 *
	 * \return The superstate of the current state.
	 */
	FSM_EV_PROLOG,
	/*!
	 * \brief Event to destroy the FSM state.
	 *
	 * \note
	 * Used to destroy the FSM state when an event causes a
	 * transition from the state.
	 *
	 * \return The superstate of the current state.
	 */
	FSM_EV_EPILOG,

	/*!
	 * \brief First normal event code value available for a user defined FSM.
	 *
	 * \note MUST be last in the enum.
	 *
	 * \retval NULL The state handled the event.
	 *
	 * \retval superstate The superstate of the current state to
	 * pass the event to next.
	 */
	FSM_EV_FIRST_USER_EV
};

/*!
 * \brief Pass an event to a state.
 *
 * \param ctrl D channel controller.
 * \param fsm FSM controller.
 * \param event Event to process.
 *
 * \return The value has various meanings depending upon what
 * event was passed in.
 * \see enum fsm_ev event descriptions for return value.
 */
typedef void *(*fsm_state)(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event);

/*!
 * \brief Do what is necessary to clean up after a FSM terminates.
 *
 * \param ctrl D channel controller.
 * \param fsm FSM controller.
 * \param parms Struct containing the instance of the FSM that terminated.
 *
 * \note
 * The destructor may destroy the fsm and parms structures.
 *
 * \return Nothing
 */
typedef void (*fsm_destructor)(struct pri *ctrl, struct fsm_ctrl *fsm, void *parms);

/*! FSM control block. */
struct fsm_ctrl {
	/*! Current state of the FSM. */
	fsm_state state;
	/*!
	 * \brief Function called when the FSM terminates.
	 * \note NULL if not used.
	 */
	fsm_destructor destructor;
	/*!
	 * \brief Name of the FSM for debug output
	 * \note Use to make unique names such as call-reference id, cc_record id...
	 */
	const char *name;
	/*! Where is the event Q. */
	struct fsm_queue *que;
	/*! Struct containing this instance of the FSM. */
	void *parms;
};

struct fsm_event {
	/*! FSM event code. */
	int code;
	/*! \brief Event parameters if needed. */
	union {
		/*! \brief Generic pointer to extra event parameters. */
		void *ptr;
		/*! \brief Simple extra numeric event parameter. */
		long num;
	} parms;
};

struct fsm_event_q {
	/*! Which FSM is to receive the event. */
	struct fsm_ctrl *fsm;
	/*! Event sent to FSM. */
	struct fsm_event event;
};

/*!
 * Q of FSM events so the main FSM driver can determine when it
 * is done processing events.
 */
struct fsm_queue {
	/*! Next index to get an event off the Q. */
	unsigned head;
	/*! Next index to put and event on the Q. */
	unsigned tail;
	struct fsm_event_q events[FSM_MAX_Q_EVENTS];
};

/* ------------------------------------------------------------------- */

#define FSM_IS_DEBUG(is_debug)	((is_debug) ? (void *) 1 : (void *) 0)

const char *fsm_ev2str(enum fsm_ev event);
void fsm_event_push_parms(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event);
void fsm_event_post_parms(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event);
void fsm_event_push(struct pri *ctrl, struct fsm_ctrl *fsm, int code);
void fsm_event_post(struct pri *ctrl, struct fsm_ctrl *fsm, int code);
void *fsm_top_state(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event);
void *fsm_transition(struct pri *ctrl, int debug, struct fsm_ctrl *fsm, fsm_state dest);
void fsm_run(struct pri *ctrl, struct fsm_queue *que);
void fsm_init(struct pri *ctrl, struct fsm_ctrl *fsm, fsm_state init);

/* ------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif	/* _LIBPRI_PRI_FSM_H */
/* ------------------------------------------------------------------- */
/* end pri_fsm.h */
