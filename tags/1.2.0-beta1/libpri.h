/*
 * libpri: An implementation of Primary Rate ISDN
 *
 * Written by Mark Spencer <markster@linux-support.net>
 *
 * Copyright (C) 2001, Linux Support Services, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */
 
#ifndef _LIBPRI_H
#define _LIBPRI_H

/* Node types */
#define PRI_NETWORK		1
#define PRI_CPE			2

/* Debugging */
#define PRI_DEBUG_Q921_RAW		(1 << 0)	/* Show raw HDLC frames */
#define PRI_DEBUG_Q921_DUMP		(1 << 1)	/* Show each interpreted Q.921 frame */
#define PRI_DEBUG_Q921_STATE 	(1 << 2)	/* Debug state machine changes */
#define PRI_DEBUG_CONFIG		(1 << 3) 	/* Display error events on stdout */
#define PRI_DEBUG_Q931_DUMP		(1 << 5)	/* Show interpreted Q.931 frames */
#define PRI_DEBUG_Q931_STATE	(1 << 6)	/* Debug Q.931 state machine changes */
#define	PRI_DEBUG_Q931_ANOMALY 	(1 << 7)	/* Show unexpected events */
#define PRI_DEBUG_APDU			(1 << 8)	/* Debug of APDU components such as ROSE */
#define PRI_DEBUG_AOC			(1 << 9)	/* Debug of Advice of Charge ROSE Messages */

#define PRI_DEBUG_ALL			(0xffff)	/* Everything */

/* Switch types */
#define PRI_SWITCH_UNKNOWN 		0
#define PRI_SWITCH_NI2	   		1	/* National ISDN 2 */
#define PRI_SWITCH_DMS100		2	/* DMS 100 */
#define PRI_SWITCH_LUCENT5E		3	/* Lucent 5E */
#define PRI_SWITCH_ATT4ESS		4	/* AT&T 4ESS */
#define PRI_SWITCH_EUROISDN_E1		5	/* Standard EuroISDN (CTR4, ETSI 300-102) */
#define PRI_SWITCH_EUROISDN_T1		6	/* T1 EuroISDN variant (ETSI 300-102) */
#define PRI_SWITCH_NI1			7	/* National ISDN 1 */
#define PRI_SWITCH_GR303_EOC		8	/* GR-303 Embedded Operations Channel */
#define PRI_SWITCH_GR303_TMC		9	/* GR-303 Timeslot Management Channel */
#define PRI_SWITCH_QSIG			10	/* QSIG Switch */
/* Switchtypes 11 - 20 are reserved for internal use */


/* PRI D-Channel Events */
#define PRI_EVENT_DCHAN_UP		 1	/* D-channel is up */
#define PRI_EVENT_DCHAN_DOWN 	 2	/* D-channel is down */
#define PRI_EVENT_RESTART		 3	/* B-channel is restarted */
#define PRI_EVENT_CONFIG_ERR 	 4	/* Configuration Error Detected */
#define PRI_EVENT_RING			 5	/* Incoming call */
#define PRI_EVENT_HANGUP		 6	/* Call got hung up */
#define PRI_EVENT_RINGING		 7	/* Call is ringing (alerting) */
#define PRI_EVENT_ANSWER		 8	/* Call has been answered */
#define PRI_EVENT_HANGUP_ACK	 9	/* Call hangup has been acknowledged */
#define PRI_EVENT_RESTART_ACK	10	/* Restart complete on a given channel */
#define PRI_EVENT_FACNAME		11	/* Caller*ID Name received on Facility */
#define PRI_EVENT_INFO_RECEIVED 12	/* Additional info (keypad) received */
#define PRI_EVENT_PROCEEDING	13	/* When we get CALL_PROCEEDING or PROGRESS */
#define PRI_EVENT_SETUP_ACK		14	/* When we get SETUP_ACKNOWLEDGE */
#define PRI_EVENT_HANGUP_REQ	15	/* Requesting the higher layer to hangup */
#define PRI_EVENT_NOTIFY		16	/* Notification received */
#define PRI_EVENT_PROGRESS		17	/* When we get CALL_PROCEEDING or PROGRESS */
#define PRI_EVENT_KEYPAD_DIGIT		18	/* When we receive during ACTIVE state */

/* Simple states */
#define PRI_STATE_DOWN		0
#define PRI_STATE_UP		1

#define PRI_PROGRESS_MASK

/* Progress indicator values */
#define PRI_PROG_CALL_NOT_E2E_ISDN						(1 << 0)
#define PRI_PROG_CALLED_NOT_ISDN						(1 << 1)
#define PRI_PROG_CALLER_NOT_ISDN						(1 << 2)
#define PRI_PROG_INBAND_AVAILABLE						(1 << 3)
#define PRI_PROG_DELAY_AT_INTERF						(1 << 4)
#define PRI_PROG_INTERWORKING_WITH_PUBLIC				(1 << 5)
#define PRI_PROG_INTERWORKING_NO_RELEASE				(1 << 6)
#define PRI_PROG_INTERWORKING_NO_RELEASE_PRE_ANSWER		(1 << 7)
#define PRI_PROG_INTERWORKING_NO_RELEASE_POST_ANSWER	(1 << 8)
#define PRI_PROG_CALLER_RETURNED_TO_ISDN					(1 << 9)

/* Numbering plan identifier */
#define PRI_NPI_UNKNOWN					0x0
#define PRI_NPI_E163_E164				0x1
#define PRI_NPI_X121					0x3
#define PRI_NPI_F69						0x4
#define PRI_NPI_NATIONAL				0x8
#define PRI_NPI_PRIVATE					0x9
#define PRI_NPI_RESERVED				0xF

/* Type of number */
#define PRI_TON_UNKNOWN					0x0
#define PRI_TON_INTERNATIONAL			0x1
#define PRI_TON_NATIONAL				0x2
#define PRI_TON_NET_SPECIFIC			0x3
#define PRI_TON_SUBSCRIBER				0x4
#define PRI_TON_ABBREVIATED				0x6
#define PRI_TON_RESERVED				0x7

/* Redirection reasons */
#define PRI_REDIR_UNKNOWN				0x0
#define PRI_REDIR_FORWARD_ON_BUSY		0x1
#define PRI_REDIR_FORWARD_ON_NO_REPLY	0x2
#define PRI_REDIR_DEFLECTION			0x3
#define PRI_REDIR_DTE_OUT_OF_ORDER		0x9
#define PRI_REDIR_FORWARDED_BY_DTE		0xA
#define PRI_REDIR_UNCONDITIONAL			0xF

/* Dialing plan */
#define PRI_INTERNATIONAL_ISDN		0x11
#define PRI_NATIONAL_ISDN			0x21
#define PRI_LOCAL_ISDN				0x41
#define PRI_PRIVATE					0x49
#define PRI_UNKNOWN					0x0

/* Presentation */
#define PRES_ALLOWED_USER_NUMBER_NOT_SCREENED	0x00
#define PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN	0x01
#define PRES_ALLOWED_USER_NUMBER_FAILED_SCREEN	0x02
#define PRES_ALLOWED_NETWORK_NUMBER				0x03
#define PRES_PROHIB_USER_NUMBER_NOT_SCREENED	0x20
#define PRES_PROHIB_USER_NUMBER_PASSED_SCREEN	0x21
#define PRES_PROHIB_USER_NUMBER_FAILED_SCREEN	0x22
#define PRES_PROHIB_NETWORK_NUMBER				0x23
#define PRES_NUMBER_NOT_AVAILABLE				0x43

/* Causes for disconnection */
#define PRI_CAUSE_UNALLOCATED					1
#define PRI_CAUSE_NO_ROUTE_TRANSIT_NET			2
#define PRI_CAUSE_NO_ROUTE_DESTINATION			3
#define PRI_CAUSE_CHANNEL_UNACCEPTABLE			6
#define PRI_CAUSE_CALL_AWARDED_DELIVERED		7
#define PRI_CAUSE_NORMAL_CLEARING				16
#define PRI_CAUSE_USER_BUSY						17
#define PRI_CAUSE_NO_USER_RESPONSE				18
#define PRI_CAUSE_NO_ANSWER						19
#define PRI_CAUSE_CALL_REJECTED					21
#define PRI_CAUSE_NUMBER_CHANGED				22
#define PRI_CAUSE_DESTINATION_OUT_OF_ORDER		27
#define PRI_CAUSE_INVALID_NUMBER_FORMAT			28
#define PRI_CAUSE_FACILITY_REJECTED				29
#define PRI_CAUSE_RESPONSE_TO_STATUS_ENQUIRY	30
#define PRI_CAUSE_NORMAL_UNSPECIFIED			31
#define PRI_CAUSE_NORMAL_CIRCUIT_CONGESTION		34
#define PRI_CAUSE_NETWORK_OUT_OF_ORDER			38
#define PRI_CAUSE_NORMAL_TEMPORARY_FAILURE		41
#define PRI_CAUSE_SWITCH_CONGESTION				42
#define PRI_CAUSE_ACCESS_INFO_DISCARDED			43
#define PRI_CAUSE_REQUESTED_CHAN_UNAVAIL		44
#define PRI_CAUSE_PRE_EMPTED					45
#define PRI_CAUSE_FACILITY_NOT_SUBSCRIBED  		50
#define PRI_CAUSE_OUTGOING_CALL_BARRED     		52
#define PRI_CAUSE_INCOMING_CALL_BARRED     		54
#define PRI_CAUSE_BEARERCAPABILITY_NOTAUTH		57
#define PRI_CAUSE_BEARERCAPABILITY_NOTAVAIL     58
#define PRI_CAUSE_BEARERCAPABILITY_NOTIMPL		65
#define PRI_CAUSE_CHAN_NOT_IMPLEMENTED     		66
#define PRI_CAUSE_FACILITY_NOT_IMPLEMENTED      69
#define PRI_CAUSE_INVALID_CALL_REFERENCE		81
#define PRI_CAUSE_INCOMPATIBLE_DESTINATION		88
#define PRI_CAUSE_INVALID_MSG_UNSPECIFIED  		95
#define PRI_CAUSE_MANDATORY_IE_MISSING			96
#define PRI_CAUSE_MESSAGE_TYPE_NONEXIST			97
#define PRI_CAUSE_WRONG_MESSAGE					98
#define PRI_CAUSE_IE_NONEXIST					99
#define PRI_CAUSE_INVALID_IE_CONTENTS			100
#define PRI_CAUSE_WRONG_CALL_STATE				101
#define PRI_CAUSE_RECOVERY_ON_TIMER_EXPIRE		102
#define PRI_CAUSE_MANDATORY_IE_LENGTH_ERROR		103
#define PRI_CAUSE_PROTOCOL_ERROR				111
#define PRI_CAUSE_INTERWORKING					127

/* Transmit capabilities */
#define PRI_TRANS_CAP_SPEECH					0x0
#define PRI_TRANS_CAP_DIGITAL					0x08
#define PRI_TRANS_CAP_RESTRICTED_DIGITAL		0x09
#define PRI_TRANS_CAP_3_1K_AUDIO				0x10
#define PRI_TRANS_CAP_7K_AUDIO					0x11	/* Depriciated ITU Q.931 (05/1998)*/
#define PRI_TRANS_CAP_DIGITAL_W_TONES			0x11
#define PRI_TRANS_CAP_VIDEO						0x18

#define PRI_LAYER_1_ITU_RATE_ADAPT	0x21
#define PRI_LAYER_1_ULAW			0x22
#define PRI_LAYER_1_ALAW			0x23
#define PRI_LAYER_1_G721			0x24
#define PRI_LAYER_1_G722_G725		0x25
#define PRI_LAYER_1_G7XX_384K		0x26
#define PRI_LAYER_1_NON_ITU_ADAPT	0x27
#define PRI_LAYER_1_V120_RATE_ADAPT	0x28
#define PRI_LAYER_1_X31_RATE_ADAPT	0x29

/* Notifications */
#define PRI_NOTIFY_USER_SUSPENDED		0x00	/* User suspended */
#define PRI_NOTIFY_USER_RESUMED			0x01	/* User resumed */
#define PRI_NOTIFY_BEARER_CHANGE		0x02	/* Bearer service change (DSS1) */
#define PRI_NOTIFY_ASN1_COMPONENT		0x03	/* ASN.1 encoded component (DSS1) */
#define PRI_NOTIFY_COMPLETION_DELAY		0x04	/* Call completion delay */
#define PRI_NOTIFY_CONF_ESTABLISHED		0x42	/* Conference established */
#define PRI_NOTIFY_CONF_DISCONNECTED		0x43	/* Conference disconnected */
#define PRI_NOTIFY_CONF_PARTY_ADDED		0x44	/* Other party added */
#define PRI_NOTIFY_CONF_ISOLATED		0x45	/* Isolated */
#define PRI_NOTIFY_CONF_REATTACHED		0x46	/* Reattached */
#define PRI_NOTIFY_CONF_OTHER_ISOLATED		0x47	/* Other party isolated */
#define PRI_NOTIFY_CONF_OTHER_REATTACHED	0x48	/* Other party reattached */
#define PRI_NOTIFY_CONF_OTHER_SPLIT		0x49	/* Other party split */
#define PRI_NOTIFY_CONF_OTHER_DISCONNECTED	0x4a	/* Other party disconnected */
#define PRI_NOTIFY_CONF_FLOATING		0x4b	/* Conference floating */
#define PRI_NOTIFY_WAITING_CALL			0x60	/* Call is waiting call */
#define PRI_NOTIFY_DIVERSION_ACTIVATED		0x68	/* Diversion activated (DSS1) */
#define PRI_NOTIFY_TRANSFER_ALERTING		0x69	/* Call transfer, alerting */
#define PRI_NOTIFY_TRANSFER_ACTIVE		0x6a	/* Call transfer, active */
#define PRI_NOTIFY_REMOTE_HOLD			0x79	/* Remote hold */
#define PRI_NOTIFY_REMOTE_RETRIEVAL		0x7a	/* Remote retrieval */
#define PRI_NOTIFY_CALL_DIVERTING		0x7b	/* Call is diverting */

#define PRI_COPY_DIGITS_CALLED_NUMBER

/* Network Specific Facilities (AT&T) */
#define PRI_NSF_NONE                   -1
#define PRI_NSF_SID_PREFERRED          0xB1
#define PRI_NSF_ANI_PREFERRED          0xB2
#define PRI_NSF_SID_ONLY               0xB3
#define PRI_NSF_ANI_ONLY               0xB4
#define PRI_NSF_CALL_ASSOC_TSC         0xB9
#define PRI_NSF_NOTIF_CATSC_CLEARING   0xBA
#define PRI_NSF_OPERATOR               0xB5
#define PRI_NSF_PCCO                   0xB6
#define PRI_NSF_SDN                    0xE1
#define PRI_NSF_TOLL_FREE_MEGACOM      0xE2
#define PRI_NSF_MEGACOM                        0xE3
#define PRI_NSF_ACCUNET                        0xE6
#define PRI_NSF_LONG_DISTANCE_SERVICE  0xE7
#define PRI_NSF_INTERNATIONAL_TOLL_FREE        0xE8
#define PRI_NSF_ATT_MULTIQUEST         0xF0
#define PRI_NSF_CALL_REDIRECTION_SERVICE       0xF7

typedef struct q931_call q931_call;

typedef struct pri_event_generic {
	/* Events with no additional information fall in this category */
	int e;
} pri_event_generic;

typedef struct pri_event_error {
	int e;
	char err[256];
} pri_event_error;

typedef struct pri_event_restart {
	int e;
	int channel;
} pri_event_restart;

typedef struct pri_event_ringing {
	int e;
	int channel;
	int cref;
	int progress;
	int progressmask;
	q931_call *call;
} pri_event_ringing;

typedef struct pri_event_answer {
	int e;
	int channel;
	int cref;
	int progress;
	int progressmask;
	q931_call *call;
} pri_event_answer;

typedef struct pri_event_facname {
	int e;
	char callingname[256];
	char callingnum[256];
	int channel;
	int cref;
	q931_call *call;
} pri_event_facname;

#define PRI_CALLINGPLANANI
typedef struct pri_event_ring {
	int e;
	int channel;				/* Channel requested */
	int callingpres;			/* Presentation of Calling CallerID */
	int callingplanani;			/* Dialing plan of Calling entity ANI */
	int callingplan;			/* Dialing plan of Calling entity */
	char callingani[256];		/* Calling ANI */
	char callingnum[256];		/* Calling number */
	char callingname[256];		/* Calling name (if provided) */
	int calledplan;				/* Dialing plan of Called number */
	int ani2;                   /* ANI II */
	char callednum[256];		/* Called number */
	char redirectingnum[256];	/* Redirecting number */
	char redirectingname[256];	/* Redirecting name */
	int redirectingreason;		/* Reason for redirect */
	char useruserinfo[256];		/* User->User info */
	int flexible;				/* Are we flexible with our channel selection? */
	int cref;					/* Call Reference Number */
	int ctype;					/* Call type (see PRI_TRANS_CAP_* */
	int layer1;					/* User layer 1 */
	int complete;				/* Have we seen "Complete" i.e. no more number? */
	q931_call *call;			/* Opaque call pointer */
	char callingsubaddr[256];	/* Calling parties subaddress */
	int progress;
	int progressmask;
	char origcalledname[256];
	char origcallednum[256];
	int origredirectingreason;
} pri_event_ring;

typedef struct pri_event_hangup {
	int e;
	int channel;				/* Channel requested */
	int cause;
	int cref;
	q931_call *call;			/* Opaque call pointer */
	long aoc_units;				/* Advise of Charge number of charged units */
} pri_event_hangup;	

typedef struct pri_event_restart_ack {
	int e;
	int channel;
} pri_event_restart_ack;

#define PRI_PROGRESS_CAUSE
typedef struct pri_event_proceeding {
	int e;
	int channel;
	int cref;
	int progress;
	int progressmask;
	int cause;
	q931_call *call;
} pri_event_proceeding;
 
typedef struct pri_event_setup_ack {
	int e;
	int channel;
} pri_event_setup_ack;

typedef struct pri_event_notify {
	int e;
	int channel;
	int info;
} pri_event_notify;

typedef struct pri_event_keypad_digit {
	int e;
	int channel;
	q931_call *call;
	char digits[64];
} pri_event_keypad_digit;

typedef union {
	int e;
	pri_event_generic gen;		/* Generic view */
	pri_event_restart restart;	/* Restart view */
	pri_event_error	  err;		/* Error view */
	pri_event_facname facname;	/* Caller*ID Name on Facility */
	pri_event_ring	  ring;		/* Ring */
	pri_event_hangup  hangup;	/* Hang up */
	pri_event_ringing ringing;	/* Ringing */
	pri_event_ringing answer;	/* Answer */
	pri_event_restart_ack restartack;	/* Restart Acknowledge */
	pri_event_proceeding  proceeding;	/* Call proceeding & Progress */
	pri_event_setup_ack   setup_ack;	/* SETUP_ACKNOWLEDGE structure */
	pri_event_notify notify;		/* Notification */
	pri_event_keypad_digit digit;			/* Digits that come during a call */
} pri_event;

struct pri;
struct pri_sr;

#define PRI_IO_FUNCS
/* Type declaration for callbacks to read or write a HDLC frame as below */
typedef int (*pri_io_cb)(struct pri *pri, void *buf, int buflen);

/* Create a D-channel on a given file descriptor.  The file descriptor must be a
   channel operating in HDLC mode with FCS computed by the fd's driver.  Also it
   must be NON-BLOCKING! Frames received on the fd should include FCS.  Nodetype 
   must be one of PRI_NETWORK or PRI_CPE.  switchtype should be PRI_SWITCH_* */
extern struct pri *pri_new(int fd, int nodetype, int switchtype);

/* Create D-channel just as above with user defined I/O callbacks and data */
extern struct pri *pri_new_cb(int fd, int nodetype, int switchtype, pri_io_cb io_read, pri_io_cb io_write, void *userdata);

/* Retrieve the user data associated with the D channel */
extern void *pri_get_userdata(struct pri *pri);

/* Set the user data associated with the D channel */
extern void pri_set_userdata(struct pri *pri, void *userdata);

/* Set Network Specific Facility for PRI */
extern void pri_set_nsf(struct pri *pri, int nsf);

/* Set debug parameters on PRI -- see above debug definitions */
extern void pri_set_debug(struct pri *pri, int debug);

/* Get debug parameters on PRI -- see above debug definitions */
extern int pri_get_debug(struct pri *pri);

#define PRI_FACILITY_ENABLE
/* Enable transmission support of Facility IEs on the pri */
extern void pri_facility_enable(struct pri *pri);

/* Run PRI on the given D-channel, taking care of any events that
   need to be handled.  If block is set, it will block until an event
   occurs which needs to be handled */
extern pri_event *pri_dchannel_run(struct pri *pri, int block);

/* Check for an outstanding event on the PRI */
pri_event *pri_check_event(struct pri *pri);

/* Give a name to a given event ID */
extern char *pri_event2str(int id);

/* Give a name to a node type */
extern char *pri_node2str(int id);

/* Give a name to a switch type */
extern char *pri_switch2str(int id);

/* Print an event */
extern void pri_dump_event(struct pri *pri, pri_event *e);

/* Turn presentation into a string */
extern char *pri_pres2str(int pres);

/* Turn numbering plan into a string */
extern char *pri_plan2str(int plan);

/* Turn cause into a string */
extern char *pri_cause2str(int cause);

/* Acknowledge a call and place it on the given channel.  Set info to non-zero if there
   is in-band data available on the channel */
extern int pri_acknowledge(struct pri *pri, q931_call *call, int channel, int info);

/* Send a digit in overlap mode */
extern int pri_information(struct pri *pri, q931_call *call, char digit);

/* Answer the incomplete(call without called number) call on the given channel.
   Set non-isdn to non-zero if you are not connecting to ISDN equipment */
extern int pri_need_more_info(struct pri *pri, q931_call *call, int channel, int nonisdn);

/* Answer the call on the given channel (ignored if you called acknowledge already).
   Set non-isdn to non-zero if you are not connecting to ISDN equipment */
extern int pri_answer(struct pri *pri, q931_call *call, int channel, int nonisdn);

/* Set CRV reference for GR-303 calls */


#undef pri_release
#undef pri_disconnect

/* backwards compatibility for those who don't use asterisk with libpri */
#define pri_release(a,b,c) \
	pri_hangup(a,b,c)

#define pri_disconnect(a,b,c) \
	pri_hangup(a,b,c)

/* Hangup a call */
#define PRI_HANGUP
extern int pri_hangup(struct pri *pri, q931_call *call, int cause);

#define PRI_DESTROYCALL
extern void pri_destroycall(struct pri *pri, q931_call *call);

extern int pri_reset(struct pri *pri, int channel);

/* Create a new call */
extern q931_call *pri_new_call(struct pri *pri);

/* Retrieve CRV reference for GR-303 calls.  Returns >0 on success. */
extern int pri_get_crv(struct pri *pri, q931_call *call, int *callmode);

/* Retrieve CRV reference for GR-303 calls.  CRV must be >0, call mode should be 0 */
extern int pri_set_crv(struct pri *pri, q931_call *call, int crv, int callmode);

/* How long until you need to poll for a new event */
extern struct timeval *pri_schedule_next(struct pri *pri);

/* Run any pending schedule events */
extern pri_event *pri_schedule_run(struct pri *pri);

extern int pri_call(struct pri *pri, q931_call *c, int transmode, int channel,
   int exclusive, int nonisdn, char *caller, int callerplan, char *callername, int callerpres,
	 char *called,int calledplan, int ulayer1);

extern struct pri_sr *pri_sr_new(void);
extern void pri_sr_free(struct pri_sr *sr);

extern int pri_sr_set_channel(struct pri_sr *sr, int channel, int exclusive, int nonisdn);
extern int pri_sr_set_bearer(struct pri_sr *sr, int transmode, int userl1);
extern int pri_sr_set_called(struct pri_sr *sr, char *called, int calledplan, int complete);
extern int pri_sr_set_caller(struct pri_sr *sr, char *caller, char *callername, int callerplan, int callerpres);
extern int pri_sr_set_redirecting(struct pri_sr *sr, char *num, int plan, int pres, int reason);

extern int pri_setup(struct pri *pri, q931_call *call, struct pri_sr *req);

/* Set a call has a call indpendent signalling connection (i.e. no bchan) */
extern int pri_sr_set_connection_call_independent(struct pri_sr *req);

/* Send an MWI indication to a remote location.  If activate is non zero, activates, if zero, decativates */
extern int pri_mwi_activate(struct pri *pri, q931_call *c, char *caller, int callerplan, char *callername, int callerpres, char *called, int calledplan);

/* Send an MWI deactivate request to a remote location */
extern int pri_mwi_deactivate(struct pri *pri, q931_call *c, char *caller, int callerplan, char *callername, int callerpres, char *called, int calledplan);

#define PRI_2BCT
/* Attempt to pass the channels back to the NET side if compatable and
 * suscribed.  Sometimes called 2 bchannel transfer (2BCT) */
int pri_channel_bridge(q931_call *call1, q931_call *call2);

/* Override message and error stuff */
#define PRI_NEW_SET_API
extern void pri_set_message(void (*__pri_error)(struct pri *pri, char *));
extern void pri_set_error(void (*__pri_error)(struct pri *pri, char *));

/* Set overlap mode */
#define PRI_SET_OVERLAPDIAL
extern void pri_set_overlapdial(struct pri *pri,int state);

#define PRI_DUMP_INFO_STR
extern char *pri_dump_info_str(struct pri *pri);

/* Get file descriptor */
extern int pri_fd(struct pri *pri);

#define PRI_PROGRESS
/* Send call proceeding */
extern int pri_progress(struct pri *pri, q931_call *c, int channel, int info);

#define PRI_PROCEEDING_FULL
/* Send call proceeding */
extern int pri_proceeding(struct pri *pri, q931_call *c, int channel, int info);

/* Enslave a PRI to another, so they share the same call list
   (and maybe some timers) */
extern void pri_enslave(struct pri *master, struct pri *slave);

#define PRI_GR303_SUPPORT
#define PRI_ENSLAVE_SUPPORT
#define PRI_SETUP_CALL
#define PRI_RECEIVE_SUBADDR
#define PRI_REDIRECTING_REASON
#define PRI_AOC_UNITS
#define PRI_ANI

/* Send notification */
extern int pri_notify(struct pri *pri, q931_call *c, int channel, int info);

/* Get/Set PRI Timers  */
#define PRI_GETSET_TIMERS
extern int pri_set_timer(struct pri *pri, int timer, int value);
extern int pri_get_timer(struct pri *pri, int timer);
extern int pri_timer2idx(char *timer);

#define PRI_MAX_TIMERS 32

#define PRI_TIMER_N200	0	/* Maximum numer of q921 retransmissions */
#define PRI_TIMER_N201	1	/* Maximum numer of octets in an information field */
#define PRI_TIMER_N202	2	/* Maximum numer of transmissions of the TEI identity request message */
#define PRI_TIMER_K	3	/* Maximum number of outstanding I-frames */

#define PRI_TIMER_T200	4	/* time between SABME's */
#define PRI_TIMER_T201	5	/* minimum time between retransmissions of the TEI Identity check messages */
#define PRI_TIMER_T202	6	/* minimum time between transmission of TEI Identity request messages */
#define PRI_TIMER_T203	7	/* maxiumum time without exchanging packets */

#define PRI_TIMER_T300	8	
#define PRI_TIMER_T301	9	/* maximum time to respond to an ALERT */
#define PRI_TIMER_T302	10
#define PRI_TIMER_T303	11	/* maximum time to wait after sending a SETUP without a response */
#define PRI_TIMER_T304	12
#define PRI_TIMER_T305	13
#define PRI_TIMER_T306	14
#define PRI_TIMER_T307	15
#define PRI_TIMER_T308	16
#define PRI_TIMER_T309	17
#define PRI_TIMER_T310	18	/* maximum time between receiving a CALLPROCEEDING and receiving a ALERT/CONNECT/DISCONNECT/PROGRESS */
#define PRI_TIMER_T313	19
#define PRI_TIMER_T314	20
#define PRI_TIMER_T316	21	/* maximum time between transmitting a RESTART and receiving a RESTART ACK */
#define PRI_TIMER_T317	22
#define PRI_TIMER_T318	23
#define PRI_TIMER_T319	24
#define PRI_TIMER_T320	25
#define PRI_TIMER_T321	26
#define PRI_TIMER_T322	27

#endif
