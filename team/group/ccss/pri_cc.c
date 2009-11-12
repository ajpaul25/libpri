/*
 * libpri: An implementation of Primary Rate ISDN
 *
 * Copyright (C) 2009 Digium, Inc.
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
 * \brief Call Completion controller
 *
 * \author Richard Mudgett <rmudgett@digium.com>
 */


#include "compat.h"
#include "libpri.h"
#include "pri_internal.h"
#include "pri_q921.h"
#include "pri_q931.h"

#include <stdlib.h>


/* ------------------------------------------------------------------- */

#if defined(BUGBUG_NOT_USED_YET)
/*!
 * \internal
 * \brief Find a cc_record by the PTMP reference_id.
 *
 * \param ctrl D channel controller.
 * \param reference_id CCBS reference ID to look for in cc_record pool.
 *
 * \retval cc_record on success.
 * \retval NULL on error.
 */
static struct pri_cc_record *pri_cc_find_by_reference(struct pri *ctrl, unsigned reference_id)
{
	struct pri_cc_record *cc_record;

	ctrl = PRI_MASTER(ctrl);
	for (cc_record = ctrl->cc.pool; cc_record; cc_record = cc_record->next) {
		if (cc_record->ccbs_reference_id == reference_id) {
			/* Found the record */
			break;
		}
	}

	return cc_record;
}
#endif

/*!
 * \internal
 * \brief Find a cc_record by the PTMP linkage_id.
 *
 * \param ctrl D channel controller.
 * \param linkage_id Call linkage ID to look for in cc_record pool.
 *
 * \retval cc_record on success.
 * \retval NULL on error.
 */
static struct pri_cc_record *pri_cc_find_by_linkage(struct pri *ctrl, unsigned linkage_id)
{
	struct pri_cc_record *cc_record;

	ctrl = PRI_MASTER(ctrl);
	for (cc_record = ctrl->cc.pool; cc_record; cc_record = cc_record->next) {
		if (cc_record->call_linkage_id == linkage_id) {
			/* Found the record */
			break;
		}
	}

	return cc_record;
}

/*!
 * \internal
 * \brief Find a cc_record by the cc_id.
 *
 * \param ctrl D channel controller.
 * \param cc_id ID to look for in cc_record pool.
 *
 * \retval cc_record on success.
 * \retval NULL on error.
 */
static struct pri_cc_record *pri_cc_find_by_id(struct pri *ctrl, long cc_id)
{
	struct pri_cc_record *cc_record;

	ctrl = PRI_MASTER(ctrl);
	for (cc_record = ctrl->cc.pool; cc_record; cc_record = cc_record->next) {
		if (cc_record->record_id == cc_id) {
			/* Found the record */
			break;
		}
	}

	return cc_record;
}

#if defined(BUGBUG_NOT_USED_YET)
/*!
 * \internal
 * \brief Find a cc_record by an incoming call addressing data.
 *
 * \param ctrl D channel controller.
 * \param party_a Party A address. 
 * \param party_b Party B address.
 *
 * \retval cc_record on success.
 * \retval NULL on error.
 */
static struct pri_cc_record *pri_cc_find_by_addressing(struct pri *ctrl, const struct q931_party_address *party_a, const struct q931_party_address *party_b)
{
	struct pri_cc_record *cc_record;

	ctrl = PRI_MASTER(ctrl);
	for (cc_record = ctrl->cc.pool; cc_record; cc_record = cc_record->next) {
		if (!q931_cmp_party_id_to_address(&cc_record->party_a, party_a)
			&& !q931_party_address_cmp(&cc_record->party_b, party_b)) {
			/* Found the record */
			break;
		}
	}

	return cc_record;

	/*! \todo BUGBUG pri_cc_find_by_addressing() not written */
}
#endif

#if defined(BUGBUG_NOT_USED_YET)
/*!
 * \internal
 * \brief Allocate a new cc_record reference id.
 *
 * \param ctrl D channel controller.
 *
 * \retval reference_id on success.
 * \retval -1 on error.
 */
static int pri_cc_new_reference_id(struct pri *ctrl)
{
	long reference_id;
	long first_id;

	ctrl = PRI_MASTER(ctrl);
	ctrl->cc.last_reference_id = (ctrl->cc.last_reference_id + 1) & 0x7F;
	reference_id = ctrl->cc.last_reference_id;
	first_id = reference_id;
	while (pri_cc_find_by_reference(ctrl, reference_id)) {
		ctrl->cc.last_reference_id = (ctrl->cc.last_reference_id + 1) & 0x7F;
		reference_id = ctrl->cc.last_reference_id;
		if (reference_id == first_id) {
			/* We probably have a resource leak. */
			pri_error(ctrl, "PTMP call completion reference id exhaustion!\n");
			reference_id = -1;
			break;
		}
	}

	return reference_id;
}
#endif

/*!
 * \internal
 * \brief Allocate a new cc_record linkage id.
 *
 * \param ctrl D channel controller.
 *
 * \retval linkage_id on success.
 * \retval -1 on error.
 */
static int pri_cc_new_linkage_id(struct pri *ctrl)
{
	long linkage_id;
	long first_id;

	ctrl = PRI_MASTER(ctrl);
	ctrl->cc.last_linkage_id = (ctrl->cc.last_linkage_id + 1) & 0x7F;
	linkage_id = ctrl->cc.last_linkage_id;
	first_id = linkage_id;
	while (pri_cc_find_by_linkage(ctrl, linkage_id)) {
		ctrl->cc.last_linkage_id = (ctrl->cc.last_linkage_id + 1) & 0x7F;
		linkage_id = ctrl->cc.last_linkage_id;
		if (linkage_id == first_id) {
			/* We probably have a resource leak. */
			pri_error(ctrl, "PTMP call completion linkage id exhaustion!\n");
			linkage_id = -1;
			break;
		}
	}

	return linkage_id;
}

/*!
 * \internal
 * \brief Allocate a new cc_record id.
 *
 * \param ctrl D channel controller.
 *
 * \retval cc_id on success.
 * \retval -1 on error.
 */
static long pri_cc_new_id(struct pri *ctrl)
{
	long record_id;
	long first_id;

	ctrl = PRI_MASTER(ctrl);
	record_id = ++ctrl->cc.last_record_id;
	first_id = record_id;
	while (pri_cc_find_by_id(ctrl, record_id)) {
		record_id = ++ctrl->cc.last_record_id;
		if (record_id == first_id) {
			/*
			 * We have a resource leak.
			 * We should never need to allocate 64k records on a D channel.
			 */
			pri_error(ctrl, "Too many call completion records!\n");
			record_id = -1;
			break;
		}
	}

	return record_id;
}

/*!
 * \brief Delete the given call completion record
 *
 * \param ctrl D channel controller.
 * \param doomed Call completion record to destroy
 *
 * \return Nothing
 */
void pri_cc_delete_record(struct pri *ctrl, struct pri_cc_record *doomed)
{
	struct pri_cc_record **prev;
	struct pri_cc_record *current;

	ctrl = PRI_MASTER(ctrl);
	for (prev = &ctrl->cc.pool, current = ctrl->cc.pool; current;
		prev = &current->next, current = current->next) {
		if (current == doomed) {
			*prev = current->next;
			free(doomed);
			return;
		}
	}

	/* The doomed node is not in the call completion database */
}

/*!
 * \brief Allocate a new cc_record.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 *
 * \retval pointer to new call completion record
 * \retval NULL if failed
 */
struct pri_cc_record *pri_cc_new_record(struct pri *ctrl, q931_call *call)
{
	struct pri_cc_record *cc_record;
	long record_id;

	ctrl = PRI_MASTER(ctrl);
	record_id = pri_cc_new_id(ctrl);
	if (record_id < 0) {
		return NULL;
	}
	cc_record = calloc(1, sizeof(*cc_record));
	if (!cc_record) {
		return NULL;
	}

	/* Initialize the new record */
	cc_record->record_id = record_id;
	cc_record->call_linkage_id = CC_PTMP_INVALID_ID;/* So it will never be found this way */
	cc_record->ccbs_reference_id = CC_PTMP_INVALID_ID;/* So it will never be found this way */
	cc_record->party_a = call->cc.party_a;
	cc_record->party_b = call->called;
	cc_record->party_b_is_remote = call->cc.party_b_is_remote;
/* BUGBUG need to record BC, HLC, and LLC from initial SETUP */
/*! \todo BUGBUG need more initialization?? */

	/* Insert the new record into the database */
	cc_record->next = ctrl->cc.pool;
	ctrl->cc.pool = cc_record;

	return cc_record;
}

/*!
 * \brief Send an event from the upper layer to the cc state machine.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \retval 0 if no error or event.
 * \retval Q931_RES_HAVEEVENT if have an event.
 * \retval -1 on error.
 */
int pri_cc_event_down(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	int res;

	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		res = 0;
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		res = 0;
		break;
	default:
		res = 0;
		break;
	}

	return res;
	/*! \todo BUGBUG pri_cc_event_down() not written */
}

/*!
 * \brief Send an event from the link to the cc state machine.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \retval 0 if no error or event.
 * \retval Q931_RES_HAVEEVENT if have an event.
 * \retval -1 on error.
 */
int pri_cc_event_up(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	int res;

	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		res = 0;
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		res = 0;
		break;
	default:
		res = 0;
		break;
	}

	return res;
	/*! \todo BUGBUG pri_cc_event_up() not written */
}

/*!
 * \brief Indicate to the far end that CCBS/CCNR is available.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 *
 * \details
 * The CC available indication will go out with the next
 * DISCONNECT(busy/congested)/ALERTING message.
 *
 * \retval cc_id on success for subsequent reference.
 * \retval -1 on error.
 */
long pri_cc_available(struct pri *ctrl, q931_call *call)
{
	struct pri_cc_record *cc_record;
	long cc_id;

	if (call->cc.record) {
		/* This call is already associated with call completion. */
		return -1;
	}

	cc_record = NULL;

	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		cc_record = pri_cc_new_record(ctrl, call);
		if (!cc_record) {
			break;
		}

		/*
		 * Q.SIG has no message to send when CC is available.
		 * Q.SIG assumes CC is always available and is denied when
		 * requested if CC is not possible or allowed.
		 */
		cc_record->state = CC_STATE_AVAILABLE;
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
			int linkage_id;

			linkage_id = pri_cc_new_linkage_id(ctrl);
			if (linkage_id < 0) {
				break;
			}
			cc_record = pri_cc_new_record(ctrl, call);
			if (!cc_record) {
				break;
			}
			cc_record->signaling = PRI_MASTER(ctrl)->dummy_call;
			cc_record->call_linkage_id = linkage_id;
		} else {
			cc_record = pri_cc_new_record(ctrl, call);
			if (!cc_record) {
				break;
			}
		}
		cc_record->state = CC_STATE_PENDING_AVAILABLE;
		break;
	default:
		break;
	}

	call->cc.record = cc_record;
	if (cc_record) {
		cc_id = cc_record->record_id;
	} else {
		cc_id = -1;
	}
	return cc_id;
}

/*!
 * \brief Request to activate CC.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to activate.
 * \param mode Which CC mode to use CCBS(0)/CCNR(1)
 *
 * \note
 * Will always get a reply from libpri.  libpri will start a timer to guarantee
 * that a reply will be passed back to the upper layer.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
int pri_cc_req(struct pri *ctrl, long cc_id, int mode)
{
	struct pri_sr req;
	q931_call *call;
	struct pri_cc_record *cc_record;

	cc_record = pri_cc_find_by_id(ctrl, cc_id);
	if (!cc_record) {
		return -1;
	}

	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		call = q931_new_call(ctrl);
		if (!call) {
			return -1;
		}

/* BUGBUG */
		//add_qsigCcRequestArg_facility_ie(ctrl, call, Q931_SETUP, cc_record, mode);

		pri_sr_init(&req);
		req.caller = cc_record->party_a;
		req.called = cc_record->party_b;
		req.numcomplete = 1;
		req.cis_auto_disconnect = 0;
		req.cis_call = 1;
		if (q931_setup(ctrl, call, &req)) {
			return -1;
		}
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
		} else {
		}
		return -1;
		break;
	default:
		return -1;
	}

	cc_record->state = CC_STATE_REQUESTED;

	return 0;

	/*! \todo BUGBUG pri_cc_req() not written */
}

/*!
 * \brief Response to an incoming CC activation request.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to activate.
 * \param status success(0)/timeout(1)/
 *		short_term_denial(2)/long_term_denial(3)/not_subscribed(4)
 *
 * \return Nothing
 */
void pri_cc_req_rsp(struct pri *ctrl, long cc_id, int status)
{
	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
		} else {
		}
		break;
	default:
		break;
	}

	/*! \todo BUGBUG pri_cc_req_rsp() not written */
}

/*!
 * \brief Indicate that the remote user (Party B) is free to call.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to activate.
 *
 * \return Nothing
 */
void pri_cc_remote_user_free(struct pri *ctrl, long cc_id)
{
	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
		} else {
		}
		break;
	default:
		break;
	}

	/*! \todo BUGBUG pri_cc_remote_user_free() not written */
}

/*!
 * \brief Poll/Ping for the status of CC party A.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to activate.
 *
 * \note
 * There could be zero, one, or more PRI_SUBCMD_CC_STATUS_REQ_RSP responses to
 * the status request depending upon how many endpoints respond to the request.
 * \note
 * This is expected to be called only if there are two PTMP links between
 * party A and the network.  (e.g., A --> * --> PSTN)
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
int pri_cc_status_req(struct pri *ctrl, long cc_id)
{
	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
		} else {
		}
		break;
	default:
		break;
	}

	/*! \todo BUGBUG pri_cc_status_req() not written */
	return -1;
}

/*!
 * \brief Update the busy status of CC party A.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to activate.
 * \param status Updated party A status free(0)/busy(1)
 *
 * \note
 * This is expected to be called only if there are two PTMP links between
 * party A and the network.  (e.g., A --> * --> PSTN)
 *
 * \return Nothing
 */
void pri_cc_status_req_rsp(struct pri *ctrl, long cc_id, int status)
{
	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
		} else {
		}
		break;
	default:
		break;
	}

	/*! \todo BUGBUG pri_cc_status_req() not written */
}

/*!
 * \brief Update the busy status of CC party A.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to activate.
 * \param status Updated party A status free(0)/busy(1)
 *
 * \note
 * Party A status is used to suspend/resume monitoring party B.
 *
 * \return Nothing
 */
void pri_cc_status(struct pri *ctrl, long cc_id, int status)
{
	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
		} else {
		}
		break;
	default:
		break;
	}

	/*! \todo BUGBUG pri_cc_status() not written */
}

/*!
 * \brief Initiate the CC callback call.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to activate.
 * \param call Q.931 call leg.
 * \param channel Encoded B channel to use.
 * \param exclusive TRUE if the specified B channel must be used.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
int pri_cc_call(struct pri *ctrl, long cc_id, q931_call *call, int channel, int exclusive)
{
	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
		} else {
		}
		break;
	default:
		break;
	}

	/*! \todo BUGBUG pri_cc_call() not written */
	return -1;
}

/*!
 * \brief Unsolicited indication that CC is cancelled.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to deactivate.
 *
 * \return Nothing.  The cc_id is no longer valid.
 */
void pri_cc_cancel(struct pri *ctrl, long cc_id)
{
	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
		} else {
		}
		break;
	default:
		break;
	}

	/*! \todo BUGBUG pri_cc_cancel() not written */
}

/*!
 * \brief Request that the CC be canceled.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to request deactivation.
 *
 * \note
 * Will always get a PRI_SUBCMD_CC_DEACTIVATE_RSP from libpri.
 * \note
 * Deactivate is used in the party A to party B direction.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
int pri_cc_deactivate_req(struct pri *ctrl, long cc_id)
{
	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
		} else {
		}
		break;
	default:
		break;
	}

	/*! \todo BUGBUG pri_cc_deactivate_req() not written */
	return -1;
}

/* ------------------------------------------------------------------- */
/* end pri_cc.c */
