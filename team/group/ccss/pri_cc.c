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
#include "pri_facility.h"

#include <stdlib.h>


/* ------------------------------------------------------------------- */

/*!
 * \brief Find a cc_record by the PTMP reference_id.
 *
 * \param ctrl D channel controller.
 * \param reference_id CCBS reference ID to look for in cc_record pool.
 *
 * \retval cc_record on success.
 * \retval NULL on error.
 */
struct pri_cc_record *pri_cc_find_by_reference(struct pri *ctrl, unsigned reference_id)
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

/*!
 * \brief Find a cc_record by the PTMP linkage_id.
 *
 * \param ctrl D channel controller.
 * \param linkage_id Call linkage ID to look for in cc_record pool.
 *
 * \retval cc_record on success.
 * \retval NULL on error.
 */
struct pri_cc_record *pri_cc_find_by_linkage(struct pri *ctrl, unsigned linkage_id)
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

/*!
 * \brief Find a cc_record by an incoming call addressing data.
 *
 * \param ctrl D channel controller.
 * \param party_a Party A address.
 * \param party_b Party B address.
 *
 * \retval cc_record on success.
 * \retval NULL on error.
 */
struct pri_cc_record *pri_cc_find_by_addressing(struct pri *ctrl, const struct q931_party_address *party_a, const struct q931_party_address *party_b)
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

	/* BUGBUG Probably need to add BC, HLC, and LLC comparison as well. */
	/*! \todo BUGBUG pri_cc_find_by_addressing() not written */
}

/*!
 * \brief Allocate a new cc_record reference id.
 *
 * \param ctrl D channel controller.
 *
 * \retval reference_id on success.
 * \retval CC_PTMP_INVALID_ID on error.
 */
int pri_cc_new_reference_id(struct pri *ctrl)
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
			reference_id = CC_PTMP_INVALID_ID;
			break;
		}
	}

	return reference_id;
}

/*!
 * \internal
 * \brief Allocate a new cc_record linkage id.
 *
 * \param ctrl D channel controller.
 *
 * \retval linkage_id on success.
 * \retval CC_PTMP_INVALID_ID on error.
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
			linkage_id = CC_PTMP_INVALID_ID;
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
	cc_record->saved_ie_contents = call->cc.saved_ie_contents;
	cc_record->bc = call->bc;
/*! \todo BUGBUG need more initialization?? */

	/*
	 * Append the new record to the end of the list so they are in
	 * cronological order for interrogations.
	 */
	if (ctrl->cc.pool) {
		struct pri_cc_record *cur;

		for (cur = ctrl->cc.pool; cur->next; cur = cur->next) {
		}
		cur->next = cc_record;
	} else {
		ctrl->cc.pool = cc_record;
	}

	return cc_record;
}

/*!
 * \internal
 * \brief Encode ETSI PTP call completion event operation message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param operation PTP call completion event operation to encode.
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_etsi_ptp_cc_operation(struct pri *ctrl, unsigned char *pos,
	unsigned char *end, enum rose_operation operation)
{
	struct rose_msg_invoke msg;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = get_invokeid(ctrl);
	msg.operation = operation;

	pos = rose_encode_invoke(ctrl, pos, end, &msg);

	return pos;
}

/*!
 * \internal
 * \brief Encode ETSI PTMP call completion available message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param cc_record Call completion record to process event.
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_etsi_ptmp_cc_available(struct pri *ctrl, unsigned char *pos,
	unsigned char *end, struct pri_cc_record *cc_record)
{
	struct rose_msg_invoke msg;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = get_invokeid(ctrl);
	msg.operation = ROSE_ETSI_CallInfoRetain;

	msg.args.etsi.CallInfoRetain.call_linkage_id = cc_record->call_linkage_id;

	pos = rose_encode_invoke(ctrl, pos, end, &msg);

	return pos;
}

/*!
 * \internal
 * \brief Encode and queue a cc-available message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode call completion available.
 * \param cc_record Call completion record to process event.
 * \param msgtype Q.931 message type to put facility ie in.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_cc_available_encode(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, int msgtype)
{
	unsigned char buffer[256];
	unsigned char *end;

	switch (ctrl->switchtype) {
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
			end =
				enc_etsi_ptmp_cc_available(ctrl, buffer, buffer + sizeof(buffer),
					cc_record);
		} else {
			end =
				enc_etsi_ptp_cc_operation(ctrl, buffer, buffer + sizeof(buffer),
					ROSE_ETSI_CCBS_T_Available);
		}
		break;
	case PRI_SWITCH_QSIG:
		/* Q.SIG does not have a cc-available type message. */
		return 0;
	default:
		return -1;
	}
	if (!end) {
		return -1;
	}

	return pri_call_apdu_queue(call, msgtype, buffer, end - buffer, NULL);
}

/*!
 * \internal
 * \brief Encode ETSI PTMP EraseCallLinkageID message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param cc_record Call completion record to process event.
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_etsi_ptmp_erase_call_linkage(struct pri *ctrl,
	unsigned char *pos, unsigned char *end, struct pri_cc_record *cc_record)
{
	struct rose_msg_invoke msg;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = get_invokeid(ctrl);
	msg.operation = ROSE_ETSI_EraseCallLinkageID;

	msg.args.etsi.EraseCallLinkageID.call_linkage_id = cc_record->call_linkage_id;

	pos = rose_encode_invoke(ctrl, pos, end, &msg);

	return pos;
}

/*!
 * \internal
 * \brief Encode and queue an EraseCallLinkageID message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode EraseCallLinkageID.
 * \param cc_record Call completion record to process event.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_erase_call_linkage_encode(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record)
{
	unsigned char buffer[256];
	unsigned char *end;

	end =
		enc_etsi_ptmp_erase_call_linkage(ctrl, buffer, buffer + sizeof(buffer),
			cc_record);
	if (!end) {
		return -1;
	}

	return pri_call_apdu_queue(call, Q931_FACILITY, buffer, end - buffer, NULL);
}

/*!
 * \internal
 * \brief Encode and send an EraseCallLinkageID message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode EraseCallLinkageID.
 * \param cc_record Call completion record to process event.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int send_erase_call_linkage_id(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record)
{
	if (rose_erase_call_linkage_encode(ctrl, call, cc_record)
		|| q931_facility(ctrl, call)) {
		pri_message(ctrl,
			"Could not schedule facility message for EraseCallLinkageID.\n");
		return -1;
	}

	return 0;
}

/*!
 * \internal
 * \brief Encode ETSI PTMP CCBSErase message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param cc_record Call completion record to process event.
 * \param reason CCBS Erase reason
 *  normal-unspecified(0), t-CCBS2-timeout(1), t-CCBS3-timeout(2), basic-call-failed(3)
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_etsi_ptmp_ccbs_erase(struct pri *ctrl,
	unsigned char *pos, unsigned char *end, struct pri_cc_record *cc_record, int reason)
{
	struct rose_msg_invoke msg;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = get_invokeid(ctrl);
	msg.operation = ROSE_ETSI_CCBSErase;

	if (cc_record->saved_ie_contents.length
		<= sizeof(msg.args.etsi.CCBSErase.q931ie_contents)) {
		/* Saved BC, HLC, and LLC from initial SETUP */
		msg.args.etsi.CCBSErase.q931ie.length = cc_record->saved_ie_contents.length;
		memcpy(msg.args.etsi.CCBSErase.q931ie.contents, cc_record->saved_ie_contents.data,
			cc_record->saved_ie_contents.length);
	} else {
		pri_error(ctrl, "CCBSErase q931 ie contents did not fit.\n");
	}

	q931_copy_address_to_rose(ctrl, &msg.args.etsi.CCBSErase.address_of_b,
		&cc_record->party_b);
	msg.args.etsi.CCBSErase.recall_mode = cc_record->option.recall_mode;
	msg.args.etsi.CCBSErase.ccbs_reference = cc_record->ccbs_reference_id;
	msg.args.etsi.CCBSErase.reason = reason;

	pos = rose_encode_invoke(ctrl, pos, end, &msg);

	return pos;
}

/*!
 * \internal
 * \brief Encode and queue an CCBSErase message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode CCBSErase.
 * \param cc_record Call completion record to process event.
 * \param reason CCBS Erase reason
 *  normal-unspecified(0), t-CCBS2-timeout(1), t-CCBS3-timeout(2), basic-call-failed(3)
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_ccbs_erase_encode(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, int reason)
{
	unsigned char buffer[256];
	unsigned char *end;

	end =
		enc_etsi_ptmp_ccbs_erase(ctrl, buffer, buffer + sizeof(buffer), cc_record,
			reason);
	if (!end) {
		return -1;
	}

	return pri_call_apdu_queue(call, Q931_FACILITY, buffer, end - buffer, NULL);
}

/*!
 * \internal
 * \brief Encode and send an CCBSErase message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode EraseCallLinkageID.
 * \param cc_record Call completion record to process event.
 * \param reason CCBS Erase reason
 *  normal-unspecified(0), t-CCBS2-timeout(1), t-CCBS3-timeout(2), basic-call-failed(3)
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int send_ccbs_erase(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, int reason)
{
/*
 * XXX May need to add called-party-ie with Party A number in FACILITY message. (CCBSErase)
 * ETSI EN 300-195-1 Section 5.41 MSN interaction.
 */
	if (rose_ccbs_erase_encode(ctrl, call, cc_record, reason)
		|| q931_facility(ctrl, call)) {
		pri_message(ctrl,
			"Could not schedule facility message for CCBSErase.\n");
		return -1;
	}

	return 0;
}

/*!
 * \internal
 * \brief Encode ETSI PTMP CCBSStatusRequest result message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param cc_record Call completion record to process event.
 * \param is_free TRUE if the Party A status is available.
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_etsi_ptmp_ccbs_status_request_rsp(struct pri *ctrl,
	unsigned char *pos, unsigned char *end, struct pri_cc_record *cc_record, int is_free)
{
	struct rose_msg_result msg;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = cc_record->response.invoke_id;
	msg.operation = ROSE_ETSI_CCBSStatusRequest;

	msg.args.etsi.CCBSStatusRequest.free = is_free;

	pos = rose_encode_result(ctrl, pos, end, &msg);

	return pos;
}

/*!
 * \internal
 * \brief Encode ETSI PTMP CCBSStatusRequest message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param cc_record Call completion record to process event.
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_etsi_ptmp_ccbs_status_request(struct pri *ctrl,
	unsigned char *pos, unsigned char *end, struct pri_cc_record *cc_record)
{
	struct rose_msg_invoke msg;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = get_invokeid(ctrl);
	msg.operation = ROSE_ETSI_CCBSStatusRequest;

	if (cc_record->saved_ie_contents.length
		<= sizeof(msg.args.etsi.CCBSStatusRequest.q931ie_contents)) {
		/* Saved BC, HLC, and LLC from initial SETUP */
		msg.args.etsi.CCBSStatusRequest.q931ie.length =
			cc_record->saved_ie_contents.length;
		memcpy(msg.args.etsi.CCBSStatusRequest.q931ie.contents,
			cc_record->saved_ie_contents.data, cc_record->saved_ie_contents.length);
	} else {
		pri_error(ctrl, "CCBSStatusRequest q931 ie contents did not fit.\n");
	}

	msg.args.etsi.CCBSStatusRequest.recall_mode = cc_record->option.recall_mode;
	msg.args.etsi.CCBSStatusRequest.ccbs_reference = cc_record->ccbs_reference_id;

	pos = rose_encode_invoke(ctrl, pos, end, &msg);

	return pos;
}

/*!
 * \internal
 * \brief Encode ETSI PTMP CCBSBFree message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param cc_record Call completion record to process event.
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_etsi_ptmp_ccbs_b_free(struct pri *ctrl,
	unsigned char *pos, unsigned char *end, struct pri_cc_record *cc_record)
{
	struct rose_msg_invoke msg;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = get_invokeid(ctrl);
	msg.operation = ROSE_ETSI_CCBSBFree;

	if (cc_record->saved_ie_contents.length
		<= sizeof(msg.args.etsi.CCBSBFree.q931ie_contents)) {
		/* Saved BC, HLC, and LLC from initial SETUP */
		msg.args.etsi.CCBSBFree.q931ie.length = cc_record->saved_ie_contents.length;
		memcpy(msg.args.etsi.CCBSBFree.q931ie.contents, cc_record->saved_ie_contents.data,
			cc_record->saved_ie_contents.length);
	} else {
		pri_error(ctrl, "CCBSBFree q931 ie contents did not fit.\n");
	}

	q931_copy_address_to_rose(ctrl, &msg.args.etsi.CCBSBFree.address_of_b,
		&cc_record->party_b);
	msg.args.etsi.CCBSBFree.recall_mode = cc_record->option.recall_mode;
	msg.args.etsi.CCBSBFree.ccbs_reference = cc_record->ccbs_reference_id;

	pos = rose_encode_invoke(ctrl, pos, end, &msg);

	return pos;
}

/*!
 * \internal
 * \brief Encode and queue an CCBSBFree message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode CCBSBFree.
 * \param cc_record Call completion record to process event.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_ccbs_b_free_encode(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record)
{
	unsigned char buffer[256];
	unsigned char *end;

	end =
		enc_etsi_ptmp_ccbs_b_free(ctrl, buffer, buffer + sizeof(buffer), cc_record);
	if (!end) {
		return -1;
	}

	return pri_call_apdu_queue(call, Q931_FACILITY, buffer, end - buffer, NULL);
}

/*!
 * \internal
 * \brief Encode and send an CCBSBFree message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode CCBSBFree.
 * \param cc_record Call completion record to process event.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int send_ccbs_b_free(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record)
{
/*
 * XXX May need to add called-party-ie with Party A number in FACILITY message. (CCBSBFree)
 * ETSI EN 300-195-1 Section 5.41 MSN interaction.
 */
	if (rose_ccbs_b_free_encode(ctrl, call, cc_record)
		|| q931_facility(ctrl, call)) {
		pri_message(ctrl,
			"Could not schedule facility message for CCBSBFree.\n");
		return -1;
	}

	return 0;
}

/*!
 * \internal
 * \brief Encode ETSI PTMP CCBSRemoteUserFree message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param cc_record Call completion record to process event.
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_etsi_ptmp_remote_user_free(struct pri *ctrl,
	unsigned char *pos, unsigned char *end, struct pri_cc_record *cc_record)
{
	struct rose_msg_invoke msg;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = get_invokeid(ctrl);
	msg.operation = ROSE_ETSI_CCBSRemoteUserFree;

	if (cc_record->saved_ie_contents.length
		<= sizeof(msg.args.etsi.CCBSRemoteUserFree.q931ie_contents)) {
		/* Saved BC, HLC, and LLC from initial SETUP */
		msg.args.etsi.CCBSRemoteUserFree.q931ie.length =
			cc_record->saved_ie_contents.length;
		memcpy(msg.args.etsi.CCBSRemoteUserFree.q931ie.contents,
			cc_record->saved_ie_contents.data, cc_record->saved_ie_contents.length);
	} else {
		pri_error(ctrl, "CCBSRemoteUserFree q931 ie contents did not fit.\n");
	}

	q931_copy_address_to_rose(ctrl, &msg.args.etsi.CCBSRemoteUserFree.address_of_b,
		&cc_record->party_b);
	msg.args.etsi.CCBSRemoteUserFree.recall_mode = cc_record->option.recall_mode;
	msg.args.etsi.CCBSRemoteUserFree.ccbs_reference = cc_record->ccbs_reference_id;

	pos = rose_encode_invoke(ctrl, pos, end, &msg);

	return pos;
}

/*!
 * \internal
 * \brief Encode and queue a remote user free message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode remote user free message.
 * \param cc_record Call completion record to process event.
 * \param msgtype Q.931 message type to put facility ie in.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_remote_user_free_encode(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, int msgtype)
{
	unsigned char buffer[256];
	unsigned char *end;

	switch (ctrl->switchtype) {
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
			end =
				enc_etsi_ptmp_remote_user_free(ctrl, buffer, buffer + sizeof(buffer),
					cc_record);
		} else {
			end =
				enc_etsi_ptp_cc_operation(ctrl, buffer, buffer + sizeof(buffer),
					ROSE_ETSI_CCBS_T_RemoteUserFree);
		}
		break;
	case PRI_SWITCH_QSIG:
		/* \todo BUGBUG rose_remote_user_free_encode(Q.SIG) not written. */
		return -1;
	default:
		return -1;
	}
	if (!end) {
		return -1;
	}

	return pri_call_apdu_queue(call, msgtype, buffer, end - buffer, NULL);
}

/*!
 * \internal
 * \brief Encode and send an remote user free message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode remote user free.
 * \param cc_record Call completion record to process event.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int send_remote_user_free(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record)
{
/*
 * XXX May need to add called-party-ie with Party A number in FACILITY message. (CCBSRemoteUserFree)
 * ETSI EN 300-195-1 Section 5.41 MSN interaction.
 */
	if (rose_remote_user_free_encode(ctrl, call, cc_record, Q931_FACILITY)
		|| q931_facility(ctrl, call)) {
		pri_message(ctrl,
			"Could not schedule facility message for remote user free.\n");
		return -1;
	}

	return 0;
}

/*!
 * \internal
 * \brief Encode ETSI PTMP CCBSStopAlerting message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param cc_record Call completion record to process event.
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_etsi_ptmp_ccbs_stop_alerting(struct pri *ctrl,
	unsigned char *pos, unsigned char *end, struct pri_cc_record *cc_record)
{
	struct rose_msg_invoke msg;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = get_invokeid(ctrl);
	msg.operation = ROSE_ETSI_CCBSStopAlerting;

	msg.args.etsi.CCBSStopAlerting.ccbs_reference = cc_record->ccbs_reference_id;

	pos = rose_encode_invoke(ctrl, pos, end, &msg);

	return pos;
}

/*!
 * \internal
 * \brief Encode and queue an CCBSStopAlerting message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode CCBSStopAlerting.
 * \param cc_record Call completion record to process event.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_ccbs_stop_alerting_encode(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record)
{
	unsigned char buffer[256];
	unsigned char *end;

	end =
		enc_etsi_ptmp_ccbs_stop_alerting(ctrl, buffer, buffer + sizeof(buffer), cc_record);
	if (!end) {
		return -1;
	}

	return pri_call_apdu_queue(call, Q931_FACILITY, buffer, end - buffer, NULL);
}

/*!
 * \internal
 * \brief Encode and send CCBSStopAlerting message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode remote user free.
 * \param cc_record Call completion record to process event.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int send_ccbs_stop_alerting(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record)
{
	if (rose_ccbs_stop_alerting_encode(ctrl, call, cc_record)
		|| q931_facility(ctrl, call)) {
		pri_message(ctrl,
			"Could not schedule facility message for CCBSStopAlerting.\n");
		return -1;
	}

	return 0;
}

/*!
 * \internal
 * \brief Copy the cc information into the ETSI ROSE call-information record.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call_information ROSE call-information record to fill in.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void q931_copy_call_information_to_etsi_rose(struct pri *ctrl, struct roseEtsiCallInformation *call_information, const struct pri_cc_record *cc_record)
{
	q931_copy_address_to_rose(ctrl, &call_information->address_of_b, &cc_record->party_b);

	if (cc_record->saved_ie_contents.length
		<= sizeof(call_information->q931ie_contents)) {
		/* Saved BC, HLC, and LLC from initial SETUP */
		call_information->q931ie.length = cc_record->saved_ie_contents.length;
		memcpy(call_information->q931ie.contents, cc_record->saved_ie_contents.data,
			cc_record->saved_ie_contents.length);
	} else {
		pri_error(ctrl, "call-information q931 ie contents did not fit.\n");
	}

	call_information->ccbs_reference = cc_record->ccbs_reference_id;

	q931_copy_subaddress_to_rose(ctrl, &call_information->subaddress_of_a,
		&cc_record->party_a.subaddress);
}

/*!
 * \internal
 * \brief Encode ETSI PTMP specific CCBSInterrogate/CCNRInterrogate result message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param invoke Decoded ROSE invoke message contents.
 * \param cc_record Call completion record to process event.
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_etsi_ptmp_cc_interrogate_rsp_specific(struct pri *ctrl,
	unsigned char *pos, unsigned char *end, const struct rose_msg_invoke *invoke,
	const struct pri_cc_record *cc_record)
{
	struct rose_msg_result msg;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = invoke->invoke_id;
	msg.operation = invoke->operation;

	msg.args.etsi.CCBSInterrogate.recall_mode = cc_record->option.recall_mode;
	msg.args.etsi.CCBSInterrogate.call_details.num_records = 1;
	q931_copy_call_information_to_etsi_rose(ctrl,
		&msg.args.etsi.CCBSInterrogate.call_details.list[0], cc_record);

	pos = rose_encode_result(ctrl, pos, end, &msg);

	return pos;
}

/*!
 * \internal
 * \brief Encode ETSI PTMP general CCBSInterrogate/CCNRInterrogate result message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param invoke Decoded ROSE invoke message contents.
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_etsi_ptmp_cc_interrogate_rsp_general(struct pri *ctrl,
	unsigned char *pos, unsigned char *end, const struct rose_msg_invoke *invoke)
{
	struct rose_msg_result msg;
	struct q931_party_number party_a_number;
	const struct pri_cc_record *cc_record;
	unsigned char *new_pos;
	struct pri *master;
	unsigned idx;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = invoke->invoke_id;
	msg.operation = invoke->operation;

	master = PRI_MASTER(ctrl);
	msg.args.etsi.CCBSInterrogate.recall_mode = master->cc.option.recall_mode;

	/* Convert the given party A number. */
	q931_party_number_init(&party_a_number);
	if (invoke->args.etsi.CCBSInterrogate.a_party_number.length) {
		/* The party A number was given. */
		rose_copy_number_to_q931(ctrl, &party_a_number,
			&invoke->args.etsi.CCBSInterrogate.a_party_number);
	}

	/* Build the CallDetails list. */
	idx = 0;
	for (cc_record = master->cc.pool; cc_record; cc_record = cc_record->next) {
		if (cc_record->ccbs_reference_id == CC_PTMP_INVALID_ID
			|| (!cc_record->is_ccnr) != (invoke->operation == ROSE_ETSI_CCBSInterrogate)) {
			/*
			 * Record does not have a reference id yet
			 * or is not for the requested CCBS/CCNR mode.
			 */
			continue;
		}
		if (party_a_number.valid) {
			/* The party A number was given. */
			party_a_number.presentation = cc_record->party_a.number.presentation;
			if (q931_party_number_cmp(&party_a_number, &cc_record->party_a.number)) {
				/* Record party A does not match. */
				continue;
			}
		}

		/* Add call information to the CallDetails list. */
		q931_copy_call_information_to_etsi_rose(ctrl,
			&msg.args.etsi.CCBSInterrogate.call_details.list[idx], cc_record);

		++idx;
		if (ARRAY_LEN(msg.args.etsi.CCBSInterrogate.call_details.list) <= idx) {
			/* List is full. */
			break;
		}
	}
	msg.args.etsi.CCBSInterrogate.call_details.num_records = idx;

	new_pos = rose_encode_result(ctrl, pos, end, &msg);

	/* Reduce the CallDetails list until it fits into the given buffer. */
	while (!new_pos && msg.args.etsi.CCBSInterrogate.call_details.num_records) {
		--msg.args.etsi.CCBSInterrogate.call_details.num_records;
		new_pos = rose_encode_result(ctrl, pos, end, &msg);
	}

	return new_pos;
}

/*!
 * \internal
 * \brief Encode and queue a specific CCBSInterrogate/CCNRInterrogate result message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode CCBSInterrogate/CCNRInterrogate response.
 * \param invoke Decoded ROSE invoke message contents.
 * \param cc_record Call completion record to process event.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_cc_interrogate_rsp_specific(struct pri *ctrl, q931_call *call, const struct rose_msg_invoke *invoke, const struct pri_cc_record *cc_record)
{
	unsigned char buffer[256];
	unsigned char *end;

	end =
		enc_etsi_ptmp_cc_interrogate_rsp_specific(ctrl, buffer, buffer + sizeof(buffer),
			invoke, cc_record);
	if (!end) {
		return -1;
	}

	return pri_call_apdu_queue(call, Q931_FACILITY, buffer, end - buffer, NULL);
}

/*!
 * \internal
 * \brief Encode and queue a general CCBSInterrogate/CCNRInterrogate result message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode CCBSInterrogate/CCNRInterrogate response.
 * \param invoke Decoded ROSE invoke message contents.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_cc_interrogate_rsp_general(struct pri *ctrl, q931_call *call, const struct rose_msg_invoke *invoke)
{
	unsigned char buffer[256];
	unsigned char *end;

	end =
		enc_etsi_ptmp_cc_interrogate_rsp_general(ctrl, buffer, buffer + sizeof(buffer),
			invoke);
	if (!end) {
		return -1;
	}

	return pri_call_apdu_queue(call, Q931_FACILITY, buffer, end - buffer, NULL);
}

/*!
 * \brief Respond to the received CCBSInterrogate/CCNRInterrogate invoke message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which the message came.
 * \param invoke Decoded ROSE invoke message contents.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
int pri_cc_interrogate_rsp(struct pri *ctrl, q931_call *call, const struct rose_msg_invoke *invoke)
{
	int encode_result;

	if (!PRI_MASTER(ctrl)->cc_support) {
		/* Call completion is disabled. */
		return send_facility_error(ctrl, call, invoke->invoke_id,
			ROSE_ERROR_Gen_NotSubscribed);
	}

	if (invoke->args.etsi.CCBSInterrogate.ccbs_reference_present) {
		struct pri_cc_record *cc_record;

		/* Specific CC request interrogation. */
		cc_record = pri_cc_find_by_reference(ctrl,
			invoke->args.etsi.CCBSInterrogate.ccbs_reference);
		if (!cc_record
			|| ((!cc_record->is_ccnr)
				== (invoke->operation == ROSE_ETSI_CCBSInterrogate))) {
			/* Record does not exist or is not for the requested CCBS/CCNR mode. */
			return send_facility_error(ctrl, call, invoke->invoke_id,
				ROSE_ERROR_CCBS_InvalidCCBSReference);
		}
		encode_result = rose_cc_interrogate_rsp_specific(ctrl, call, invoke, cc_record);
	} else {
		/* General CC request interrogation. */
		encode_result = rose_cc_interrogate_rsp_general(ctrl, call, invoke);
	}

	if (encode_result || q931_facility(ctrl, call)) {
		pri_message(ctrl,
			"Could not schedule facility message for cc-interrogate.\n");
		return -1;
	}

	return 0;
}

/*!
 * \brief Respond to the received CCBSRequest/CCNRRequest invoke message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which the message came.
 * \param invoke Decoded ROSE invoke message contents.
 *
 * \return Nothing
 */
void pri_cc_request(struct pri *ctrl, q931_call *call, const struct rose_msg_invoke *invoke)
{
	struct pri_cc_record *cc_record;

	cc_record = pri_cc_find_by_linkage(ctrl,
		invoke->args.etsi.CCBSRequest.call_linkage_id);
	if (!cc_record) {
		send_facility_error(ctrl, call, invoke->invoke_id,
			ROSE_ERROR_CCBS_InvalidCallLinkageID);
		return;
	}
	if (cc_record->state != CC_STATE_AVAILABLE) {
		send_facility_error(ctrl, call, invoke->invoke_id,
			ROSE_ERROR_CCBS_IsAlreadyActivated);
		return;
	}
	cc_record->ccbs_reference_id = pri_cc_new_reference_id(ctrl);
	if (cc_record->ccbs_reference_id == CC_PTMP_INVALID_ID) {
		/* Could not allocate a call reference id. */
		send_facility_error(ctrl, call, invoke->invoke_id,
			ROSE_ERROR_CCBS_OutgoingCCBSQueueFull);
		return;
	}

	/* Save off data to know how to send back any response. */
	cc_record->response.signaling = call;
	cc_record->response.invoke_operation = invoke->operation;
	cc_record->response.invoke_id = invoke->invoke_id;

	/* Set the requested CC mode. */
	cc_record->is_ccnr = (invoke->operation == ROSE_ETSI_CCNRRequest) ? 1 : 0;

	pri_cc_event(ctrl, call, cc_record, CC_EVENT_CC_REQUEST);
}

/*!
 * \internal
 * \brief Convert the given call completion state to a string.
 *
 * \param state CC state to convert to string.
 *
 * \return String version of call completion state.
 */
static const char *pri_cc_fsm_state_str(enum CC_STATES state)
{
	const char *str;

	str = "Unknown";
	switch (state) {
	case CC_STATE_IDLE:
		str = "CC_STATE_IDLE";
		break;
	case CC_STATE_PENDING_AVAILABLE:
		str = "CC_STATE_PENDING_AVAILABLE";
		break;
	case CC_STATE_AVAILABLE:
		str = "CC_STATE_AVAILABLE";
		break;
	case CC_STATE_REQUESTED:
		str = "CC_STATE_REQUESTED";
		break;
	case CC_STATE_ACTIVATED:
		str = "CC_STATE_ACTIVATED";
		break;
	case CC_STATE_B_AVAILABLE:
		str = "CC_STATE_B_AVAILABLE";
		break;
	case CC_STATE_SUSPENDED:
		str = "CC_STATE_SUSPENDED";
		break;
	case CC_STATE_WAIT_CALLBACK:
		str = "CC_STATE_WAIT_CALLBACK";
		break;
	case CC_STATE_CALLBACK:
		str = "CC_STATE_CALLBACK";
		break;
	case CC_STATE_WAIT_DESTRUCTION:
		str = "CC_STATE_WAIT_DESTRUCTION";
		break;
	case CC_STATE_NUM:
		/* Not a real state. */
		break;
	}
	return str;
}

/*!
 * \internal
 * \brief Convert the given call completion event to a string.
 *
 * \param event CC event to convert to string.
 *
 * \return String version of call completion event.
 */
static const char *pri_cc_fsm_event_str(enum CC_EVENTS event)
{
	const char *str;

	str = "Unknown";
	switch (event) {
	case CC_EVENT_AVAILABLE:
		str = "CC_EVENT_AVAILABLE";
		break;
	case CC_EVENT_CC_REQUEST:
		str = "CC_EVENT_CC_REQUEST";
		break;
	case CC_EVENT_CC_REQUEST_ACCEPT:
		str = "CC_EVENT_CC_REQUEST_ACCEPT";
		break;
	case CC_EVENT_CC_REQUEST_FAIL:
		str = "CC_EVENT_CC_REQUEST_FAIL";
		break;
	case CC_EVENT_REMOTE_USER_FREE:
		str = "CC_EVENT_REMOTE_USER_FREE";
		break;
	case CC_EVENT_B_FREE:
		str = "CC_EVENT_B_FREE";
		break;
	case CC_EVENT_A_STATUS:
		str = "CC_EVENT_A_STATUS";
		break;
	case CC_EVENT_A_FREE:
		str = "CC_EVENT_A_FREE";
		break;
	case CC_EVENT_A_BUSY:
		str = "CC_EVENT_A_BUSY";
		break;
	case CC_EVENT_SUSPEND:
		str = "CC_EVENT_SUSPEND";
		break;
	case CC_EVENT_RESUME:
		str = "CC_EVENT_RESUME";
		break;
	case CC_EVENT_RECALL:
		str = "CC_EVENT_RECALL";
		break;
	case CC_EVENT_LINK_CANCEL:
		str = "CC_EVENT_LINK_CANCEL";
		break;
	case CC_EVENT_CANCEL:
		str = "CC_EVENT_CANCEL";
		break;
	case CC_EVENT_SIGNALING_GONE:
		str = "CC_EVENT_SIGNALING_GONE";
		break;
	case CC_EVENT_MSG_ALERTING:
		str = "CC_EVENT_MSG_ALERTING";
		break;
	case CC_EVENT_MSG_DISCONNECT:
		str = "CC_EVENT_MSG_DISCONNECT";
		break;
	case CC_EVENT_MSG_RELEASE:
		str = "CC_EVENT_MSG_RELEASE";
		break;
	case CC_EVENT_MSG_RELEASE_COMPLETE:
		str = "CC_EVENT_MSG_RELEASE_COMPLETE";
		break;
	case CC_EVENT_TIMEOUT_T_RETENTION:
		str = "CC_EVENT_TIMEOUT_T_RETENTION";
		break;
	case CC_EVENT_TIMEOUT_T_CCBS1:
		str = "CC_EVENT_TIMEOUT_T_CCBS1";
		break;
	case CC_EVENT_TIMEOUT_EXTENDED_T_CCBS1:
		str = "CC_EVENT_TIMEOUT_EXTENDED_T_CCBS1";
		break;
	case CC_EVENT_TIMEOUT_T_CCBS2:
		str = "CC_EVENT_TIMEOUT_T_CCBS2";
		break;
	case CC_EVENT_TIMEOUT_T_CCBS3:
		str = "CC_EVENT_TIMEOUT_T_CCBS3";
		break;
	}
	return str;
}

static const char pri_cc_act_header[] = "  CC-Act: %s\n";
#define PRI_CC_ACT_DEBUG_OUTPUT(ctrl)							\
	if ((ctrl)->debug & PRI_DEBUG_CC) {							\
		pri_message((ctrl), pri_cc_act_header, __FUNCTION__);	\
	}

/*!
 * \internal
 * \brief FSM action to mark FSM for destruction.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_set_self_destruct(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	cc_record->fsm_complete = 1;
}

/*!
 * \internal
 * \brief FSM action to send CC available message.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param msgtype Q.931 message type to put facility ie in.
 *
 * \return Nothing
 */
static void pri_cc_act_send_cc_available(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, int msgtype)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	rose_cc_available_encode(ctrl, call, cc_record, msgtype);
}

/*!
 * \internal
 * \brief FSM action to stop the PTMP T_RETENTION timer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_stop_t_retention(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	pri_schedule_del(ctrl, cc_record->fsm.ptmp.t_retention);
	cc_record->fsm.ptmp.t_retention = 0;
}

/*!
 * \internal
 * \brief T_RETENTION timeout callback.
 *
 * \param data CC record pointer.
 *
 * \return Nothing
 */
static void pri_cc_timeout_t_retention(void *data)
{
	struct pri_cc_record *cc_record = data;

	cc_record->fsm.ptmp.t_retention = 0;
	q931_cc_timeout(cc_record->signaling->pri, cc_record->signaling, cc_record,
		CC_EVENT_TIMEOUT_T_RETENTION);
}

/*!
 * \internal
 * \brief FSM action to start the PTMP T_RETENTION timer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_start_t_retention(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	if (cc_record->fsm.ptmp.t_retention) {
		pri_error(ctrl, "!! T_RETENTION is already running!");
		pri_schedule_del(ctrl, cc_record->fsm.ptmp.t_retention);
	}
	cc_record->fsm.ptmp.t_retention = pri_schedule_event(ctrl,
		ctrl->timers[PRI_TIMER_T_RETENTION], pri_cc_timeout_t_retention, cc_record);
}

/*!
 * \internal
 * \brief FSM action to stop the PTMP EXTENDED_T_CCBS1 timer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_stop_extended_t_ccbs1(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	pri_schedule_del(ctrl, cc_record->fsm.ptmp.extended_t_ccbs1);
	cc_record->fsm.ptmp.extended_t_ccbs1 = 0;
}

/*!
 * \internal
 * \brief EXTENDED_T_CCBS1 timeout callback.
 *
 * \param data CC record pointer.
 *
 * \return Nothing
 */
static void pri_cc_timeout_extended_t_ccbs1(void *data)
{
	struct pri_cc_record *cc_record = data;

	cc_record->fsm.ptmp.extended_t_ccbs1 = 0;
	q931_cc_timeout(cc_record->signaling->pri, cc_record->signaling, cc_record,
		CC_EVENT_TIMEOUT_EXTENDED_T_CCBS1);
}

/*!
 * \internal
 * \brief FSM action to start the PTMP extended T_CCBS1 timer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_start_extended_t_ccbs1(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	if (cc_record->fsm.ptmp.extended_t_ccbs1) {
		pri_error(ctrl, "!! Extended T_CCBS1 is already running!");
		pri_schedule_del(ctrl, cc_record->fsm.ptmp.extended_t_ccbs1);
	}
	/* Timeout is T_CCBS1 + 2 seconds. */
	cc_record->fsm.ptmp.extended_t_ccbs1 = pri_schedule_event(ctrl,
		ctrl->timers[PRI_TIMER_T_CCBS1] + 2000, pri_cc_timeout_extended_t_ccbs1,
		cc_record);
}

/*!
 * \internal
 * \brief FSM action to stop the PTMP T_CCBS2 timer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_stop_t_ccbs2(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	pri_schedule_del(ctrl, cc_record->fsm.ptmp.t_ccbs2);
	cc_record->fsm.ptmp.t_ccbs2 = 0;
}

/*!
 * \internal
 * \brief T_CCBS2 timeout callback.
 *
 * \param data CC record pointer.
 *
 * \return Nothing
 */
static void pri_cc_timeout_t_ccbs2(void *data)
{
	struct pri_cc_record *cc_record = data;

	cc_record->fsm.ptmp.t_ccbs2 = 0;
	q931_cc_timeout(cc_record->signaling->pri, cc_record->signaling, cc_record,
		CC_EVENT_TIMEOUT_T_CCBS2);
}

/*!
 * \internal
 * \brief FSM action to start the PTMP T_CCBS2 timer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_start_t_ccbs2(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	if (cc_record->fsm.ptmp.t_ccbs2) {
		pri_error(ctrl, "!! T_CCBS2/T_CCNR2 is already running!");
		pri_schedule_del(ctrl, cc_record->fsm.ptmp.t_ccbs2);
	}
	cc_record->fsm.ptmp.t_ccbs2 = pri_schedule_event(ctrl,
		ctrl->timers[cc_record->is_ccnr ? PRI_TIMER_T_CCNR2 : PRI_TIMER_T_CCBS2],
		pri_cc_timeout_t_ccbs2, cc_record);
}

/*!
 * \internal
 * \brief FSM action to stop the PTMP T_CCBS3 timer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_stop_t_ccbs3(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	pri_schedule_del(ctrl, cc_record->fsm.ptmp.t_ccbs3);
	cc_record->fsm.ptmp.t_ccbs3 = 0;
}

/*!
 * \internal
 * \brief T_CCBS3 timeout callback.
 *
 * \param data CC record pointer.
 *
 * \return Nothing
 */
static void pri_cc_timeout_t_ccbs3(void *data)
{
	struct pri_cc_record *cc_record = data;

	cc_record->fsm.ptmp.t_ccbs3 = 0;
	q931_cc_timeout(cc_record->signaling->pri, cc_record->signaling, cc_record,
		CC_EVENT_TIMEOUT_T_CCBS3);
}

/*!
 * \internal
 * \brief FSM action to start the PTMP T_CCBS3 timer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_start_t_ccbs3(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	if (cc_record->fsm.ptmp.t_ccbs3) {
		pri_error(ctrl, "!! T_CCBS3 is already running!");
		pri_schedule_del(ctrl, cc_record->fsm.ptmp.t_ccbs3);
	}
	cc_record->fsm.ptmp.t_ccbs3 = pri_schedule_event(ctrl,
		ctrl->timers[PRI_TIMER_T_CCBS3], pri_cc_timeout_t_ccbs3, cc_record);
}

/*!
 * \internal
 * \brief FSM action to send the EraseCallLinkageID message.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_send_erase_call_linkage_id(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	send_erase_call_linkage_id(ctrl, cc_record->signaling, cc_record);
}

/*!
 * \internal
 * \brief FSM action to send the CCBSErase message.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 * \param reason CCBS Erase reason
 *  normal-unspecified(0), t-CCBS2-timeout(1), t-CCBS3-timeout(2), basic-call-failed(3)
 *
 * \return Nothing
 */
static void pri_cc_act_send_ccbs_erase(struct pri *ctrl, struct pri_cc_record *cc_record, int reason)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	send_ccbs_erase(ctrl, cc_record->signaling, cc_record, reason);
}

/*!
 * \internal
 * \brief Find the T_CCBS1 timer/CCBSStatusRequest message.
 *
 * \param cc_record Call completion record to process event.
 *
 * \return Facility message pointer or NULL if not active.
 */
static struct apdu_event *pri_cc_get_t_ccbs1_status(struct pri_cc_record *cc_record)
{
	return pri_call_apdu_find(cc_record->signaling,
		cc_record->fsm.ptmp.t_ccbs1_invoke_id);
}

/*!
 * \internal
 * \brief FSM action to stop the PTMP T_CCBS1 timer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_stop_t_ccbs1(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	struct apdu_event *msg;

	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);

	msg = pri_cc_get_t_ccbs1_status(cc_record);
	if (msg) {
		pri_call_apdu_delete(cc_record->signaling, msg);
	}
}

/*!
 * \internal
 * \brief CCBSStatusRequest response callback function.
 *
 * \param reason Reason callback is called.
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param apdu APDU queued entry.  Do not change!
 * \param msg APDU response message data.  (NULL if was not the reason called.)
 *
 * \return TRUE if no more responses are expected.
 */
static int pri_cc_ccbs_status_response(enum APDU_CALLBACK_REASON reason, struct pri *ctrl, struct q931_call *call, struct apdu_event *apdu, const union apdu_msg_data *msg)
{
	struct pri_cc_record *cc_record;

	cc_record = apdu->response.user.ptr;
	switch (reason) {
	case APDU_CALLBACK_REASON_TIMEOUT:
		pri_cc_event(ctrl, call, cc_record, CC_EVENT_TIMEOUT_T_CCBS1);
		break;
	case APDU_CALLBACK_REASON_MSG_RESULT:
		pri_cc_event(ctrl, call, cc_record, msg->result->args.etsi.CCBSStatusRequest.free
			? CC_EVENT_A_FREE : CC_EVENT_A_BUSY);
		break;
	default:
		break;
	}
	return 0;
}

/*!
 * \internal
 * \brief Encode and queue an CCBSStatusRequest message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode CCBSStatusRequest.
 * \param cc_record Call completion record to process event.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_ccbs_status_request(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record)
{
	unsigned char buffer[256];
	unsigned char *end;
	struct apdu_callback_data response;

	end =
		enc_etsi_ptmp_ccbs_status_request(ctrl, buffer, buffer + sizeof(buffer),
			cc_record);
	if (!end) {
		return -1;
	}

	memset(&response, 0, sizeof(response));
	response.invoke_id = ctrl->last_invoke;
	response.timeout_time = ctrl->timers[PRI_TIMER_T_CCBS1];
	response.callback = pri_cc_ccbs_status_response;
	response.user.ptr = cc_record;
	return pri_call_apdu_queue(call, Q931_FACILITY, buffer, end - buffer, &response);
}

/*!
 * \internal
 * \brief Encode and send an CCBSStatusRequest message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode CCBSStatusRequest.
 * \param cc_record Call completion record to process event.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int send_ccbs_status_request(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record)
{
/*
 * XXX May need to add called-party-ie with Party A number in FACILITY message. (CCBSStatusRequest)
 * ETSI EN 300-195-1 Section 5.41 MSN interaction.
 */
	if (rose_ccbs_status_request(ctrl, call, cc_record)
		|| q931_facility(ctrl, call)) {
		pri_message(ctrl,
			"Could not schedule facility message for CCBSStatusRequest.\n");
		return -1;
	}

	return 0;
}

/*!
 * \internal
 * \brief FSM action to send the CCBSStatusRequest message.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_send_ccbs_status_request(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	send_ccbs_status_request(ctrl, cc_record->signaling, cc_record);
}

/*!
 * \internal
 * \brief FSM action to send the CCBSBFree message.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_send_ccbs_b_free(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	send_ccbs_b_free(ctrl, cc_record->signaling, cc_record);
}

/*!
 * \internal
 * \brief FSM action to send the remote user free message.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_send_remote_user_free(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	send_remote_user_free(ctrl, cc_record->signaling, cc_record);
}

/*!
 * \internal
 * \brief FSM action to send the CCBSStopAlerting message.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_send_ccbs_stop_alerting(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	send_ccbs_stop_alerting(ctrl, cc_record->signaling, cc_record);
}

/*!
 * \internal
 * \brief FSM action to release the call linkage id.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_release_link_id(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	cc_record->call_linkage_id = CC_PTMP_INVALID_ID;
}

/*!
 * \internal
 * \brief FSM action to reset raw A status.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_reset_raw_a_status(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	cc_record->fsm.ptmp.party_a_status_acc = CC_PARTY_A_AVAILABILITY_INVALID;
}

/*!
 * \internal
 * \brief FSM action to add raw A status with busy.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_add_raw_a_status_busy(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	if (cc_record->fsm.ptmp.party_a_status_acc != CC_PARTY_A_AVAILABILITY_FREE) {
		cc_record->fsm.ptmp.party_a_status_acc = CC_PARTY_A_AVAILABILITY_BUSY;
	}
}

/*!
 * \internal
 * \brief FSM action to set raw A status to free.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_set_raw_a_status_free(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	cc_record->fsm.ptmp.party_a_status_acc = CC_PARTY_A_AVAILABILITY_FREE;
}

/*!
 * \internal
 * \brief Fill in the status response party A status update event.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_fill_status_rsp_a(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record)
{
	struct pri_subcommand *subcmd;

	if (cc_record->fsm.ptmp.party_a_status_acc == CC_PARTY_A_AVAILABILITY_INVALID) {
		/* Accumulated party A status is invalid so don't pass it up. */
		return;
	}

	subcmd = q931_alloc_subcommand(ctrl);
	if (!subcmd) {
		return;
	}

	subcmd->cmd = PRI_SUBCMD_CC_STATUS_REQ_RSP;
	subcmd->u.cc_status_req_rsp.cc_id =  cc_record->record_id;
	subcmd->u.cc_status_req_rsp.status =
		(cc_record->fsm.ptmp.party_a_status_acc == CC_PARTY_A_AVAILABILITY_FREE)
		? 0 /* free */ : 1 /* busy */;
}

/*!
 * \internal
 * \brief Pass up party A status to upper layer (indirectly).
 *
 * \param data CC record pointer.
 *
 * \return Nothing
 */
static void pri_cc_indirect_status_rsp_a(void *data)
{
	struct pri_cc_record *cc_record = data;

	q931_cc_indirect(cc_record->signaling->pri, cc_record->signaling, cc_record,
		pri_cc_fill_status_rsp_a);
}

/*!
 * \internal
 * \brief FSM action to pass up party A status to upper layer (indirectly).
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 *
 * \note
 * Warning:  Must not use this action with pri_cc_act_set_self_destruct() in the
 * same event.
 */
static void pri_cc_act_pass_up_status_rsp_a_indirect(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	if (cc_record->fsm.ptmp.party_a_status_acc != CC_PARTY_A_AVAILABILITY_INVALID) {
		/* Accumulated party A status is not invalid so pass it up. */
		pri_schedule_event(ctrl, 0, pri_cc_indirect_status_rsp_a, cc_record);
	}
}

/*!
 * \internal
 * \brief FSM action to pass up party A status to upper layer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_pass_up_status_rsp_a(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	pri_cc_fill_status_rsp_a(ctrl, cc_record->signaling, cc_record);
}

/*!
 * \internal
 * \brief FSM action to reset A status.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_reset_a_status(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	cc_record->fsm.ptmp.party_a_status = CC_PARTY_A_AVAILABILITY_INVALID;
}

/*!
 * \internal
 * \brief FSM action to promote raw A status.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_promote_a_status(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	cc_record->fsm.ptmp.party_a_status = cc_record->fsm.ptmp.party_a_status_acc;
}

/*!
 * \internal
 * \brief FSM action to pass up party A status to upper layer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_pass_up_a_status(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	struct pri_subcommand *subcmd;

	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);

	if (cc_record->fsm.ptmp.party_a_status == CC_PARTY_A_AVAILABILITY_INVALID) {
		/* Party A status is invalid so don't pass it up. */
		return;
	}

	subcmd = q931_alloc_subcommand(ctrl);
	if (!subcmd) {
		return;
	}

	subcmd->cmd = PRI_SUBCMD_CC_STATUS;
	subcmd->u.cc_status.cc_id =  cc_record->record_id;
	subcmd->u.cc_status.status =
		(cc_record->fsm.ptmp.party_a_status == CC_PARTY_A_AVAILABILITY_FREE)
		? 0 /* free */ : 1 /* busy */;
}

/*!
 * \internal
 * \brief FSM action to pass up CC request (CCBS/CCNR) to upper layer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_pass_up_cc_request(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	struct pri_subcommand *subcmd;

	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);

	subcmd = q931_alloc_subcommand(ctrl);
	if (!subcmd) {
		return;
	}

	subcmd->cmd = PRI_SUBCMD_CC_REQ;
	subcmd->u.cc_request.cc_id =  cc_record->record_id;
	subcmd->u.cc_request.mode = cc_record->is_ccnr ? 1 /* ccnr */ : 0 /* ccbs */;
}

/*!
 * \internal
 * \brief FSM action to pass up CC cancel to upper layer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_pass_up_cc_cancel(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	struct pri_subcommand *subcmd;

	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);

	subcmd = q931_alloc_subcommand(ctrl);
	if (!subcmd) {
		return;
	}

	subcmd->cmd = PRI_SUBCMD_CC_CANCEL;
	subcmd->u.cc_cancel.cc_id =  cc_record->record_id;
}

/*!
 * \internal
 * \brief FSM action to pass up CC call to upper layer.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_pass_up_cc_call(struct pri *ctrl, struct pri_cc_record *cc_record)
{
	struct pri_subcommand *subcmd;

	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);

	subcmd = q931_alloc_subcommand(ctrl);
	if (!subcmd) {
		return;
	}

	subcmd->cmd = PRI_SUBCMD_CC_CALL;
	subcmd->u.cc_call.cc_id =  cc_record->record_id;
}

/*!
 * \internal
 * \brief FSM action to send error response to recall attempt.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 * \param code Error code to put in error message response.
 *
 * \return Nothing
 */
static void pri_cc_act_send_error_recall(struct pri *ctrl, struct pri_cc_record *cc_record, enum rose_error_code code)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	rose_error_msg_encode(ctrl, cc_record->response.signaling, Q931_ANY_MESSAGE,
		cc_record->response.invoke_id, code);
}

/*!
 * \internal
 * \brief FSM action to request the call be hung up.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 *
 * \return Nothing
 */
static void pri_cc_act_set_call_to_hangup(struct pri *ctrl, q931_call *call)
{
	PRI_CC_ACT_DEBUG_OUTPUT(ctrl);
	call->cc.hangup_call = 1;
}

/*!
 * \internal
 * \brief FSM action to set original call data into recall call.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 *
 * \return Nothing
 */
static void pri_cc_act_set_original_call_parameters(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record)
{
	call->called = cc_record->party_b;
	call->remote_id = cc_record->party_a;
	call->cc.saved_ie_contents = cc_record->saved_ie_contents;
	call->bc = cc_record->bc;
}

/*!
 * \internal
 * \brief CC FSM PTMP agent CC_STATE_IDLE.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \return Nothing
 */
static void pri_cc_fsm_ptmp_agent_idle(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	switch (event) {
	case CC_EVENT_AVAILABLE:
		cc_record->state = CC_STATE_PENDING_AVAILABLE;
		break;
	case CC_EVENT_CANCEL:
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		break;
	default:
		break;
	}
}

/*!
 * \internal
 * \brief CC FSM PTMP agent CC_STATE_PENDING_AVAILABLE.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \return Nothing
 */
static void pri_cc_fsm_ptmp_agent_pend_avail(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	switch (event) {
	case CC_EVENT_MSG_ALERTING:
		pri_cc_act_send_cc_available(ctrl, call, cc_record, Q931_ALERTING);
		cc_record->state = CC_STATE_AVAILABLE;
		break;
	case CC_EVENT_MSG_DISCONNECT:
		pri_cc_act_send_cc_available(ctrl, call, cc_record, Q931_DISCONNECT);
		cc_record->state = CC_STATE_AVAILABLE;
		break;
	case CC_EVENT_CANCEL:
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	default:
		break;
	}
}

/*!
 * \internal
 * \brief CC FSM PTMP agent CC_STATE_AVAILABLE.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \return Nothing
 */
static void pri_cc_fsm_ptmp_agent_avail(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	switch (event) {
	case CC_EVENT_MSG_RELEASE:
	case CC_EVENT_MSG_RELEASE_COMPLETE:
		pri_cc_act_stop_t_retention(ctrl, cc_record);
		pri_cc_act_start_t_retention(ctrl, cc_record);
		break;
	case CC_EVENT_CC_REQUEST:
		pri_cc_act_pass_up_cc_request(ctrl, cc_record);
		pri_cc_act_stop_t_retention(ctrl, cc_record);
		cc_record->state = CC_STATE_REQUESTED;
		break;
	case CC_EVENT_TIMEOUT_T_RETENTION:
		pri_cc_act_send_erase_call_linkage_id(ctrl, cc_record);
		pri_cc_act_release_link_id(ctrl, cc_record);
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_stop_t_retention(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_CANCEL:
		pri_cc_act_send_erase_call_linkage_id(ctrl, cc_record);
		pri_cc_act_release_link_id(ctrl, cc_record);
		pri_cc_act_stop_t_retention(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	default:
		break;
	}
}

/*!
 * \internal
 * \brief CC FSM PTMP agent CC_STATE_REQUESTED.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \return Nothing
 */
static void pri_cc_fsm_ptmp_agent_req(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	switch (event) {
	case CC_EVENT_CC_REQUEST_ACCEPT:
		pri_cc_act_send_erase_call_linkage_id(ctrl, cc_record);
		pri_cc_act_release_link_id(ctrl, cc_record);
		pri_cc_act_start_t_ccbs2(ctrl, cc_record);
		pri_cc_act_reset_a_status(ctrl, cc_record);
		cc_record->state = CC_STATE_ACTIVATED;
		break;
	case CC_EVENT_CANCEL:
		pri_cc_act_send_erase_call_linkage_id(ctrl, cc_record);
		pri_cc_act_release_link_id(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	default:
		break;
	}
}

/*!
 * \internal
 * \brief CC FSM PTMP agent CC_STATE_ACTIVATED.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \return Nothing
 */
static void pri_cc_fsm_ptmp_agent_activated(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	switch (event) {
	case CC_EVENT_RECALL:
		pri_cc_act_send_error_recall(ctrl, cc_record, ROSE_ERROR_CCBS_NotReadyForCall);
		pri_cc_act_set_call_to_hangup(ctrl, call);
		break;
	case CC_EVENT_REMOTE_USER_FREE:
		if (cc_record->fsm.ptmp.is_ccbs_busy) {
			pri_cc_act_send_ccbs_b_free(ctrl, cc_record);
			pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
			pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
			cc_record->state = CC_STATE_SUSPENDED;
			break;
		}
		switch (cc_record->fsm.ptmp.party_a_status) {
		case CC_PARTY_A_AVAILABILITY_INVALID:
			if (!pri_cc_get_t_ccbs1_status(cc_record)) {
				pri_cc_act_reset_raw_a_status(ctrl, cc_record);
				pri_cc_act_send_ccbs_status_request(ctrl, cc_record);
				//pri_cc_act_start_t_ccbs1(ctrl, cc_record);
			}
			cc_record->state = CC_STATE_B_AVAILABLE;
			break;
		case CC_PARTY_A_AVAILABILITY_BUSY:
			pri_cc_act_pass_up_a_status(ctrl, cc_record);
			pri_cc_act_send_ccbs_b_free(ctrl, cc_record);
			if (!pri_cc_get_t_ccbs1_status(cc_record)) {
				pri_cc_act_reset_raw_a_status(ctrl, cc_record);
				pri_cc_act_send_ccbs_status_request(ctrl, cc_record);
				//pri_cc_act_start_t_ccbs1(ctrl, cc_record);
			}
			cc_record->state = CC_STATE_SUSPENDED;
			break;
		case CC_PARTY_A_AVAILABILITY_FREE:
			//pri_cc_act_pass_up_a_status(ctrl, cc_record);
			pri_cc_act_send_remote_user_free(ctrl, cc_record);
			pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
			pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
			pri_cc_act_start_t_ccbs3(ctrl, cc_record);
			cc_record->state = CC_STATE_WAIT_CALLBACK;
			break;
		default:
			break;
		}
		break;
	case CC_EVENT_A_STATUS:
		if (pri_cc_get_t_ccbs1_status(cc_record)) {
			pri_cc_act_pass_up_status_rsp_a_indirect(ctrl, cc_record);
		} else {
			pri_cc_act_reset_a_status(ctrl, cc_record);
			pri_cc_act_reset_raw_a_status(ctrl, cc_record);
			pri_cc_act_send_ccbs_status_request(ctrl, cc_record);
			//pri_cc_act_start_t_ccbs1(ctrl, cc_record);
			pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
			pri_cc_act_start_extended_t_ccbs1(ctrl, cc_record);
		}
		break;
	case CC_EVENT_A_FREE:
		pri_cc_act_set_raw_a_status_free(ctrl, cc_record);
		pri_cc_act_promote_a_status(ctrl, cc_record);
		pri_cc_act_pass_up_a_status(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		break;
	case CC_EVENT_A_BUSY:
		pri_cc_act_add_raw_a_status_busy(ctrl, cc_record);
		pri_cc_act_pass_up_status_rsp_a(ctrl, cc_record);
		break;
	case CC_EVENT_TIMEOUT_T_CCBS1:
		pri_cc_act_promote_a_status(ctrl, cc_record);
		if (cc_record->fsm.ptmp.party_a_status == CC_PARTY_A_AVAILABILITY_INVALID) {
			/*
			 * Did not get any responses.
			 * User A no longer present.
			 */
			pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
			pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
			pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
			pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
			pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
			pri_cc_act_set_self_destruct(ctrl, cc_record);
			cc_record->state = CC_STATE_IDLE;
		}
		break;
	case CC_EVENT_TIMEOUT_EXTENDED_T_CCBS1:
		pri_cc_act_reset_a_status(ctrl, cc_record);
		break;
	case CC_EVENT_TIMEOUT_T_CCBS2:
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 1 /* t-CCBS2-timeout */);
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_LINK_CANCEL:
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_CANCEL:
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	default:
		break;
	}
}

/*!
 * \internal
 * \brief CC FSM PTMP agent CC_STATE_B_AVAILABLE.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \return Nothing
 */
static void pri_cc_fsm_ptmp_agent_b_avail(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	switch (event) {
	case CC_EVENT_RECALL:
		pri_cc_act_send_error_recall(ctrl, cc_record, ROSE_ERROR_CCBS_NotReadyForCall);
		pri_cc_act_set_call_to_hangup(ctrl, call);
		break;
	case CC_EVENT_REMOTE_USER_FREE:
		if (cc_record->fsm.ptmp.is_ccbs_busy) {
			pri_cc_act_send_ccbs_b_free(ctrl, cc_record);
			pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
			if (cc_record->fsm.ptmp.extended_t_ccbs1) {
				pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
				pri_cc_act_add_raw_a_status_busy(ctrl, cc_record);
				pri_cc_act_pass_up_status_rsp_a_indirect(ctrl, cc_record);
			}
			cc_record->state = CC_STATE_SUSPENDED;
		}
		break;
	case CC_EVENT_A_STATUS:
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_start_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_pass_up_status_rsp_a_indirect(ctrl, cc_record);
		break;
	case CC_EVENT_A_FREE:
		pri_cc_act_send_remote_user_free(ctrl, cc_record);
		pri_cc_act_set_raw_a_status_free(ctrl, cc_record);
		//pri_cc_act_promote_a_status(ctrl, cc_record);
		//pri_cc_act_pass_up_a_status(ctrl, cc_record);
		if (cc_record->fsm.ptmp.extended_t_ccbs1) {
			pri_cc_act_pass_up_status_rsp_a(ctrl, cc_record);
		}
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_start_t_ccbs3(ctrl, cc_record);
		cc_record->state = CC_STATE_WAIT_CALLBACK;
		break;
	case CC_EVENT_A_BUSY:
		pri_cc_act_add_raw_a_status_busy(ctrl, cc_record);
		if (cc_record->fsm.ptmp.extended_t_ccbs1) {
			pri_cc_act_pass_up_status_rsp_a(ctrl, cc_record);
		}
		break;
	case CC_EVENT_TIMEOUT_T_CCBS1:
		if (cc_record->fsm.ptmp.party_a_status_acc == CC_PARTY_A_AVAILABILITY_INVALID) {
			/*
			 * Did not get any responses.
			 * User A no longer present.
			 */
			pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
			pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
			pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
			pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
			pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
			pri_cc_act_set_self_destruct(ctrl, cc_record);
			cc_record->state = CC_STATE_IDLE;
			break;
		}
		/* Only received User A busy. */
		pri_cc_act_send_ccbs_b_free(ctrl, cc_record);
		pri_cc_act_promote_a_status(ctrl, cc_record);
		pri_cc_act_pass_up_a_status(ctrl, cc_record);
		pri_cc_act_reset_raw_a_status(ctrl, cc_record);
		pri_cc_act_send_ccbs_status_request(ctrl, cc_record);
		//pri_cc_act_start_t_ccbs1(ctrl, cc_record);
		cc_record->state = CC_STATE_SUSPENDED;
		break;
	case CC_EVENT_TIMEOUT_T_CCBS2:
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 1 /* t-CCBS2-timeout */);
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_LINK_CANCEL:
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_CANCEL:
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	default:
		break;
	}
}

/*!
 * \internal
 * \brief CC FSM PTMP agent CC_STATE_SUSPENDED.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \return Nothing
 */
static void pri_cc_fsm_ptmp_agent_suspended(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	switch (event) {
	case CC_EVENT_RECALL:
		pri_cc_act_send_error_recall(ctrl, cc_record, ROSE_ERROR_CCBS_NotReadyForCall);
		pri_cc_act_set_call_to_hangup(ctrl, call);
		break;
	case CC_EVENT_REMOTE_USER_FREE:
		if (cc_record->fsm.ptmp.is_ccbs_busy) {
			pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
			if (cc_record->fsm.ptmp.extended_t_ccbs1) {
				pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
				pri_cc_act_add_raw_a_status_busy(ctrl, cc_record);
				pri_cc_act_pass_up_status_rsp_a_indirect(ctrl, cc_record);
			}
		} else {
			if (!pri_cc_get_t_ccbs1_status(cc_record)) {
				pri_cc_act_reset_raw_a_status(ctrl, cc_record);
				pri_cc_act_send_ccbs_status_request(ctrl, cc_record);
				//pri_cc_act_start_t_ccbs1(ctrl, cc_record);
			}
		}
		break;
	case CC_EVENT_A_STATUS:
		/* Upper layer should not send when CCBS busy. */
		if (cc_record->fsm.ptmp.is_ccbs_busy) {
			pri_cc_act_add_raw_a_status_busy(ctrl, cc_record);
		} else {
			pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
			pri_cc_act_start_extended_t_ccbs1(ctrl, cc_record);
		}
		pri_cc_act_pass_up_status_rsp_a_indirect(ctrl, cc_record);
		break;
	case CC_EVENT_A_FREE:
		pri_cc_act_set_raw_a_status_free(ctrl, cc_record);
		pri_cc_act_promote_a_status(ctrl, cc_record);
		pri_cc_act_pass_up_a_status(ctrl, cc_record);
		if (cc_record->fsm.ptmp.extended_t_ccbs1) {
			pri_cc_act_pass_up_status_rsp_a(ctrl, cc_record);
		}
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_reset_a_status(ctrl, cc_record);
		cc_record->state = CC_STATE_ACTIVATED;
		break;
	case CC_EVENT_A_BUSY:
		pri_cc_act_add_raw_a_status_busy(ctrl, cc_record);
		if (cc_record->fsm.ptmp.extended_t_ccbs1) {
			pri_cc_act_pass_up_status_rsp_a(ctrl, cc_record);
		}
		break;
	case CC_EVENT_TIMEOUT_T_CCBS1:
		if (cc_record->fsm.ptmp.party_a_status_acc == CC_PARTY_A_AVAILABILITY_INVALID) {
			/*
			 * Did not get any responses.
			 * User A no longer present.
			 */
			pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
			pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
			pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
			pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
			pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
			pri_cc_act_set_self_destruct(ctrl, cc_record);
			cc_record->state = CC_STATE_IDLE;
			break;
		}
		/* Only received User A busy. */
		pri_cc_act_reset_raw_a_status(ctrl, cc_record);
		pri_cc_act_send_ccbs_status_request(ctrl, cc_record);
		//pri_cc_act_start_t_ccbs1(ctrl, cc_record);
		break;
	case CC_EVENT_TIMEOUT_T_CCBS2:
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 1 /* t-CCBS2-timeout */);
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_LINK_CANCEL:
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_CANCEL:
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
		pri_cc_act_stop_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_extended_t_ccbs1(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	default:
		break;
	}
}

/*!
 * \internal
 * \brief CC FSM PTMP agent CC_STATE_WAIT_CALLBACK.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \return Nothing
 */
static void pri_cc_fsm_ptmp_agent_wait_callback(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	switch (event) {
	case CC_EVENT_TIMEOUT_T_CCBS3:
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 2 /* t-CCBS3-timeout */);
		pri_cc_act_stop_t_ccbs3(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_RECALL:
		pri_cc_act_pass_up_cc_call(ctrl, cc_record);
		pri_cc_act_set_original_call_parameters(ctrl, call, cc_record);
		if (cc_record->option.recall_mode == 0 /* globalRecall */) {
			pri_cc_act_send_ccbs_stop_alerting(ctrl, cc_record);
		}
		pri_cc_act_stop_t_ccbs3(ctrl, cc_record);
		cc_record->state = CC_STATE_CALLBACK;
		break;
	case CC_EVENT_A_STATUS:
		pri_cc_act_set_raw_a_status_free(ctrl, cc_record);
		pri_cc_act_pass_up_status_rsp_a_indirect(ctrl, cc_record);
		break;
	case CC_EVENT_TIMEOUT_T_CCBS2:
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 1 /* t-CCBS2-timeout */);
		pri_cc_act_stop_t_ccbs3(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_LINK_CANCEL:
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
		pri_cc_act_stop_t_ccbs3(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_CANCEL:
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
		pri_cc_act_stop_t_ccbs3(ctrl, cc_record);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	default:
		break;
	}
}

/*!
 * \internal
 * \brief CC FSM PTMP agent CC_STATE_CALLBACK.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \return Nothing
 */
static void pri_cc_fsm_ptmp_agent_callback(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	switch (event) {
	case CC_EVENT_RECALL:
		pri_cc_act_send_error_recall(ctrl, cc_record, ROSE_ERROR_CCBS_AlreadyAccepted);
		pri_cc_act_set_call_to_hangup(ctrl, call);
		break;
	case CC_EVENT_A_STATUS:
		pri_cc_act_set_raw_a_status_free(ctrl, cc_record);
		pri_cc_act_pass_up_status_rsp_a_indirect(ctrl, cc_record);
		break;
	case CC_EVENT_TIMEOUT_T_CCBS2:
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 1 /* t-CCBS2-timeout */);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_LINK_CANCEL:
		pri_cc_act_pass_up_cc_cancel(ctrl, cc_record);
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	case CC_EVENT_CANCEL:
		pri_cc_act_send_ccbs_erase(ctrl, cc_record, 0 /* normal-unspecified */);
		pri_cc_act_stop_t_ccbs2(ctrl, cc_record);
		pri_cc_act_set_self_destruct(ctrl, cc_record);
		cc_record->state = CC_STATE_IDLE;
		break;
	default:
		break;
	}
}

/*!
 * \internal
 * \brief CC FSM state function type.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \return Nothing
 */
typedef void (*pri_cc_fsm_state)(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event);

/*! CC FSM PTMP agent state table. */
static const pri_cc_fsm_state pri_cc_fsm_ptmp_agent[CC_STATE_NUM] = {
/* *INDENT-OFF* */
	[CC_STATE_IDLE] = pri_cc_fsm_ptmp_agent_idle,
	[CC_STATE_PENDING_AVAILABLE] = pri_cc_fsm_ptmp_agent_pend_avail,
	[CC_STATE_AVAILABLE] = pri_cc_fsm_ptmp_agent_avail,
	[CC_STATE_REQUESTED] = pri_cc_fsm_ptmp_agent_req,
	[CC_STATE_ACTIVATED] = pri_cc_fsm_ptmp_agent_activated,
	[CC_STATE_B_AVAILABLE] = pri_cc_fsm_ptmp_agent_b_avail,
	[CC_STATE_SUSPENDED] = pri_cc_fsm_ptmp_agent_suspended,
	[CC_STATE_WAIT_CALLBACK] = pri_cc_fsm_ptmp_agent_wait_callback,
	[CC_STATE_CALLBACK] = pri_cc_fsm_ptmp_agent_callback,
/* *INDENT-ON* */
};

/*!
 * \brief Send an event to the cc state machine.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * (May be NULL if it is supposed to be the signaling connection
 * for Q.SIG or PTP and it is not established yet.)
 * \param cc_record Call completion record to process event.
 * \param event Event to process.
 *
 * \retval nonzero if cc record destroyed because FSM completed.
 */
int pri_cc_event(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event)
{
	const pri_cc_fsm_state *cc_fsm;
	enum CC_STATES orig_state;

	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		/*! \todo BUGBUG pri_cc_event(Q.SIG) not written */
		if (cc_record->is_agent) {
			cc_fsm = NULL;
		} else {
			cc_fsm = NULL;
		}
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
			/*! \todo BUGBUG pri_cc_event(ETSI PTMP) not written */
			if (cc_record->is_agent) {
				cc_fsm = pri_cc_fsm_ptmp_agent;
			} else {
				cc_fsm = NULL;
			}
		} else {
			/*! \todo BUGBUG pri_cc_event(ETSI PTP) not written */
			if (cc_record->is_agent) {
				cc_fsm = NULL;
			} else {
				cc_fsm = NULL;
			}
		}
		break;
	default:
		/* CC not supported on this switch type. */
		cc_fsm = NULL;
		break;
	}

	if (!cc_fsm) {
		/* No FSM available. */
		pri_cc_delete_record(ctrl, cc_record);
		return 1;
	}
	orig_state = cc_record->state;
	if (ctrl->debug & PRI_DEBUG_CC) {
		pri_message(ctrl, "CC-Event: %s in state %s\n", pri_cc_fsm_state_str(orig_state),
			pri_cc_fsm_event_str(event));
	}
	if (orig_state < CC_STATE_IDLE || CC_STATE_NUM <= orig_state || !cc_fsm[orig_state]) {
		/* Programming error: State not implemented. */
		pri_error(ctrl, "!! CC state not implemented: %s\n",
			pri_cc_fsm_state_str(orig_state));
		return 0;
	}
	/* Execute the state. */
	cc_fsm[orig_state](ctrl, call, cc_record, event);
	if (ctrl->debug & PRI_DEBUG_CC) {
		pri_message(ctrl, "  CC-Next-State: %s\n", (orig_state == cc_record->state)
			? "$" : pri_cc_fsm_state_str(cc_record->state));
	}
	if (cc_record->fsm_complete) {
		if (call && call->cc.record == cc_record) {
			call->cc.record = NULL;
		}
		pri_cc_delete_record(ctrl, cc_record);
		return 1;
	} else {
		return 0;
	}
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
		cc_record->is_agent = 1;
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
			int linkage_id;

			linkage_id = pri_cc_new_linkage_id(ctrl);
			if (linkage_id == CC_PTMP_INVALID_ID) {
				break;
			}
			cc_record = pri_cc_new_record(ctrl, call);
			if (!cc_record) {
				break;
			}
			cc_record->call_linkage_id = linkage_id;
			cc_record->signaling = PRI_MASTER(ctrl)->dummy_call;
		} else {
			cc_record = pri_cc_new_record(ctrl, call);
			if (!cc_record) {
				break;
			}
		}
		cc_record->is_agent = 1;
		break;
	default:
		break;
	}

	call->cc.record = cc_record;
	if (cc_record && !pri_cc_event(ctrl, call, cc_record, CC_EVENT_AVAILABLE)) {
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

	/* Set the requested CC mode. */
	cc_record->is_ccnr = mode ? 1 : 0;

	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		call = q931_new_call(ctrl);
		if (!call) {
			return -1;
		}

		/*! \todo BUGBUG pri_cc_req(Q.SIG) not written */
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
			pri_cc_event(ctrl, cc_record->signaling, cc_record, CC_EVENT_CC_REQUEST);
		} else {
			/*! \todo BUGBUG pri_cc_req(PTP) not written */
			return -1;
		}
		break;
	default:
		return -1;
	}

	return 0;
}

/*!
 * \internal
 * \brief Encode a PTMP cc-request reply message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param pos Starting position to encode the facility ie contents.
 * \param end End of facility ie contents encoding data buffer.
 * \param operation CCBS/CCNR operation code.
 * \param invoke_id Invoke id to put in error message response.
 * \param recall_mode Configured PTMP recall mode.
 * \param reference_id Active CC reference id.
 *
 * \retval Start of the next ASN.1 component to encode on success.
 * \retval NULL on error.
 */
static unsigned char *enc_cc_etsi_ptmp_req_rsp(struct pri *ctrl, unsigned char *pos,
	unsigned char *end, enum rose_operation operation, int invoke_id, int recall_mode,
	int reference_id)
{
	struct rose_msg_result msg;

	pos = facility_encode_header(ctrl, pos, end, NULL);
	if (!pos) {
		return NULL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.invoke_id = invoke_id;
	msg.operation = operation;

	/* CCBS/CCNR reply */
	msg.args.etsi.CCBSRequest.recall_mode = recall_mode;
	msg.args.etsi.CCBSRequest.ccbs_reference = reference_id;

	pos = rose_encode_result(ctrl, pos, end, &msg);

	return pos;
}

/*!
 * \internal
 * \brief Encode and queue PTMP a cc-request reply message.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param msgtype Q.931 message type to put facility ie in.
 * \param operation CCBS/CCNR operation code.
 * \param invoke_id Invoke id to put in error message response.
 * \param recall_mode Configured PTMP recall mode.
 * \param reference_id Active CC reference id.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_cc_etsi_ptmp_req_rsp_encode(struct pri *ctrl, q931_call *call, int msgtype, enum rose_operation operation, int invoke_id, int recall_mode, int reference_id)
{
	unsigned char buffer[256];
	unsigned char *end;

	end = enc_cc_etsi_ptmp_req_rsp(ctrl, buffer, buffer + sizeof(buffer), operation,
		invoke_id, recall_mode, reference_id);
	if (!end) {
		return -1;
	}

	return pri_call_apdu_queue(call, msgtype, buffer, end - buffer, NULL);
}

/*!
 * \internal
 * \brief Send the CC activation request result PTMP.
 *
 * \param ctrl D channel controller.
 * \param call Q.931 call leg.
 * \param operation CCBS/CCNR operation code.
 * \param invoke_id Invoke id to put in error message response.
 * \param recall_mode Configured PTMP recall mode.
 * \param reference_id Active CC reference id.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int send_cc_etsi_ptmp_req_rsp(struct pri *ctrl, q931_call *call, enum rose_operation operation, int invoke_id, int recall_mode, int reference_id)
{
	if (rose_cc_etsi_ptmp_req_rsp_encode(ctrl, call, Q931_FACILITY, operation, invoke_id,
		recall_mode, reference_id)
		|| q931_facility(ctrl, call)) {
		pri_message(ctrl,
			"Could not schedule facility message for CC request result message.\n");
		return -1;
	}

	return 0;
}

/*!
 * \internal
 * \brief Response to an incoming CC activation request PTMP.
 *
 * \param ctrl D channel controller.
 * \param cc_record Call completion record to process event.
 * \param status success(0)/timeout(1)/
 *		short_term_denial(2)/long_term_denial(3)/not_subscribed(4)/queue_full(5)
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_cc_req_rsp_ptmp(struct pri *ctrl, struct pri_cc_record *cc_record, int status)
{
	int fail;

	switch (cc_record->response.invoke_operation) {
	case ROSE_ETSI_CCBSRequest:
	case ROSE_ETSI_CCNRRequest:
		break;
	default:
		/* We no longer know how to send the response.  Should not happen. */
		return -1;
	}

	fail = 0;
	if (status) {
		enum rose_error_code code;

		switch (status) {
		default:
		case 1:/* timeout */
		case 2:/* short_term_denial */
			code = ROSE_ERROR_CCBS_ShortTermDenial;
			break;
		case 3:/* long_term_denial */
			code = ROSE_ERROR_CCBS_LongTermDenial;
			break;
		case 4:/* not_subscribed */
			code = ROSE_ERROR_Gen_NotSubscribed;
			break;
		case 5:/* queue_full */
			code = ROSE_ERROR_CCBS_OutgoingCCBSQueueFull;
			break;
		}
		send_facility_error(ctrl, cc_record->response.signaling,
			cc_record->response.invoke_id, code);
		pri_cc_event(ctrl, cc_record->response.signaling, cc_record,
			CC_EVENT_CANCEL);
	} else {
		/* Successful CC activation. */
		if (send_cc_etsi_ptmp_req_rsp(ctrl, cc_record->response.signaling,
			cc_record->response.invoke_operation, cc_record->response.invoke_id,
			cc_record->option.recall_mode, cc_record->ccbs_reference_id)) {
			fail = -1;
		}
		pri_cc_event(ctrl, cc_record->response.signaling, cc_record,
			CC_EVENT_CC_REQUEST_ACCEPT);
	}
	return fail;
}

/*!
 * \brief Response to an incoming CC activation request.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to activate.
 * \param status success(0)/timeout(1)/
 *      short_term_denial(2)/long_term_denial(3)/not_subscribed(4)/queue_full(5)
 *
 * \note
 * If the given status was failure, then the cc_id is no longer valid.
 * \note
 * The caller should cancel CC if error is returned.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
int pri_cc_req_rsp(struct pri *ctrl, long cc_id, int status)
{
	struct pri_cc_record *cc_record;
	int fail;

	cc_record = pri_cc_find_by_id(ctrl, cc_id);
	if (!cc_record) {
		return -1;
	}

	fail = -1;
	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		/*! \todo BUGBUG pri_cc_req_rsp(Q.SIG) not written */
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
			if (!rose_cc_req_rsp_ptmp(ctrl, cc_record, status)) {
				fail = 0;
			}
		} else {
			/*! \todo BUGBUG pri_cc_req_rsp(PTP) not written */
		}
		break;
	default:
		break;
	}
	return fail;
}

/*!
 * \brief Indicate that the remote user (Party B) is free to call.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to activate.
 * \param is_ccbs_busy TRUE if the span does not have any B channels
 * available for a recall call.
 *
 * \retval TRUE if is_ccbs_busy is used in the current CC mode.
 *
 * \note
 * If returned TRUE then this call must be called again when a B channel
 * becomes available.
 */
int pri_cc_remote_user_free(struct pri *ctrl, long cc_id, int is_ccbs_busy)
{
	int is_ccbs_busy_used;
	struct pri_cc_record *cc_record;

	cc_record = pri_cc_find_by_id(ctrl, cc_id);
	if (!cc_record) {
		return 0;
	}

	is_ccbs_busy_used = 0;
	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		/*! \todo BUGBUG pri_cc_remote_user_free(Q.SIG) not written */
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
			cc_record->fsm.ptmp.is_ccbs_busy = is_ccbs_busy ? 1 : 0;
			is_ccbs_busy_used = 1;
		}
		pri_cc_event(ctrl, cc_record->signaling, cc_record, CC_EVENT_REMOTE_USER_FREE);
		break;
	default:
		break;
	}

	return is_ccbs_busy_used;
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
	int fail;
	struct pri_cc_record *cc_record;

	cc_record = pri_cc_find_by_id(ctrl, cc_id);
	if (!cc_record) {
		return -1;
	}

	fail = -1;
	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		/* Does not apply. */
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
			pri_cc_event(ctrl, cc_record->signaling, cc_record, CC_EVENT_A_STATUS);
			fail = 0;
		}
		break;
	default:
		break;
	}

	return fail;
}

/*!
 * \internal
 * \brief Encode and queue an CCBSStatusRequest result message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode CCBSStatusRequest.
 * \param cc_record Call completion record to process event.
 * \param is_free TRUE if the Party A status is available.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int rose_ccbs_status_request_rsp(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, int is_free)
{
	unsigned char buffer[256];
	unsigned char *end;

	end =
		enc_etsi_ptmp_ccbs_status_request_rsp(ctrl, buffer, buffer + sizeof(buffer),
			cc_record, is_free);
	if (!end) {
		return -1;
	}

	return pri_call_apdu_queue(call, Q931_FACILITY, buffer, end - buffer, NULL);
}

/*!
 * \internal
 * \brief Encode and send an CCBSStatusRequest result message.
 *
 * \param ctrl D channel controller for diagnostic messages or global options.
 * \param call Call leg from which to encode CCBSStatusRequest.
 * \param cc_record Call completion record to process event.
 * \param is_free TRUE if the Party A status is available.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int send_ccbs_status_request_rsp(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, int is_free)
{
	if (rose_ccbs_status_request_rsp(ctrl, call, cc_record, is_free)
		|| q931_facility(ctrl, call)) {
		pri_message(ctrl,
			"Could not schedule facility message for CCBSStatusRequest result.\n");
		return -1;
	}

	return 0;
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
	struct pri_cc_record *cc_record;

	cc_record = pri_cc_find_by_id(ctrl, cc_id);
	if (!cc_record) {
		return;
	}

	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		/* Does not apply. */
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (q931_is_ptmp(ctrl)) {
			if (cc_record->response.invoke_operation != ROSE_ETSI_CCBSStatusRequest) {
				/* We no longer know how to send the response. */
				break;
			}
			send_ccbs_status_request_rsp(ctrl, cc_record->signaling, cc_record,
				status ? 0 /* busy */ : 1 /* free */);
		}
		break;
	default:
		break;
	}
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
	struct pri_cc_record *cc_record;

	cc_record = pri_cc_find_by_id(ctrl, cc_id);
	if (!cc_record) {
		return;
	}

	switch (ctrl->switchtype) {
	case PRI_SWITCH_QSIG:
		/*! \todo BUGBUG pri_cc_status(Q.SIG) not written */
		break;
	case PRI_SWITCH_EUROISDN_E1:
	case PRI_SWITCH_EUROISDN_T1:
		if (!q931_is_ptmp(ctrl)) {
			/*! \todo BUGBUG pri_cc_status(PTP) not written */
		}
		break;
	default:
		break;
	}
}

/*!
 * \brief Initiate the CC callback call.
 *
 * \param ctrl D channel controller.
 * \param cc_id CC record ID to activate.
 * \param call Q.931 call leg.
 * \param req SETUP request parameters.  Parameters saved by CC will override.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
int pri_cc_call(struct pri *ctrl, long cc_id, q931_call *call, struct pri_sr *req)
{
	struct pri_cc_record *cc_record;

	cc_record = pri_cc_find_by_id(ctrl, cc_id);
	if (!cc_record) {
		return -1;
	}

	/* Override parameters for sending recall. */
	req->caller = cc_record->party_a;
	req->called = cc_record->party_b;
	req->transmode = cc_record->bc.transcapability;
	req->userl1 = cc_record->bc.userl1;

	/*
	 * The caller is allowed to send different user-user information.
	 *
	 * It makes no sense for the caller to supply redirecting information
	 * but we'll allow it to pass anyway.
	 */
	//q931_party_redirecting_init(&req->redirecting);

	/* Add switch specific recall APDU to call. */
	pri_cc_event(ctrl, call, cc_record, CC_EVENT_RECALL);

	if (q931_setup(ctrl, call, req)) {
		return -1;
	}
	return 0;
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
	struct pri_cc_record *cc_record;

	cc_record = pri_cc_find_by_id(ctrl, cc_id);
	if (!cc_record) {
		return;
	}
	pri_cc_event(ctrl, cc_record->signaling, cc_record, CC_EVENT_CANCEL);
}

/* ------------------------------------------------------------------- */
/* end pri_cc.c */
