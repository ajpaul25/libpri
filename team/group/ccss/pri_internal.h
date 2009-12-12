/*
 * libpri: An implementation of Primary Rate ISDN
 *
 * Written by Mark Spencer <markster@digium.com>
 *
 * Copyright (C) 2001, Digium, Inc.
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
 
#ifndef _PRI_INTERNAL_H
#define _PRI_INTERNAL_H

#include <stddef.h>
#include <sys/time.h>
#include "pri_q921.h"
#include "pri_q931.h"

#define ARRAY_LEN(arr)	(sizeof(arr) / sizeof((arr)[0]))

#define DBGHEAD __FILE__ ":%d %s: "
#define DBGINFO __LINE__,__PRETTY_FUNCTION__

/* Forward declare some structs */
struct apdu_event;
struct pri_cc_record;

struct pri_sched {
	struct timeval when;
	void (*callback)(void *data);
	void *data;
};

/*
 * libpri needs to be able to allocate B channels to support Q.SIG path reservation.
 * Until that happens, path reservation is not possible.  Fortunately,
 * path reservation is optional with a fallback to what we can implement.
 */
//#define QSIG_PATH_RESERVATION_SUPPORT	1

/*! Maximum number of scheduled events active at the same time. */
#define MAX_SCHED	(128 + 256) /* 256 CC supervision timer events */

/*! Maximum number of facility ie's to handle per incoming message. */
#define MAX_FACILITY_IES	8

/*! Accumulated pri_message() line until a '\n' is seen on the end. */
struct pri_msg_line {
	/*! Accumulated buffer used. */
	unsigned length;
	/*! Accumulated pri_message() contents. */
	char str[2048];
};

/*! \brief D channel controller structure */
struct pri {
	int fd;				/* File descriptor for D-Channel */
	pri_io_cb read_func;		/* Read data callback */
	pri_io_cb write_func;		/* Write data callback */
	void *userdata;
	/*! Accumulated pri_message() line. (Valid in master record only) */
	struct pri_msg_line *msg_line;
	struct pri *subchannel;	/* Sub-channel if appropriate */
	struct pri *master;		/* Master channel if appropriate */
	struct pri_sched pri_sched[MAX_SCHED];	/* Scheduled events */
	int debug;			/* Debug stuff */
	int state;			/* State of D-channel */
	int switchtype;		/* Switch type */
	int nsf;		/* Network-Specific Facility (if any) */
	int localtype;		/* Local network type (unknown, network, cpe) */
	int remotetype;		/* Remote network type (unknown, network, cpe) */

	int sapi;
	int tei;
	int protodisc;
	unsigned int bri:1;
	unsigned int acceptinbanddisconnect:1;	/* Should we allow inband progress after DISCONNECT? */
	unsigned int hold_support:1;/* TRUE if upper layer supports call hold. */
	unsigned int deflection_support:1;/* TRUE if upper layer supports call deflection/rerouting. */
	unsigned int cc_support:1;/* TRUE if upper layer supports call completion. */

	/* Q.921 State */
	int q921_state;	
	int window;			/* Max window size */
	int windowlen;		/* Fullness of window */
	int v_s;			/* Next N(S) for transmission */
	int v_a;			/* Last acknowledged frame */
	int v_r;			/* Next frame expected to be received */
	int v_na;			/* What we've told our peer we've acknowledged */
	int solicitfbit;	/* Have we sent an I or S frame with the F-bit set? */
	int retrans;		/* Retransmissions */
	int sentrej;		/* Are we in reject state */
	
	int cref;			/* Next call reference value */
	
	int busy;			/* Peer is busy */

	/* Various timers */
	int sabme_timer;	/* SABME retransmit */
	int sabme_count;	/* SABME retransmit counter for BRI */
	int t203_timer;		/* Max idle time */
	int t202_timer;
	int n202_counter;
	int ri;
	int t200_timer;		/* T-200 retransmission timer */
	/* All ISDN Timer values */
	int timers[PRI_MAX_TIMERS];

	/* Used by scheduler */
	struct timeval tv;
	int schedev;
	pri_event ev;		/* Static event thingy */
	/*! Subcommands for static event thingy. */
	struct pri_subcommands subcmds;
	
	/* Q.921 Re-transmission queue */
	struct q921_frame *txqueue;
	
	/* Q.931 calls */
	q931_call **callpool;
	q931_call *localpool;

	/*!
	 * \brief Q.931 Dummy call reference call associated with this TEI.
	 * \note If present then this call is allocated as part of the
	 * D channel control structure.
	 */
	q931_call *dummy_call;

	/* do we do overlap dialing */
	int overlapdial;

	/* do we support SERVICE messages */
	int service_message_support;

	/* do not skip channel 16 */
	int chan_mapping_logical;

#ifdef LIBPRI_COUNTERS
	/* q921/q931 packet counters */
	unsigned int q921_txcount;
	unsigned int q921_rxcount;
	unsigned int q931_txcount;
	unsigned int q931_rxcount;
#endif

	short last_invoke;	/* Last ROSE invoke ID (Valid in master record only) */
	unsigned char sendfacility;

	/*! Call completion (Valid in master record only) */
	struct {
		/*! Active CC records */
		struct pri_cc_record *pool;
		/*! Last CC record id allocated. */
		unsigned short last_record_id;
		/*! Last CC PTMP reference id allocated. (0-127) */
		unsigned char last_reference_id;
		/*! Last CC PTMP linkage id allocated. (0-127) */
		unsigned char last_linkage_id;
		/*! Configured CC options. */
		struct {
			/*! PTMP recall mode: globalRecall(0), specificRecall(1) */
			unsigned char recall_mode;
			/*! TRUE if can retain cc service if party B is unavailable again. */
			unsigned char retain_service;
			/*! Q.SIG Request signaling link retention: release(0), retain(1), do-not-care(2) */
			unsigned char signaling_retention_req;
			/*! Q.SIG Response request signaling link retention: release(0), retain(1) */
			unsigned char signaling_retention_rsp;
#if defined(QSIG_PATH_RESERVATION_SUPPORT)
			/*! Q.SIG TRUE if response request can support path reservation. */
			unsigned char allow_path_reservation;
#endif	/* defined(QSIG_PATH_RESERVATION_SUPPORT) */
		} option;
	} cc;

	/*! For delayed processing of facility ie's. */
	struct {
		/*! Array of facility ie locations in the current received message. */
		q931_ie *ie[MAX_FACILITY_IES];
		/*! Codeset facility ie found within. */
		unsigned char codeset[MAX_FACILITY_IES];
		/*! Number of facility ie's in the array from the current received message. */
		unsigned char count;
	} facility;
};

/*! \brief Maximum name length plus null terminator (From ECMA-164) */
#define PRI_MAX_NAME_LEN		(50 + 1)

/*! \brief Q.SIG name information. */
struct q931_party_name {
	/*! \brief TRUE if name data is valid */
	unsigned char valid;
	/*!
	 * \brief Q.931 presentation-indicator encoded field
	 * \note Must tollerate the Q.931 screening-indicator field values being present.
	 */
	unsigned char presentation;
	/*!
	 * \brief Character set the name is using.
	 * \details
	 * unknown(0),
	 * iso8859-1(1),
	 * enum-value-withdrawn-by-ITU-T(2)
	 * iso8859-2(3),
	 * iso8859-3(4),
	 * iso8859-4(5),
	 * iso8859-5(6),
	 * iso8859-7(7),
	 * iso10646-BmpString(8),
	 * iso10646-utf-8String(9)
	 */
	unsigned char char_set;
	/*! \brief Name data with null terminator. */
	char str[PRI_MAX_NAME_LEN];
};

/*! \brief Maximum phone number (address) length plus null terminator */
#define PRI_MAX_NUMBER_LEN		(31 + 1)

struct q931_party_number {
	/*! \brief TRUE if number data is valid */
	unsigned char valid;
	/*! \brief Q.931 presentation-indicator and screening-indicator encoded fields */
	unsigned char presentation;
	/*! \brief Q.931 Type-Of-Number and numbering-plan encoded fields */
	unsigned char plan;
	/*! \brief Number data with terminator. */
	char str[PRI_MAX_NUMBER_LEN];
};

/*! \brief Maximum subaddress length plus null terminator */
#define PRI_MAX_SUBADDRESS_LEN	(20 + 1)

struct q931_party_subaddress {
	/*! \brief TRUE if the subaddress information is valid/present */
	unsigned char valid;
	/*!
	 * \brief Subaddress type.
	 * \details
	 * nsap(0),
	 * user_specified(2)
	 */
	unsigned char type;
	/*!
	 * \brief TRUE if odd number of address signals
	 * \note The odd/even indicator is used when the type of subaddress is
	 * user_specified and the coding is BCD.
	 */
	unsigned char odd_even_indicator;
	/*! \brief Length of the subaddress data */
	unsigned char length;
	/*!
	 * \brief Subaddress data with null terminator.
	 * \note The null terminator is a convenience only since the data could be
	 * BCD/binary and thus have a null byte as part of the contents.
	 */
	unsigned char data[PRI_MAX_SUBADDRESS_LEN];
};

struct q931_party_address {
	/*! \brief Subscriber phone number */
	struct q931_party_number number;
	/*! \brief Subscriber subaddress */
	struct q931_party_subaddress subaddress;
};

/*! \brief Information needed to identify an endpoint in a call. */
struct q931_party_id {
	/*! \brief Subscriber name */
	struct q931_party_name name;
	/*! \brief Subscriber phone number */
	struct q931_party_number number;
	/*! \brief Subscriber subaddress */
	struct q931_party_subaddress subaddress;
};

enum Q931_REDIRECTING_STATE {
	/*!
	 * \details
	 * CDO-Idle/CDF-Inv-Idle
	 */
	Q931_REDIRECTING_STATE_IDLE,
	/*!
	 * \details
	 * CDF-Inv-Wait - A DivLeg2 has been received and
	 * we are waiting for valid presentation restriction information to send.
	 */
	Q931_REDIRECTING_STATE_PENDING_TX_DIV_LEG_3,
	/*!
	 * \details
	 * CDO-Divert - A DivLeg1 has been received and
	 * we are waiting for the presentation restriction information to come in.
	 */
	Q931_REDIRECTING_STATE_EXPECTING_RX_DIV_LEG_3,
};

/*!
 * \brief Do not increment above this count.
 * \details
 * It is not our responsibility to enforce the maximum number of redirects.
 * However, we cannot allow an increment past this number without breaking things.
 * Besides, more than 255 redirects is probably not a good thing.
 */
#define PRI_MAX_REDIRECTS	0xFF

/*! \brief Redirecting information struct */
struct q931_party_redirecting {
	enum Q931_REDIRECTING_STATE state;
	/*! \brief Who is redirecting the call (Sent to the party the call is redirected toward) */
	struct q931_party_id from;
	/*! \brief Call is redirecting to a new party (Sent to the caller) */
	struct q931_party_id to;
	/*! Originally called party (in cases of multiple redirects) */
	struct q931_party_id orig_called;
	/*!
	 * \brief Number of times the call was redirected
	 * \note The call is being redirected if the count is non-zero.
	 */
	unsigned char count;
	/*! Original reason for redirect (in cases of multiple redirects) */
	unsigned char orig_reason;
	/*! \brief Redirection reasons */
	unsigned char reason;
};

/*! \brief New call setup parameter structure */
struct pri_sr {
	int transmode;
	int channel;
	int exclusive;
	int nonisdn;
	struct q931_party_redirecting redirecting;
	struct q931_party_id caller;
	struct q931_party_address called;
	int userl1;
	int numcomplete;
	int cis_call;
	int cis_auto_disconnect;
	const char *useruserinfo;
	const char *keypad_digits;
	int transferable;
	int reversecharge;
};

/* Internal switch types */
#define PRI_SWITCH_GR303_EOC_PATH	19
#define PRI_SWITCH_GR303_TMC_SWITCHING	20

#define Q931_MAX_TEI	8

/*! \brief Incoming call transfer states. */
enum INCOMING_CT_STATE {
	/*!
	 * \details
	 * Incoming call transfer is not active.
	 */
	INCOMING_CT_STATE_IDLE,
	/*!
	 * \details
	 * We have seen an incoming CallTransferComplete(alerting)
	 * so we are waiting for the expected CallTransferActive
	 * before updating the connected line about the remote party id.
	 */
	INCOMING_CT_STATE_EXPECT_CT_ACTIVE,
	/*!
	 * \details
	 * A call transfer message came in that updated the remote party id
	 * that we need to post a connected line update.
	 */
	INCOMING_CT_STATE_POST_CONNECTED_LINE
};

/*! Call hold supplementary states. */
enum Q931_HOLD_STATE {
	/*! \brief No call hold activity. */
	Q931_HOLD_STATE_IDLE,
	/*! \brief Request made to hold call. */
	Q931_HOLD_STATE_HOLD_REQ,
	/*! \brief Request received to hold call. */
	Q931_HOLD_STATE_HOLD_IND,
	/*! \brief Call is held. */
	Q931_HOLD_STATE_CALL_HELD,
	/*! \brief Request made to retrieve call. */
	Q931_HOLD_STATE_RETRIEVE_REQ,
	/*! \brief Request received to retrieve call. */
	Q931_HOLD_STATE_RETRIEVE_IND,
};

/* Only save the first of each BC, HLC, and LLC from the initial SETUP. */
#define CC_SAVED_IE_BC	(1 << 0)	/*!< BC has already been saved. */
#define CC_SAVED_IE_HLC	(1 << 1)	/*!< HLC has already been saved. */
#define CC_SAVED_IE_LLC	(1 << 2)	/*!< LLC has already been saved. */

/*! Saved ie contents for BC, HLC, and LLC. (Only the first of each is saved.) */
struct q931_saved_ie_contents {
	/*! Length of saved ie contents. */
	unsigned char length;
	/*! Saved ie contents data. */
	unsigned char data[
		/* Bearer Capability has a max length of 12. */
		12
		/* High Layer Compatibility has a max length of 5. */
		+ 5
		/* Low Layer Compatibility has a max length of 18. */
		+ 18
		/* Room for null terminator just in case. */
		+ 1];
};

/*! Digested BC parameters. */
struct decoded_bc {
	int transcapability;
	int transmoderate;
	int transmultiple;
	int userl1;
	int userl2;
	int userl3;
	int rateadaption;
};

/* q931_call datastructure */
struct q931_call {
	struct pri *pri;	/* PRI */
	int cr;				/* Call Reference */
	q931_call *next;
	/* Slotmap specified (bitmap of channels 31/24-1) (Channel Identifier IE) (-1 means not specified) */
	int slotmap;
	/* An explicit channel (Channel Identifier IE) (-1 means not specified) */
	int channelno;
	/* An explicit DS1 (-1 means not specified) */
	int ds1no;
	/* Whether or not the ds1 is explicitly identified or implicit.  If implicit
	   the bchan is on the same span as the current active dchan (NFAS) */
	int ds1explicit;
	/* Channel flags (0 means none retrieved) */
	int chanflags;
	
	int alive;			/* Whether or not the call is alive */
	int acked;			/* Whether setup has been acked or not */
	int sendhangupack;	/* Whether or not to send a hangup ack */
	int proc;			/* Whether we've sent a call proceeding / alerting */
	
	int ri;				/* Restart Indicator (Restart Indicator IE) */

	/*! Bearer Capability */
	struct decoded_bc bc;

	/*!
	 * \brief TRUE if the call is a Call Independent Signalling connection.
	 * \note The call has no B channel associated with it. (Just signalling)
	 */
	int cis_call;
	/*! \brief TRUE if we will auto disconnect the cis_call we originated. */
	int cis_auto_disconnect;

	int progcode;			/* Progress coding */
	int progloc;			/* Progress Location */	
	int progress;			/* Progress indicator */
	int progressmask;		/* Progress Indicator bitmask */
	
	int notify;				/* Notification indicator. */
	
	int causecode;			/* Cause Coding */
	int causeloc;			/* Cause Location */
	int cause;				/* Cause of clearing */
	
	enum Q931_CALL_STATE peercallstate;	/* Call state of peer as reported */
	enum Q931_CALL_STATE ourcallstate;	/* Our call state */
	enum Q931_CALL_STATE sugcallstate;	/* Status call state */

	int ani2;               /* ANI II */

	/*! Buffer for digits that come in KEYPAD_FACILITY */
	char keypad_digits[32 + 1];

	/*! Current dialed digits to be sent or just received. */
	char overlap_digits[PRI_MAX_NUMBER_LEN];

	/*!
	 * \brief Local party ID
	 * \details
	 * The Caller-ID and connected-line ID are just roles the local and remote party
	 * play while a call is being established.  Which roll depends upon the direction
	 * of the call.
	 * Outgoing party info is to identify the local party to the other end.
	 *    (Caller-ID for originated or connected-line for answered calls.)
	 * Incoming party info is to identify the remote party to us.
	 *    (Caller-ID for answered or connected-line for originated calls.)
	 */
	struct q931_party_id local_id;
	/*!
	 * \brief Remote party ID
	 * \details
	 * The Caller-ID and connected-line ID are just roles the local and remote party
	 * play while a call is being established.  Which roll depends upon the direction
	 * of the call.
	 * Outgoing party info is to identify the local party to the other end.
	 *    (Caller-ID for originated or connected-line for answered calls.)
	 * Incoming party info is to identify the remote party to us.
	 *    (Caller-ID for answered or connected-line for originated calls.)
	 */
	struct q931_party_id remote_id;

	/*!
	 * \brief Staging place for the Q.931 redirection number ie.
	 * \note
	 * The number could be the remote_id.number or redirecting.to.number
	 * depending upon the notification indicator.
	 */
	struct q931_party_number redirection_number;

	/*!
	 * \brief Called party address.
	 * \note The called.number.str is the accumulated overlap dial digits
	 * and enbloc digits.
	 * \note The called.number.presentation value is not used.
	 */
	struct q931_party_address called;
	int nonisdn;
	int complete;			/* no more digits coming */
	int newcall;			/* if the received message has a new call reference value */

	int retranstimer;		/* Timer for retransmitting DISC */
	int t308_timedout;		/* Whether t308 timed out once */

	struct q931_party_redirecting redirecting;

	/*! \brief Incoming call transfer state. */
	enum INCOMING_CT_STATE incoming_ct_state;
	/*! Call hold supplementary state. */
	enum Q931_HOLD_STATE hold_state;
	/*! Call hold event timer */
	int hold_timer;

	int deflection_in_progress;	/*!< CallDeflection for NT PTMP in progress. */
	/*! TRUE if the connected number ie was in the current received message. */
	int connected_number_in_message;
	/*! TRUE if the redirecting number ie was in the current received message. */
	int redirecting_number_in_message;

	int useruserprotocoldisc;
	char useruserinfo[256];
	
	long aoc_units;				/* Advice of Charge Units */

	struct apdu_event *apdus;	/* APDU queue for call */

	int transferable;			/* RLT call is transferable */
	unsigned int rlt_call_id;	/* RLT call id */

	/* Bridged call info */
	q931_call *bridged_call;        /* Pointer to other leg of bridged call (Used by Q.SIG when eliminating tromboned calls) */

	int changestatus;		/* SERVICE message changestatus */
	int reversecharge;		/* Reverse charging indication:
							   -1 - No reverse charging
							    1 - Reverse charging
							0,2-7 - Reserved for future use */
	int t303_timer;
	int t303_expirycnt;

	int hangupinitiated;
	/*! \brief TRUE if we broadcast this call's SETUP message. */
	int outboundbroadcast;
	int performing_fake_clearing;
	/*!
	 * \brief Master call controlling this call.
	 * \note Always valid.  Master and normal calls point to self.
	 */
	struct q931_call *master_call;

	/* These valid in master call only */
	struct q931_call *subcalls[Q931_MAX_TEI];
	int pri_winner;

	/* Call completion */
	struct {
		/*!
		 * \brief CC record associated with this call.
		 * \note
		 * CC signaling link or original call when cc-available indicated.
		 */
		struct pri_cc_record *record;
		/*! Original calling party. */
		struct q931_party_id party_a;
		/*! Saved BC, HLC, and LLC from initial SETUP */
		struct q931_saved_ie_contents saved_ie_contents;
		/*! Only save the first of each BC, HLC, and LLC from the initial SETUP. */
		unsigned char saved_ie_flags;
		/*! TRUE if the remote party is party B. */
		unsigned char party_b_is_remote;
		/*! TRUE if call needs to be hung up. */
		unsigned char hangup_call;
	} cc;
};

enum CC_STATES {
	/*! CC is not active. */
	CC_STATE_IDLE,
	// /*! CC has recorded call information in anticipation of CC availability. */
	// CC_STATE_RECORD_RETENTION,
	/*! CC is available and waiting on ALERTING or DISCONNECT to go out. */
	CC_STATE_PENDING_AVAILABLE,
	/*! CC is available and waiting on possible CC request. */
	CC_STATE_AVAILABLE,
	/*! CC is requested to be activated and waiting on party B to acknowledge. */
	CC_STATE_REQUESTED,
	/*! CC is activated and waiting for party B to become available. */
	CC_STATE_ACTIVATED,
	/*! CC party B is available and waiting for status of party A. */
	CC_STATE_B_AVAILABLE,
	/*! CC is suspended because party A is not available. (Monitor party A.) */
	CC_STATE_SUSPENDED,
	/*! CC is waiting for party A to initiate CC callback. */
	CC_STATE_WAIT_CALLBACK,
	/*! CC callback in progress. */
	CC_STATE_CALLBACK,
	/*! CC is waiting for signaling link to be cleared before destruction. */
	CC_STATE_WAIT_DESTRUCTION,

	/*! Number of CC states.  Must be last in enum. */
	CC_STATE_NUM
};

enum CC_EVENTS {
	/*! CC is available for the current call. */
	CC_EVENT_AVAILABLE,
	/*! Requesting CC activation. */
	CC_EVENT_CC_REQUEST,
	/*! Requesting CC activation accepted. */
	CC_EVENT_CC_REQUEST_ACCEPT,
	/*! Requesting CC activation failed (error/reject received). */
	CC_EVENT_CC_REQUEST_FAIL,
	/*! CC party B is available. */
	CC_EVENT_REMOTE_USER_FREE,
	/*! CC party B is available, party A is busy or CCBS busy. */
	CC_EVENT_B_FREE,
	/*! CC poll/prompt for party A status. */
	CC_EVENT_A_STATUS,
	/*! CC party A is free/available for recall. */
	CC_EVENT_A_FREE,
	/*! CC party A is busy/not-available for recall. */
	CC_EVENT_A_BUSY,
	/*! Suspend monitoring party B because party A is busy. */
	CC_EVENT_SUSPEND,
	/*! Resume monitoring party B because party A is now available. */
	CC_EVENT_RESUME,
	/*! This is the CC recall call attempt. */
	CC_EVENT_RECALL,
	/*! Link request to cancel/deactivate CC received. */
	CC_EVENT_LINK_CANCEL,
	/*! Tear down CC request from upper layer. */
	CC_EVENT_CANCEL,
	/*! Tear down of CC signaling link completed. */
	CC_EVENT_SIGNALING_GONE,
	/*! Sent ALERTING message. */
	CC_EVENT_MSG_ALERTING,
	/*! Sent DISCONNECT message. */
	CC_EVENT_MSG_DISCONNECT,
	/*! Sent RELEASE message. */
	CC_EVENT_MSG_RELEASE,
	/*! Sent RELEASE_COMPLETE message. */
	CC_EVENT_MSG_RELEASE_COMPLETE,
	/*! T_RETENTION timer timed out. */
	CC_EVENT_TIMEOUT_T_RETENTION,
	/*! T-STATUS timer equivalent for CC user A status timed out. */
	CC_EVENT_TIMEOUT_T_CCBS1,
	/*! Timeout for valid party A status. */
	CC_EVENT_TIMEOUT_EXTENDED_T_CCBS1,
	/*! Max time the CCBS/CCNR service will be active. */
	CC_EVENT_TIMEOUT_T_CCBS2,
	/*! Max time to wait for user A to respond to user B availability. */
	CC_EVENT_TIMEOUT_T_CCBS3,
};

enum CC_PARTY_A_AVAILABILITY {
	CC_PARTY_A_AVAILABILITY_INVALID,
	CC_PARTY_A_AVAILABILITY_BUSY,
	CC_PARTY_A_AVAILABILITY_FREE,
};

/* Invalid PTMP call completion reference and linkage id value. */
#define CC_PTMP_INVALID_ID  0xFF

/*! \brief Call-completion record */
struct pri_cc_record {
	/*! Next call-completion record in the list */
	struct pri_cc_record *next;
	/*!
	 * \brief Associated signaling link. (NULL if not established.)
	 * \note
	 * PTMP - Broadcast dummy call reference call.
	 * (If needed, the TE side could use this pointer to locate its specific
	 * dummy call reference call.)
	 * \note
	 * PTP - REGISTER signaling link.
	 * \note
	 * Q.SIG - SETUP signaling link.
	 */
	struct q931_call *signaling;
	/*! Call-completion record id (0 - 65535) */
	long record_id;
	/*! Call-completion state */
	enum CC_STATES state;
	/*! Original calling party. */
	struct q931_party_id party_a;
	/*! Original called party. */
	struct q931_party_address party_b;
	/*! Saved BC, HLC, and LLC from initial SETUP */
	struct q931_saved_ie_contents saved_ie_contents;
	/*! Saved decoded BC */
	struct decoded_bc bc;

	/*! FSM parameters. */
	union {
		/*! PTMP FSM parameters. */
		struct {
			/*! T_RETENTION timer id. */
			int t_retention;
			/*! Extended T_CCBS1 timer id for CCBSStatusRequest handling. */
			int extended_t_ccbs1;
			/*! T_CCBS2/T_CCNR2 timer id.  CC service supervision timer. */
			int t_ccbs2;
			/*! T_CCBS3 timer id. A response to B available timer. */
			int t_ccbs3;
			/*! Invoke id for the CCBSStatusRequest message to find if T_CCBS1 still running. */
			int t_ccbs1_invoke_id;
			/*! Accumulating party A availability status */
			enum CC_PARTY_A_AVAILABILITY party_a_status_acc;
			/*! Party A availability status */
			enum CC_PARTY_A_AVAILABILITY party_a_status;
			/*! TRUE if no B channels available for recall call. */
			unsigned char is_ccbs_busy;
		} ptmp;
		/*! PTP FSM parameters. */
		struct {
			/*! T_CCBS5/T_CCBS6/T_CCNR5/T_CCNR6 timer id.  CC service supervision timer. */
			int t_supervision;
		} ptp;
	} fsm;
	/*! Pending response information. */
	struct {
		/*! Send response on this signaling link. */
		struct q931_call *signaling;
		/*! Invoke operation code */
		int invoke_operation;
		/*! Invoke id to use in the pending response. */
		short invoke_id;
	} response;

	/*! TRUE if the call-completion FSM has completed and this record needs to be destroyed. */
	unsigned char fsm_complete;
	/*! TRUE if we are a call completion agent. */
	unsigned char is_agent;
	/*! TRUE if the remote party is party B. (We are a monitor./We are the originator.) */
	unsigned char party_b_is_remote;
	/*! TRUE if active cc mode is CCNR. */
	unsigned char is_ccnr;
	/*! PTMP pre-activation reference id. (0-127) */
	unsigned char call_linkage_id;
	/*! PTMP active CCBS reference id. (0-127) */
	unsigned char ccbs_reference_id;
	/*! Negotiated options */
	struct {
		/*! PTMP recall mode: globalRecall(0), specificRecall(1) */
		unsigned char recall_mode;
		/*! TRUE if can retain cc service if party B is unavailable again. */
		unsigned char retain_service;
		/*! TRUE if Q.SIG signaling link is retained. */
		unsigned char retain_signaling_link;
#if defined(QSIG_PATH_RESERVATION_SUPPORT)
		/*! Q.SIG TRUE if can do path reservation. */
		unsigned char do_path_reservation;
#endif	/* defined(QSIG_PATH_RESERVATION_SUPPORT) */
	} option;
};

/*! D channel control structure with associated dummy call reference record. */
struct d_ctrl_dummy {
	/*! D channel control structure. Must be first in the structure. */
	struct pri ctrl;
	/*! Dummy call reference call record. */
	struct q931_call dummy_call;
};

extern int pri_schedule_event(struct pri *pri, int ms, void (*function)(void *data), void *data);

extern pri_event *pri_schedule_run(struct pri *pri);

extern void pri_schedule_del(struct pri *pri, int ev);

extern pri_event *pri_mkerror(struct pri *pri, char *errstr);

void pri_message(struct pri *ctrl, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void pri_error(struct pri *ctrl, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

void libpri_copy_string(char *dst, const char *src, size_t size);

struct pri *__pri_new_tei(int fd, int node, int switchtype, struct pri *master, pri_io_cb rd, pri_io_cb wr, void *userdata, int tei, int bri);
void __pri_free_tei(struct pri *p);

void q931_init_call_record(struct pri *ctrl, struct q931_call *call, int cr);

void pri_sr_init(struct pri_sr *req);

void q931_party_name_init(struct q931_party_name *name);
void q931_party_number_init(struct q931_party_number *number);
void q931_party_subaddress_init(struct q931_party_subaddress *subaddr);
void q931_party_address_init(struct q931_party_address *address);
void q931_party_id_init(struct q931_party_id *id);
void q931_party_redirecting_init(struct q931_party_redirecting *redirecting);

static inline void q931_party_address_to_id(struct q931_party_id *id, struct q931_party_address *address)
{
	id->number = address->number;
	id->subaddress = address->subaddress;
}

int q931_party_name_cmp(const struct q931_party_name *left, const struct q931_party_name *right);
int q931_party_number_cmp(const struct q931_party_number *left, const struct q931_party_number *right);
int q931_party_subaddress_cmp(const struct q931_party_subaddress *left, const struct q931_party_subaddress *right);
int q931_party_address_cmp(const struct q931_party_address *left, const struct q931_party_address *right);
int q931_party_id_cmp(const struct q931_party_id *left, const struct q931_party_id *right);
int q931_party_id_cmp_address(const struct q931_party_id *left, const struct q931_party_id *right);

int q931_cmp_party_id_to_address(const struct q931_party_id *id, const struct q931_party_address *address);
void q931_party_id_copy_to_address(struct q931_party_address *address, const struct q931_party_id *id);

void q931_party_name_copy_to_pri(struct pri_party_name *pri_name, const struct q931_party_name *q931_name);
void q931_party_number_copy_to_pri(struct pri_party_number *pri_number, const struct q931_party_number *q931_number);
void q931_party_subaddress_copy_to_pri(struct pri_party_subaddress *pri_subaddress, const struct q931_party_subaddress *q931_subaddress);
void q931_party_address_copy_to_pri(struct pri_party_address *pri_address, const struct q931_party_address *q931_address);
void q931_party_id_copy_to_pri(struct pri_party_id *pri_id, const struct q931_party_id *q931_id);
void q931_party_redirecting_copy_to_pri(struct pri_party_redirecting *pri_redirecting, const struct q931_party_redirecting *q931_redirecting);

void q931_party_id_fixup(const struct pri *ctrl, struct q931_party_id *id);
int q931_party_id_presentation(const struct q931_party_id *id);

const char *q931_call_state_str(enum Q931_CALL_STATE callstate);
const char *msg2str(int msg);

int q931_is_ptmp(const struct pri *ctrl);
int q931_master_pass_event(struct pri *ctrl, struct q931_call *subcall, int msg_type);
struct pri_subcommand *q931_alloc_subcommand(struct pri *ctrl);

int q931_notify_redirection(struct pri *ctrl, q931_call *call, int notify, const struct q931_party_number *number);

struct pri_cc_record *pri_cc_find_by_reference(struct pri *ctrl, unsigned reference_id);
struct pri_cc_record *pri_cc_find_by_linkage(struct pri *ctrl, unsigned linkage_id);
struct pri_cc_record *pri_cc_find_by_addressing(struct pri *ctrl, const struct q931_party_address *party_a, const struct q931_party_address *party_b);
int pri_cc_new_reference_id(struct pri *ctrl);
void pri_cc_delete_record(struct pri *ctrl, struct pri_cc_record *doomed);
struct pri_cc_record *pri_cc_new_record(struct pri *ctrl, q931_call *call);
int pri_cc_event(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event);
int q931_cc_timeout(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, enum CC_EVENTS event);
void q931_cc_indirect(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record, void (*func)(struct pri *ctrl, q931_call *call, struct pri_cc_record *cc_record));

static inline struct pri * PRI_MASTER(struct pri *mypri)
{
	struct pri *pri = mypri;
	
	if (!pri)
		return NULL;

	while (pri->master)
		pri = pri->master;

	return pri;
}

static inline int BRI_NT_PTMP(struct pri *mypri)
{
	struct pri *pri;

	pri = PRI_MASTER(mypri);

	return pri->bri && (((pri)->localtype == PRI_NETWORK) && ((pri)->tei == Q921_TEI_GROUP));
}

static inline int BRI_TE_PTMP(struct pri *mypri)
{
	struct pri *pri;

	pri = PRI_MASTER(mypri);

	return pri->bri && (((pri)->localtype == PRI_CPE) && ((pri)->tei == Q921_TEI_GROUP));
}

static inline int PRI_PTP(struct pri *mypri)
{
	struct pri *pri;

	pri = PRI_MASTER(mypri);

	return !pri->bri;
}

#define Q931_DUMMY_CALL_REFERENCE	-1

/*!
 * \brief Deterimine if the given call control pointer is a dummy call.
 *
 * \retval TRUE if given call is a dummy call.
 * \retval FALSE otherwise.
 */
static inline int q931_is_dummy_call(const q931_call *call)
{
	return (call->cr == Q931_DUMMY_CALL_REFERENCE) ? 1 : 0;
}

static inline short get_invokeid(struct pri *ctrl)
{
	ctrl = PRI_MASTER(ctrl);
	return ++ctrl->last_invoke;
}

#endif
