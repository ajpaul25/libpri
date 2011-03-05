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

#define FSM_MAX_EV_PARAM_BYTES		100		/*!< Max space available for any event parameters. */
#define FSM_MAX_Q_EVENTS			10		/*!< Max number of events the common Q can contain. */
#define FSM_MAX_SUPERSTATE_NESTING	10		/*!< Max number of nested superstates. */

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
	 */
	FSM_EV_GET_SUPERSTATE,
	/*! \brief Event to get the __PRETTY_FUNCTION__ string of the FSM state. */
	FSM_EV_GET_STATE_NAME,
	/*! \brief Event to get the event name string. */
	FSM_EV_GET_EV_NAME,
	/*! \brief Event to get the FSM debug output enable flag. */
	FSM_EV_GET_DEBUG,
	/*!
	 * \brief Event to get the initial sub-state of the FSM state.
	 *
	 * \note
	 * Used to drill down into the FSM to find the initial FSM leaf
	 * state.
	 */
	FSM_EV_INIT,
	/*!
	 * \brief Event to construct the FSM state.
	 *
	 * \note
	 * Used to construct the FSM state when an event causes a
	 * transition into the state.
	 */
	FSM_EV_PROLOG,
	/*!
	 * \brief Event to destroy the FSM state.
	 *
	 * \note
	 * Used to destroy the FSM state when an event causes a
	 * transition from the state.
	 */
	FSM_EV_EPILOG,

	/*!
	 * \brief First event code value available for a user defined FSM.
	 *
	 * \note MUST be last in the enum.
	 */
	FSM_EV_FIRST_USER_EV
};

/*!
 * \brief Pass an event to a state.
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
typedef void *(*fsm_state)(struct pri *ctrl, struct fsm_event *event);

/*!
 * \brief Do what is necessary to clean up after a FSM terminates.
 *
 * \param ctrl D channel controller.
 * \param parms Struct containing the instance of the FSM that terminated.
 *
 * \note
 * The destructor may destroy the parms structure.
 *
 * \return Nothing
 */
typedef void (*fsm_destructor)(struct pri *ctrl, void *parms);

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
	/* The following elements are common to all events. */

	/*! FSM event code. */
	int code;
	/*! Which FSM is to receive the event. */
	struct fsm_ctrl *fsm;

	/* Any following elements are optional. */

	/*!
	 * \brief Event parameters if needed.
	 * \note Union done for alignment purposes.
	 */
	union {
		/*! Generic pointer to guarantee pointer alignment. */
		void *ptr;
		int num;
		/*! Reserve space for custom event parameters. */
		char filler[FSM_MAX_EV_PARAM_BYTES];
	} parms;
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
	struct fsm_event events[FSM_MAX_Q_EVENTS];
};

/* ------------------------------------------------------------------- */

const char *fsm_ev2str(enum fsm_ev event);
void fsm_event_push(struct pri *ctrl, struct fsm_event *event);
void fsm_event_post(struct pri *ctrl, struct fsm_event *event);
void *fsm_top_state(struct pri *ctrl, struct fsm_event *event);
void fsm_transition(struct pri *ctrl, int debug, struct fsm_ctrl *fsm, fsm_state from, fsm_state to);
void fsm_run(struct pri *ctrl, struct fsm_queue *que);
void fsm_init(struct pri *ctrl, struct fsm_ctrl *fsm);

/* ------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif	/* _LIBPRI_PRI_FSM_H */
/* ------------------------------------------------------------------- */
/* end pri_fsm.h */
