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
 
#ifndef _PRI_Q931_H
#define _PRI_Q931_H

typedef enum q931_state {
	/* User states */
	U0_NULL_STATE,
	U1_CALL_INITIATED,
	U2_OVERLAP_SENDING,
	U3_OUTGOING_CALL_PROCEEDING,
	U4_CALL_DELIVERED,
	U6_CALL_PRESENT,
	U7_CALL_RECEIVED,
	U8_CONNECT_REQUEST,
	U9_INCOMING_CALL_PROCEEDING,
	U10_ACTIVE,
	U11_DISCONNECT_REQUEST,
	U12_DISCONNECT_INDICATION,
	U15_SUSPEND_REQUEST,
	U17_RESUME_REQUEST,
	U19_RELEASE_REQUEST,
	U25_OVERLAP_RECEIVING,
	/* Network states */
	N0_NULL_STATE,
	N1_CALL_INITIATED,
	N2_OVERLAP_SENDING,
	N3_OUTGOING_CALL_PROCEEDING,
	N4_CALL_DELIVERED,
	N6_CALL_PRESENT,
	N7_CALL_RECEIVED,
	N8_CONNECT_REQUEST,
	N9_INCOMING_CALL_PROCEEDING,
	N10_ACTIVE,
	N11_DISCONNECT_REQUEST,
	N12_DISCONNECT_INDICATION,
	N15_SUSPEND_REQUEST,
	N17_RESUME_REQUEST,
	N19_RELEASE_REQUEST,
	N22_CALL_ABORT,
	N25_OVERLAP_RECEIVING
} q931_state;

typedef enum q931_mode {
	UNKNOWN_MODE,
	CIRCUIT_MODE,
	PACKET_MODE
} q931_mode;

typedef struct q931_h {
	unsigned char raw[0];
	u_int8_t pd;		/* Protocol Discriminator */
#if __BYTE_ORDER == __BIG_ENDIAN
	u_int8_t x0:4;
	u_int8_t crlen:4;
#else
	u_int8_t crlen:4;
	u_int8_t x0:4;
#endif
	u_int8_t contents[0];
	u_int8_t crv[3];
} q931_h;


/* Message type header */
typedef struct q931_mh {
#if __BYTE_ORDER == __BIG_ENDIAN
	u_int8_t f:1;
	u_int8_t msg:7;
#else
	u_int8_t msg:7;
	u_int8_t f:1;
#endif
	u_int8_t data[0];
} q931_mh;

/* Information element format */
typedef struct q931_ie {
#if __BYTE_ORDER == __BIG_ENDIAN
	u_int8_t f:1;
	u_int8_t ie:7;
#else
	u_int8_t ie:7;
	u_int8_t f:1;
#endif
	u_int8_t len;
	u_int8_t data[0];
} q931_ie;

#define Q931_RES_HAVEEVENT (1 << 0)
#define Q931_RES_INERRROR  (1 << 1)

#define Q931_PROTOCOL_DISCRIMINATOR 0x08

/* Q.931 / National ISDN Message Types */

/* Call Establishment Messages */
#define Q931_ALERTING 				0x01
#define Q931_CALL_PROCEEDING		0x02
#define Q931_CONNECT				0x07
#define Q931_CONNECT_ACKNOWLEDGE	0x0f
#define Q931_PROGRESS				0x03
#define Q931_SETUP					0x05
#define Q931_SETUP_ACKNOWLEDGE		0x0d

/* Call Disestablishment Messages */
#define Q931_DISCONNECT				0x45
#define Q931_RELEASE				0x4d
#define Q931_RELEASE_COMPLETE		0x5a
#define Q931_RESTART				0x46
#define Q931_RESTART_ACKNOWLEDGE	0x4e

/* Miscellaneous Messages */
#define Q931_STATUS					0x7d
#define Q931_STATUS_ENQUIRY			0x75
#define Q931_USER_INFORMATION		0x20
#define Q931_SEGMENT				0x60
#define Q931_CONGESTION_CONTROL		0x79
#define Q931_INFORMATION			0x7b
#define Q931_FACILITY				0x62
#define Q931_NOTIFY					0x6e

/* Call Management Messages */
#define Q931_HOLD					0x24
#define Q931_HOLD_ACKNOWLEDGE		0x28
#define Q931_HOLD_REJECT			0x30
#define Q931_RETRIEVE				0x31
#define Q931_RETRIEVE_ACKNOWLEDGE	0x33
#define Q931_RETRIEVE_REJECT		0x37
#define Q931_RESUME					0x26
#define Q931_RESUME_ACKNOWLEDGE		0x2e
#define Q931_RESUME_REJECT			0x22
#define Q931_SUSPEND				0x25
#define Q931_SUSPEND_ACKNOWLEDGE	0x2d
#define Q931_SUSPEND_REJECT			0x21

/* Maintenance messages (codeset 0 only) */
#define NATIONAL_SERVICE			0x0f
#define NATIONAL_SERVICE_ACKNOWLEDGE	0x07

/* Special codeset 0 IE */
#define	NATIONAL_CHANGE_STATUS		0x1

/* Q.931 / National ISDN Information Elements */
#define Q931_LOCKING_SHIFT			0x90
#define Q931_BEARER_CAPABILITY		0x04
#define Q931_CAUSE					0x08
#define Q931_CALL_STATE				0x14
#define Q931_CHANNEL_IDENT			0x18
#define Q931_PROGRESS_INDICATOR		0x1e
#define Q931_NETWORK_SPEC_FAC		0x20
#define Q931_INFORMATION_RATE		0x40
#define Q931_TRANSIT_DELAY			0x42
#define Q931_TRANS_DELAY_SELECT		0x43
#define Q931_BINARY_PARAMETERS		0x44
#define Q931_WINDOW_SIZE			0x45
#define Q931_PACKET_SIZE			0x46
#define Q931_CLOSED_USER_GROUP		0x47
#define Q931_REVERSE_CHARGE_INDIC	0x4a
#define Q931_CALLING_PARTY_NUMBER	0x6c
#define Q931_CALLING_PARTY_SUBADDR	0x6d
#define Q931_CALLED_PARTY_NUMBER	0x70
#define Q931_CALLED_PARTY_SUBADDR	0x71
#define Q931_REDIRECTING_NUMBER		0x74
#define Q931_REDIRECTING_SUBADDR	0x75
#define Q931_TRANSIT_NET_SELECT		0x78
#define Q931_RESTART_INDICATOR		0x79
#define Q931_LOW_LAYER_COMPAT		0x7c
#define Q931_HIGH_LAYER_COMPAT		0x7d

#define Q931_IE_SEGMENTED_MSG			0x00
#define Q931_IE_CHANGE_STATUS			0x01
#define Q931_IE_CONNECTED_NUM			0x0C
#define Q931_IE_CALL_IDENTITY			0x10
#define Q931_IE_FACILITY				0x1c
#define Q931_IE_ENDPOINT_ID				0x26
#define Q931_IE_NOTIFY_IND				0x27
#define Q931_IE_DISPLAY					0x28
#define Q931_IE_TIME_DATE				0x29
#define Q931_IE_KEYPAD_FACILITY			0x2c
#define Q931_IE_CALL_STATUS				0x2d
#define Q931_IE_UPDATE                  0x31
#define Q931_IE_INFO_REQUEST            0x32
#define Q931_IE_SIGNAL					0x34
#define Q931_IE_SWITCHHOOK				0x36
#define Q931_IE_FEATURE_ACTIVATE		0x38
#define Q931_IE_FEATURE_IND				0x39
#define Q931_IE_ORIGINAL_CALLED_NUMBER 	0x73
#define Q931_IE_REDIRECTION_NUMBER		0x76
#define Q931_IE_REDIRECTION_SUBADDR		0x77
#define Q931_IE_USER_USER_FACILITY		0x7A
#define Q931_IE_USER_USER				0x7E
#define Q931_IE_ESCAPE_FOR_EXT			0x7F

/* EuroISDN  */
#define Q931_SENDING_COMPLETE		0x21

extern int q931_receive(struct pri *pri, q931_h *h, int len);

extern int q931_alerting(struct pri *pri, q931_call *call, int channel, int info);

extern int q931_call_proceeding(struct pri *pri, q931_call *call);

extern int q931_connect(struct pri *pri, q931_call *call, int channel, int nonisdn);

extern int q931_release(struct pri *pri, q931_call *call, int cause);

extern int q931_disconnect(struct pri *pri, q931_call *call, int cause);

extern q931_call *q931_new_call(struct pri *pri);

extern int q931_setup(struct pri *pri, q931_call *c, int transmode, int channel, int exclusive, 
					int nonisdn, char *caller, int callerplan, int callerpres, char *called,
					int calledplan, int ulay1);
extern void q931_dump(q931_h *h, int len, int txrx);
#endif
