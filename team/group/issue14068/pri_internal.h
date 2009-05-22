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

#define ARRAY_LEN(arr)	(sizeof(arr) / sizeof((arr)[0]))

#define DBGHEAD __FILE__ ":%d %s: "
#define DBGINFO __LINE__,__PRETTY_FUNCTION__

/* BUGBUG Eliminate the need for the DIVERTEDSTATE_xxx defines below */
/* divertedstate */
#define DIVERTEDSTATE_NONE				0
#define DIVERTEDSTATE_DIVERTED			1
#define DIVERTEDSTATE_DIVLEGINFO1SEND	2
#define DIVERTEDSTATE_DIVLEGINFO3SEND	3

struct pri_sched {
	struct timeval when;
	void (*callback)(void *data);
	void *data;
};

struct q921_frame;
enum q931_state;
enum q931_mode;

/* No more than 128 scheduled events */
#define MAX_SCHED 128

#define MAX_TIMERS 32

/*! \brief D channel controller structure */
struct pri {
	int fd;				/* File descriptor for D-Channel */
	pri_io_cb read_func;		/* Read data callback */
	pri_io_cb write_func;		/* Write data callback */
	void *userdata;
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
	int timers[MAX_TIMERS];

	/* Used by scheduler */
	struct timeval tv;
	int schedev;
	pri_event ev;		/* Static event thingy */
	
	/* Q.921 Re-transmission queue */
	struct q921_frame *txqueue;
	
	/* Q.931 calls */
	q931_call **callpool;
	q931_call *localpool;

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

	short last_invoke;	/* Last ROSE invoke ID */
	unsigned char sendfacility;
};

/*! \brief New call setup parameter structure */
struct pri_sr {
	int transmode;
	int channel;
	int exclusive;
	int nonisdn;
	char *caller;
	int callerplan;
	char *callername;
	int callerpres;
	char *called;
	int calledplan;
	int userl1;
	int numcomplete;
	char *redirectingname;
	char *redirectingnum;
	int redirectingplan;
	int redirectingpres;
	int redirectingreason;
	int justsignalling;
	const char *useruserinfo;
	int transferable;
};

/* Internal switch types */
#define PRI_SWITCH_GR303_EOC_PATH	19
#define PRI_SWITCH_GR303_TMC_SWITCHING	20

struct apdu_event {
	int message;			/* What message to send the ADPU in */
	void (*callback)(void *data);	/* Callback function for when response is received */
	void *data;			/* Data to callback */
	unsigned char apdu[255];			/* ADPU to send */
	int apdu_len; 			/* Length of ADPU */
	int sent;  			/* Have we been sent already? */
	struct apdu_event *next;	/* Linked list pointer */
};

enum Q931_PARTY_DATA_STATUS {
	/*! Data is not initialized or available. */
	Q931_PARTY_DATA_STATUS_INVALID,
	/*! Data is valid, available, and has been handled. */
	Q931_PARTY_DATA_STATUS_VALID,
	/*! Data is valid and available but has changed. */
	Q931_PARTY_DATA_STATUS_CHANGED,
};

/*! \brief Maximum name length plus null terminator (From ECMA-164) */
#define PRI_MAX_NAME_LEN		(50 + 1)

/*! \brief Q.SIG name information. */
struct q931_party_name {
	enum Q931_PARTY_DATA_STATUS status;
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
	/*! \brief Name data with terminator. */
	char str[PRI_MAX_NAME_LEN];
};

/*! \brief Maximum phone number (address) length plus null terminator */
#define PRI_MAX_NUMBER_LEN		(31 + 1)

struct q931_party_number {
	enum Q931_PARTY_DATA_STATUS status;
	/*!
	 * \brief Q.931 presentation-indicator and screening-indicator encoded fields
	 */
	unsigned char presentation;
	/*! \brief Q.931 Type-Of-Number and numbering-plan encoded fields */
	unsigned char plan;
	/*! \brief Number data with terminator. */
	char str[PRI_MAX_NUMBER_LEN];
};

/*! \brief Information needed to identify an endpoint in a call. */
struct q931_party_id {
	struct q931_party_name name;
	struct q931_party_number number;
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

/*! \brief Redirecting information struct */
struct q931_party_redirecting {
	/*! \brief Who is redirecting the call (Sent to the party the call is redirected toward) */
	struct q931_party_id from;
	/*! \brief Call is redirecting to a new party (Sent to the caller) */
	struct q931_party_id to;
	enum Q931_REDIRECTING_STATE state;
	/*! \brief Number of times the call was redirected */
	unsigned char count;
	/*! \brief Redirection reasons */
	unsigned char reason;
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

	/* Bearer Capability */
	int transcapability;
	int transmoderate;
	int transmultiple;
	int userl1;
	int userl2;
	int userl3;
	int rateadaption;

	int justsignalling;		/* for a signalling-only connection */

	int progcode;			/* Progress coding */
	int progloc;			/* Progress Location */	
	int progress;			/* Progress indicator */
	int progressmask;		/* Progress Indicator bitmask */
	
	int notify;				/* Notification */
	
	int causecode;			/* Cause Coding */
	int causeloc;			/* Cause Location */
	int cause;				/* Cause of clearing */
	
	int peercallstate;		/* Call state of peer as reported */
	int ourcallstate;		/* Our call state */
	int sugcallstate;		/* Status call state */

	char keypad_digits[64];		/* Buffer for digits that come in KEYPAD_FACILITY */

	int ani2;               /* ANI II */

/* BUGBUG need to check usage of elements in caller_id */
	struct q931_party_id caller_id;

/* BUGBUG need to check usage of elements in called_name */
	struct q931_party_name called_name;

	/*! \note called_number.presentation is not used */
/* BUGBUG need to check usage of elements in called_number */
/* BUGBUG Overlap dialing cannot wipe the called_number.str.  It needs to append and also put the digits in the keypad_digits. */
	struct q931_party_number called_number;
	int nonisdn;
	int complete;			/* no more digits coming */
	int newcall;			/* if the received message has a new call reference value */

	int retranstimer;		/* Timer for retransmitting DISC */
	int t308_timedout;		/* Whether t308 timed out once */

/* BUGBUG need to check usage of elements in redirecting */
/* BUGBUG need to check usage of elements in redirecting.from */
/* BUGBUG need to check usage of elements in redirecting.to */
	struct q931_party_redirecting redirecting;

	/* Filled in cases of multiple diversions */
	int origredirectingreason;	/* Original reason for redirect (in cases of multiple redirects) */
	/*! Originally called party */
/* BUGBUG need to check usage of elements in orig_called */
	struct q931_party_id orig_called;

/* BUGBUG need to check usage of elements in connected_line */
	struct q931_party_id connected_line;
/* BUGBUG need to check usage of elements in ct_complete */
	struct q931_party_id ct_complete;
/* BUGBUG need to check usage of elements in ct_active */
	struct q931_party_id ct_active;

	/* divertingLegInformation1 */
	int divleginfo1activeflag;

	/* divertingLegInformation3 */
	int divleginfo3activeflag;

	/* callTransferComplete */
	int ctcompleteflag;
	int ctcompletecallstatus;

	/* callTransferActive */
	int ctactiveflag;

	int useruserprotocoldisc;
	char useruserinfo[256];
	char callingsubaddr[256];	/* Calling party subaddress */
	
	long aoc_units;				/* Advice of Charge Units */

	struct apdu_event *apdus;	/* APDU queue for call */

	int transferable;			/* RLT call is transferable */
	unsigned int rlt_call_id;	/* RLT call id */

	/* Bridged call info */
	q931_call *bridged_call;        /* Pointer to other leg of bridged call (Used by Q.SIG when eliminating tromboned calls) */

	int changestatus;		/* SERVICE message changestatus */
};

extern int pri_schedule_event(struct pri *pri, int ms, void (*function)(void *data), void *data);

extern pri_event *pri_schedule_run(struct pri *pri);

extern void pri_schedule_del(struct pri *pri, int ev);

extern pri_event *pri_mkerror(struct pri *pri, char *errstr);

extern void pri_message(struct pri *pri, char *fmt, ...);

extern void pri_error(struct pri *pri, char *fmt, ...);

void libpri_copy_string(char *dst, const char *src, size_t size);

struct pri *__pri_new_tei(int fd, int node, int switchtype, struct pri *master, pri_io_cb rd, pri_io_cb wr, void *userdata, int tei, int bri);

void __pri_free_tei(struct pri *p);

void q931_party_name_init(struct q931_party_name *name);
void q931_party_number_init(struct q931_party_number *number);
void q931_party_id_init(struct q931_party_id *id);
void q931_party_redirecting_init(struct q931_party_redirecting *redirecting);
int q931_party_id_presentation(const struct q931_party_id *id);

#endif
