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
 * \brief Unit test of the FSM driver submodule.
 *
 * \author Richard Mudgett <rmudgett@digium.com>
 */


#include "compat.h"
#include "libpri.h"
#include "pri_internal.h"
#include "pri_fsm.h"	//BUGBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum test_fsm_ev {
	TST_EV_A = FSM_EV_FIRST_USER_EV,
	TST_EV_B,
	TST_EV_C,
	TST_EV_D,
	TST_EV_E,
	TST_EV_F,
	TST_EV_G,
	TST_EV_H,
	TST_EV_J,
};

/* ------------------------------------------------------------------- */

static void tst_pri_message(struct pri *ctrl, char *stuff)
{
	fprintf(stdout, "%s", stuff);
}

static void tst_pri_error(struct pri *ctrl, char *stuff)
{
	fprintf(stdout, "%s", stuff);
	fprintf(stderr, "%s", stuff);
}

/* ------------------------------------------------------------------- */

/*!
 * \internal
 * \brief Convert the given FSM event code to string.
 *
 * \param event FSM event code to convert.
 *
 * \return String equivalent of event code.
 */
static const char *tst_ev2str(int event)
{
	if (event < FSM_EV_FIRST_USER_EV) {
		return fsm_ev2str(event);
	}
	switch (event) {
	case TST_EV_A:
		return "TST_EV_A";
	case TST_EV_B:
		return "TST_EV_B";
	case TST_EV_C:
		return "TST_EV_C";
	case TST_EV_D:
		return "TST_EV_D";
	case TST_EV_E:
		return "TST_EV_E";
	case TST_EV_F:
		return "TST_EV_F";
	case TST_EV_G:
		return "TST_EV_G";
	case TST_EV_H:
		return "TST_EV_H";
	case TST_EV_J:
		return "TST_EV_J";
	default:
		return "TST_EV_unknown";
	}
}

/*!
 * \internal
 * \brief Simple event posting.
 * 
 * \param ctrl D channel controller. 
 * \param fsm Event is sent to this FSM.
 * \param code Event code.
 *  
 * \return Nothing
 */
static void tst_simple_post(struct pri *ctrl, struct fsm_ctrl *fsm, int code)
{
	struct fsm_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.code = code;
	fsm_event_post(ctrl, fsm, &ev);
}

/*!
 * \internal
 * \brief Simple event pushing.
 * 
 * \param ctrl D channel controller. 
 * \param fsm Event is sent to this FSM.
 * \param code Event code.
 *  
 * \return Nothing
 */
static void tst_simple_push(struct pri *ctrl, struct fsm_ctrl *fsm, int code)
{
	struct fsm_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.code = code;
	fsm_event_push(ctrl, fsm, &ev);
}

static void *tst_state_1(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event);
static void *tst_state_1_1(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event);
static void *tst_state_1_1_1(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event);
static void *tst_state_1_1_2(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event);
static void *tst_state_1_2(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event);
static void *tst_state_1_3(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event);
static void *tst_state_1_3_1(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event);

#define ACT_DEBUG(ctrl, fsm, event)	\
	pri_message(ctrl, "%s:  Act %s(%s)\n", fsm->name, __PRETTY_FUNCTION__, tst_ev2str(event->code))

/*!
 * \internal
 * \brief State 1.1.1.
 *
 * \param ctrl D channel controller.
 * \param event Event to process.
 *
 * \return The value has various meanings depending upon what
 * event was passed in.
 * \see enum fsm_ev event descriptions for return value.
 */
static void *tst_state_1_1_1(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event)
{
	switch (event->code) {
	case FSM_EV_GET_SUPERSTATE:
		return tst_state_1_1;
	case FSM_EV_GET_STATE_NAME:
		return (void *) __PRETTY_FUNCTION__;
	case FSM_EV_GET_EV_NAME:
		return (void *) tst_ev2str(event->parms.num);
	case FSM_EV_GET_DEBUG:
		return FSM_IS_DEBUG(ctrl->debug & PRI_DEBUG_Q931_STATE);
	case FSM_EV_PROLOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	case FSM_EV_INIT:
		return NULL;
	case FSM_EV_EPILOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	case TST_EV_A:
		ACT_DEBUG(ctrl, fsm, event);
		tst_simple_post(ctrl, fsm, TST_EV_B);
		return NULL;
	case TST_EV_B:
		ACT_DEBUG(ctrl, fsm, event);
		tst_simple_push(ctrl, fsm, TST_EV_D);
		tst_simple_push(ctrl, fsm, TST_EV_C);
		tst_simple_post(ctrl, fsm, TST_EV_E);
		fsm_transition(ctrl, ctrl->debug & PRI_DEBUG_Q931_STATE, fsm, tst_state_1_1_1);
		return NULL;
	case TST_EV_C:
		ACT_DEBUG(ctrl, fsm, event);
		fsm_transition(ctrl, ctrl->debug & PRI_DEBUG_Q931_STATE, fsm, tst_state_1_1_2);
		return NULL;
	case TST_EV_E:
		ACT_DEBUG(ctrl, fsm, event);
		tst_simple_post(ctrl, fsm, TST_EV_F);
		fsm_transition(ctrl, ctrl->debug & PRI_DEBUG_Q931_STATE, fsm, tst_state_1_2);
		return NULL;
	default:
		break;
	}
	return tst_state_1_1;
}

/*!
 * \internal
 * \brief State 1.1.2.
 *
 * \param ctrl D channel controller.
 * \param event Event to process.
 *
 * \return The value has various meanings depending upon what
 * event was passed in.
 * \see enum fsm_ev event descriptions for return value.
 */
static void *tst_state_1_1_2(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event)
{
	switch (event->code) {
	case FSM_EV_GET_SUPERSTATE:
		return tst_state_1_1;
	case FSM_EV_GET_STATE_NAME:
		return (void *) __PRETTY_FUNCTION__;
	case FSM_EV_GET_EV_NAME:
		return (void *) tst_ev2str(event->parms.num);
	case FSM_EV_GET_DEBUG:
		return FSM_IS_DEBUG(ctrl->debug & PRI_DEBUG_Q931_STATE);
	case FSM_EV_PROLOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	case FSM_EV_INIT:
		return NULL;
	case FSM_EV_EPILOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	case TST_EV_D:
		ACT_DEBUG(ctrl, fsm, event);
		fsm_transition(ctrl, ctrl->debug & PRI_DEBUG_Q931_STATE, fsm, tst_state_1_1);
		return NULL;
	case TST_EV_H:
		ACT_DEBUG(ctrl, fsm, event);
		fsm_transition(ctrl, ctrl->debug & PRI_DEBUG_Q931_STATE, fsm, NULL);
		return NULL;
	default:
		break;
	}
	return tst_state_1_1;
}

/*!
 * \internal
 * \brief State 1.1.
 *
 * \param ctrl D channel controller.
 * \param event Event to process.
 *
 * \return The value has various meanings depending upon what
 * event was passed in.
 * \see enum fsm_ev event descriptions for return value.
 */
static void *tst_state_1_1(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event)
{
	switch (event->code) {
	case FSM_EV_GET_SUPERSTATE:
		return tst_state_1;
	case FSM_EV_GET_STATE_NAME:
		return (void *) __PRETTY_FUNCTION__;
	case FSM_EV_GET_EV_NAME:
		return (void *) tst_ev2str(event->parms.num);
	case FSM_EV_GET_DEBUG:
		return FSM_IS_DEBUG(ctrl->debug & PRI_DEBUG_Q931_STATE);
	case FSM_EV_PROLOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	case FSM_EV_INIT:
		return tst_state_1_1_1;
	case FSM_EV_EPILOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	default:
		break;
	}
	return tst_state_1;
}

/*!
 * \internal
 * \brief State 1.2.
 *
 * \param ctrl D channel controller.
 * \param event Event to process.
 *
 * \return The value has various meanings depending upon what
 * event was passed in.
 * \see enum fsm_ev event descriptions for return value.
 */
static void *tst_state_1_2(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event)
{
	switch (event->code) {
	case FSM_EV_GET_SUPERSTATE:
		return tst_state_1;
	case FSM_EV_GET_STATE_NAME:
		return (void *) __PRETTY_FUNCTION__;
	case FSM_EV_GET_EV_NAME:
		return (void *) tst_ev2str(event->parms.num);
	case FSM_EV_GET_DEBUG:
		return FSM_IS_DEBUG(ctrl->debug & PRI_DEBUG_Q931_STATE);
	case FSM_EV_PROLOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	case FSM_EV_INIT:
		return NULL;
	case FSM_EV_EPILOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	case TST_EV_F:
		ACT_DEBUG(ctrl, fsm, event);
		tst_simple_post(ctrl, fsm, TST_EV_G);
		fsm_transition(ctrl, ctrl->debug & PRI_DEBUG_Q931_STATE, fsm, tst_state_1_3);
		return NULL;
	default:
		break;
	}
	return tst_state_1;
}

/*!
 * \internal
 * \brief State 1.3.1.
 *
 * \param ctrl D channel controller.
 * \param event Event to process.
 *
 * \return The value has various meanings depending upon what
 * event was passed in.
 * \see enum fsm_ev event descriptions for return value.
 */
static void *tst_state_1_3_1(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event)
{
	switch (event->code) {
	case FSM_EV_GET_SUPERSTATE:
		return tst_state_1_3;
	case FSM_EV_GET_STATE_NAME:
		return (void *) __PRETTY_FUNCTION__;
	case FSM_EV_GET_EV_NAME:
		return (void *) tst_ev2str(event->parms.num);
	case FSM_EV_GET_DEBUG:
		return FSM_IS_DEBUG(ctrl->debug & PRI_DEBUG_Q931_STATE);
	case FSM_EV_PROLOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	case FSM_EV_INIT:
		return NULL;
	case FSM_EV_EPILOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	default:
		break;
	}
	return tst_state_1_3;
}

/*!
 * \internal
 * \brief State 1.3.
 *
 * \param ctrl D channel controller.
 * \param event Event to process.
 *
 * \return The value has various meanings depending upon what
 * event was passed in.
 * \see enum fsm_ev event descriptions for return value.
 */
static void *tst_state_1_3(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event)
{
	switch (event->code) {
	case FSM_EV_GET_SUPERSTATE:
		return tst_state_1;
	case FSM_EV_GET_STATE_NAME:
		return (void *) __PRETTY_FUNCTION__;
	case FSM_EV_GET_EV_NAME:
		return (void *) tst_ev2str(event->parms.num);
	case FSM_EV_GET_DEBUG:
		return FSM_IS_DEBUG(ctrl->debug & PRI_DEBUG_Q931_STATE);
	case FSM_EV_PROLOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	case FSM_EV_INIT:
		return tst_state_1_3_1;
	case FSM_EV_EPILOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	case TST_EV_G:
		ACT_DEBUG(ctrl, fsm, event);
		fsm_transition(ctrl, ctrl->debug & PRI_DEBUG_Q931_STATE, fsm, tst_state_1_1_2);
		return NULL;
	default:
		break;
	}
	return tst_state_1;
}

/*!
 * \internal
 * \brief State 1.
 *
 * \param ctrl D channel controller.
 * \param event Event to process.
 *
 * \return The value has various meanings depending upon what
 * event was passed in.
 * \see enum fsm_ev event descriptions for return value.
 */
static void *tst_state_1(struct pri *ctrl, struct fsm_ctrl *fsm, struct fsm_event *event)
{
	switch (event->code) {
	case FSM_EV_GET_SUPERSTATE:
		return fsm_top_state;
	case FSM_EV_GET_STATE_NAME:
		return (void *) __PRETTY_FUNCTION__;
	case FSM_EV_GET_EV_NAME:
		return (void *) tst_ev2str(event->parms.num);
	case FSM_EV_GET_DEBUG:
		return FSM_IS_DEBUG(ctrl->debug & PRI_DEBUG_Q931_STATE);
	case FSM_EV_PROLOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	case FSM_EV_INIT:
		return tst_state_1_1;
	case FSM_EV_EPILOG:
		ACT_DEBUG(ctrl, fsm, event);
		break;
	default:
		break;
	}
	return fsm_top_state;
}

/*!
 * \internal
 * \brief Destructor of the test FSM.
 *
 * \param ctrl D channel controller.
 * \param parms Which instance of the FSM is being destoyed.
 *
 * \return Nothing
 */
static void tst_fsm_destructor(struct pri *ctrl, struct fsm_ctrl *fsm, void *parms)
{
	pri_message(ctrl, DBGHEAD "\n", DBGINFO);
	*(int *) parms = *((int *) parms) + 1;
}

/*!
 * \brief FSM driver test program.
 *
 * \param argc Program argument count.
 * \param argv Program argument string array.
 *
 * \retval 0 on success.
 * \retval Nonzero on error.
 */
int main(int argc, char *argv[])
{
	static struct pri dummy_ctrl;
	static struct fsm_queue ev_q;
	static struct fsm_ctrl fsm;
	static int fsm_destroyed;

	pri_set_message(tst_pri_message);
	pri_set_error(tst_pri_error);

	memset(&dummy_ctrl, 0, sizeof(dummy_ctrl));
	dummy_ctrl.debug = PRI_DEBUG_Q931_STATE;

	/* For sanity specify what version of libpri we are testing. */
	pri_error(&dummy_ctrl, "libpri version tested: %s\n", pri_get_version());

	/* Setup FSM */
	fsm.state = NULL;
	fsm.destructor = tst_fsm_destructor;
	fsm.name = "Test FSM";
	fsm.que = &ev_q;
	fsm.parms = &fsm_destroyed;
	fsm_init(&dummy_ctrl, &fsm, tst_state_1);

	/* Nothing to do. */
	fsm_run(&dummy_ctrl, &ev_q);

	/* Post event. */
	tst_simple_post(&dummy_ctrl, &fsm, TST_EV_A);
	fsm_run(&dummy_ctrl, &ev_q);

	/* Post event. */
	tst_simple_post(&dummy_ctrl, &fsm, TST_EV_H);
	fsm_run(&dummy_ctrl, &ev_q);

	if (fsm_destroyed != 1) {
		pri_error(&dummy_ctrl, "Test FSM destroyed %d times!\n", fsm_destroyed);
	}

	return 0;
}

/* ------------------------------------------------------------------- */
/* end fsmtest.c */
