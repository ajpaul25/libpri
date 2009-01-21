/*
 * libpri: An implementation of Primary Rate ISDN
 *
 * Written by Matthew Fredrickson <creslin@digium.com>
 *
 * Copyright (C) 2004-2005, Digium, Inc.
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

#include "compat.h"
#include "libpri.h"
#include "pri_internal.h"
#include "pri_q921.h"
#include "pri_q931.h"
#include "pri_facility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static char *asn1id2text(int id)
{
	static char data[32];
	static char *strings[] = {
		"none",
		"Boolean",
		"Integer",
		"Bit String",
		"Octet String",
		"NULL",
		"Object Identifier",
		"Object Descriptor",
		"External Reference",
		"Real Number",
		"Enumerated",
		"Embedded PDV",
		"UTF-8 String",
		"Relative Object ID",
		"Reserved (0e)",
		"Reserved (0f)",
		"Sequence",
		"Set",
		"Numeric String",
		"Printable String",
		"Tele-Text String",
		"IA-5 String",
		"UTC Time",
		"Generalized Time",
	};
	if (id > 0 && id <= 0x18) {
		return strings[id];
	} else {
		sprintf(data, "Unknown (%02x)", id);
		return data;
	}
}

static int asn1_dumprecursive(struct pri *pri, void *comp_ptr, int len, int level)
{
	unsigned char *vdata = (unsigned char *)comp_ptr;
	struct rose_component *comp;
	int i = 0;
	int j, k, l;
	int clen = 0;

	while (len > 0) {
		GET_COMPONENT(comp, i, vdata, len);
		pri_message(pri, "%*s%02X %04X", 2 * level, "", comp->type, comp->len);
		if ((comp->type == 0) && (comp->len == 0))
			return clen + 2;
		if ((comp->type & ASN1_PC_MASK) == ASN1_PRIMITIVE) {
			for (j = 0; j < comp->len; ++j)
				pri_message(pri, " %02X", comp->data[j]);
		}
		if ((comp->type & ASN1_CLAN_MASK) == ASN1_UNIVERSAL) {
			switch (comp->type & ASN1_TYPE_MASK) {
			case 0:
				pri_message(pri, " (none)");
				break;
			case ASN1_BOOLEAN:
				pri_message(pri, " (BOOLEAN: %d)", comp->data[0]);
				break;
			case ASN1_INTEGER:
				for (k = l = 0; k < comp->len; ++k)
					l = (l << 8) | comp->data[k];
				pri_message(pri, " (INTEGER: %d)", l);
				break;
			case ASN1_BITSTRING:
				pri_message(pri, " (BITSTRING:");
				for (k = 0; k < comp->len; ++k)
				pri_message(pri, " %02x", comp->data[k]);
				pri_message(pri, ")");
				break;
			case ASN1_OCTETSTRING:
				pri_message(pri, " (OCTETSTRING:");
				for (k = 0; k < comp->len; ++k)
					pri_message(pri, " %02x", comp->data[k]);
				pri_message(pri, ")");
				break;
			case ASN1_NULL:
				pri_message(pri, " (NULL)");
				break;
			case ASN1_OBJECTIDENTIFIER:
				pri_message(pri, " (OBJECTIDENTIFIER:");
				for (k = 0; k < comp->len; ++k)
					pri_message(pri, " %02x", comp->data[k]);
				pri_message(pri, ")");
				break;
			case ASN1_ENUMERATED:
				for (k = l = 0; k < comp->len; ++k)
					l = (l << 8) | comp->data[k];
				pri_message(pri, " (ENUMERATED: %d)", l);
				break;
			case ASN1_SEQUENCE:
				pri_message(pri, " (SEQUENCE)");
				break;
			default:
				pri_message(pri, " (component %02x - %s)", comp->type, asn1id2text(comp->type & ASN1_TYPE_MASK));
				break;
			}
		}
		else if ((comp->type & ASN1_CLAN_MASK) == ASN1_CONTEXT_SPECIFIC) {
			pri_message(pri, " (CONTEXT SPECIFIC [%d])", comp->type & ASN1_TYPE_MASK);
		}
		else {
			pri_message(pri, " (component %02x)", comp->type);
		}
		pri_message(pri, "\n");
		if ((comp->type & ASN1_PC_MASK) == ASN1_CONSTRUCTOR)
			j = asn1_dumprecursive(pri, comp->data, (comp->len ? comp->len : INT_MAX), level+1);
		else
			j = comp->len;
		j += 2;
		len -= j;
		vdata += j;
		clen += j;
	}
	return clen;
}

int asn1_dump(struct pri *pri, void *comp, int len)
{
	return asn1_dumprecursive(pri, comp, len, 0);
}

static unsigned char get_invokeid(struct pri *pri)
{
	return ++pri->last_invoke;
}

struct addressingdataelements_presentednumberunscreened {
	char partyaddress[21];
	char notused[21];
	int  npi;       /* Numbering Plan Indicator */
	int  ton;       /* Type Of Number */
	int  pres;      /* Presentation */
};

struct addressingdataelements_presentednumberscreened {
	char partyaddress[21];
	char notused[21];
	int  npi;       /* Numbering Plan Indicator */
	int  ton;       /* Type Of Number */
	int  pres;      /* Presentation */
	int  scrind;    /* Screening Indicator */
};

struct addressingdataelements_presentedaddressscreened {
	char partyaddress[21];
	char partysubaddress[21];
	int  npi;       /* Numbering Plan Indicator */
	int  ton;       /* Type Of Number */
	int  pres;      /* Presentation */
	int  scrind;    /* Screening Indicator */
};

struct addressingdataelements_addressscreened {
	char partyaddress[21];
	char partysubaddress[21];
	int  npi;       /* Numbering Plan Indicator */
	int  ton;       /* Type Of Number */
	int  notused;
	int  scrind;    /* Screening Indicator */
};

struct addressingdataelements_partysubaddress {
	char notused[21];
	char partysubaddress[21];
};

struct nameelements_name {
	char name[51];
	int  characterset;
	int  namepres;
};

struct nameelements_nameset {
	char name[51];
	int  characterset;
};

struct nameelements_namedata {
	char name[51];
};

#define PRI_CHECKOVERFLOW(size) \
		if (msgptr - message + (size) >= sizeof(message)) { \
			*msgptr = '\0'; \
			pri_message(pri, "%s", message); \
			msgptr = message; \
		}

static void dump_apdu(struct pri *pri, unsigned char *c, int len) 
{
	#define MAX_APDU_LENGTH	255
	static char hexs[16] = "0123456789ABCDEF";
	int i;
	char message[(2 + MAX_APDU_LENGTH * 3 + 6 + MAX_APDU_LENGTH + 3)] = "";	/* please adjust here, if you make changes below! */
	char *msgptr;
	
	msgptr = message;
	*msgptr++ = ' ';
	*msgptr++ = '[';
	for (i=0; i<len; i++) {
		PRI_CHECKOVERFLOW(3);
		*msgptr++ = ' ';
		*msgptr++ = hexs[(c[i] >> 4) & 0x0f];
		*msgptr++ = hexs[(c[i]) & 0x0f];
	}
	PRI_CHECKOVERFLOW(6);
	strcpy(msgptr, " ] - [");
	msgptr += strlen(msgptr);
	for (i=0; i<len; i++) {
		PRI_CHECKOVERFLOW(1);
		*msgptr++ = ((c[i] < ' ') || (c[i] > '~')) ? '.' : c[i];
	}
	PRI_CHECKOVERFLOW(2);
	*msgptr++ = ']';
	*msgptr++ = '\n';
	*msgptr = '\0';
	pri_message(pri, "%s", message);
}
#undef PRI_CHECKOVERFLOW

static const char *namepres_to_str(int namepres)
{
	return (namepres == 0) ? "Restricted" : "Allowed";
}

static const char *characterset_to_str(int characterset)
{
	switch (characterset) {
	case CHARACTER_SET_UNKNOWN:
		return "Unknown";
	case CHARACTER_SET_ISO8859_1:
		return "ISO8859-1";
	case CHARACTER_SET_ISO8859_2:
		return "ISO8859-2";
	case CHARACTER_SET_ISO8859_3:
		return "ISO8859-3";
	case CHARACTER_SET_ISO8859_4:
		return "ISO8859-4";
	case CHARACTER_SET_ISO8859_5:
		return "ISO8859-5";
	case CHARACTER_SET_ISO8859_7:
		return "ISO8859-7";
	default:
		return "illegal value";
	}
}

static const char *diversionreason_to_str(struct pri *pri, int diversionreason)
{
	if (pri->switchtype == PRI_SWITCH_QSIG) {
		switch (diversionreason) {
		case QSIG_DIVERT_REASON_UNKNOWN:
			return "Unknown";
		case QSIG_DIVERT_REASON_CFU:
			return "Call Forwarding Unconditional";
		case QSIG_DIVERT_REASON_CFB:
			return "Call Forwarding Busy";
		case QSIG_DIVERT_REASON_CFNR:
			return "Call Forwarding No Reply";
		default:
			return "invalid value";
		}
	} else {
		switch(diversionreason) {
		case Q952_DIVERT_REASON_UNKNOWN:
			return "Unknown";
		case Q952_DIVERT_REASON_CFU:
			return "Call Forwarding Unconditional";
		case Q952_DIVERT_REASON_CFB:
			return "Call Forwarding Busy";
		case Q952_DIVERT_REASON_CFNR:
			return "Call Forwarding No Reply";
		case Q952_DIVERT_REASON_CD:
			return "Call Deflection";
		case Q952_DIVERT_REASON_IMMEDIATE:
			return "Call Deflection Immediate";
		default:
			return "invalid value";
		}
	}
}

static const char *callstatus_to_str(int callstatus)
{
	switch (callstatus) {
	case 0:
		return "answered";
	case 1:
		return "alerting";
	default:
		return "illegal value";
	}
}

static const char *enddesignation_to_str(int enddesignation)
{
	switch (enddesignation) {
	case 0:
		return "primaryEnd";
	case 1:
		return "secondaryEnd";
	default:
		return "illegal value";
	}
}

int redirectingreason_from_q931(struct pri *pri, int redirectingreason)
{
	switch(pri->switchtype) {
		case PRI_SWITCH_QSIG:
			switch(redirectingreason) {
				case PRI_REDIR_UNKNOWN:
					return QSIG_DIVERT_REASON_UNKNOWN;
				case PRI_REDIR_FORWARD_ON_BUSY:
					return QSIG_DIVERT_REASON_CFB;
				case PRI_REDIR_FORWARD_ON_NO_REPLY:
					return QSIG_DIVERT_REASON_CFNR;
				case PRI_REDIR_UNCONDITIONAL:
					return QSIG_DIVERT_REASON_CFU;
				case PRI_REDIR_DEFLECTION:
				case PRI_REDIR_DTE_OUT_OF_ORDER:
				case PRI_REDIR_FORWARDED_BY_DTE:
					pri_message(pri, "!! Don't know how to convert Q.931 redirection reason %d to Q.SIG\n", redirectingreason);
					/* Fall through */
				default:
					return QSIG_DIVERT_REASON_UNKNOWN;
			}
		default:
			switch(redirectingreason) {
				case PRI_REDIR_UNKNOWN:
					return Q952_DIVERT_REASON_UNKNOWN;
				case PRI_REDIR_FORWARD_ON_BUSY:
					return Q952_DIVERT_REASON_CFB;
				case PRI_REDIR_FORWARD_ON_NO_REPLY:
					return Q952_DIVERT_REASON_CFNR;
				case PRI_REDIR_DEFLECTION:
					return Q952_DIVERT_REASON_CD;
				case PRI_REDIR_UNCONDITIONAL:
					return Q952_DIVERT_REASON_CFU;
				case PRI_REDIR_DTE_OUT_OF_ORDER:
				case PRI_REDIR_FORWARDED_BY_DTE:
					pri_message(pri, "!! Don't know how to convert Q.931 redirection reason %d to Q.952\n", redirectingreason);
					/* Fall through */
				default:
					return Q952_DIVERT_REASON_UNKNOWN;
			}
	}
}

static int redirectingreason_for_q931(struct pri *pri, int redirectingreason)
{
	switch(pri->switchtype) {
		case PRI_SWITCH_QSIG:
			switch(redirectingreason) {
				case QSIG_DIVERT_REASON_UNKNOWN:
					return PRI_REDIR_UNKNOWN;
				case QSIG_DIVERT_REASON_CFU:
					return PRI_REDIR_UNCONDITIONAL;
				case QSIG_DIVERT_REASON_CFB:
					return PRI_REDIR_FORWARD_ON_BUSY;
				case QSIG_DIVERT_REASON_CFNR:
					return PRI_REDIR_FORWARD_ON_NO_REPLY;
				default:
					pri_message(pri, "!! Unknown Q.SIG diversion reason %d\n", redirectingreason);
					return PRI_REDIR_UNKNOWN;
			}
		default:
			switch(redirectingreason) {
				case Q952_DIVERT_REASON_UNKNOWN:
					return PRI_REDIR_UNKNOWN;
				case Q952_DIVERT_REASON_CFU:
					return PRI_REDIR_UNCONDITIONAL;
				case Q952_DIVERT_REASON_CFB:
					return PRI_REDIR_FORWARD_ON_BUSY;
				case Q952_DIVERT_REASON_CFNR:
					return PRI_REDIR_FORWARD_ON_NO_REPLY;
				case Q952_DIVERT_REASON_CD:
					return PRI_REDIR_DEFLECTION;
				case Q952_DIVERT_REASON_IMMEDIATE:
					pri_message(pri, "!! Dont' know how to convert Q.952 diversion reason IMMEDIATE to PRI analog\n");
					return PRI_REDIR_UNKNOWN;	/* ??? */
				default:
					pri_message(pri, "!! Unknown Q.952 diversion reason %d\n", redirectingreason);
					return PRI_REDIR_UNKNOWN;
			}
	}
}

int typeofnumber_from_q931(struct pri *pri, int ton)
{
	switch(ton) {
		case PRI_TON_INTERNATIONAL:
			return Q932_TON_INTERNATIONAL;
		case PRI_TON_NATIONAL:
			return Q932_TON_NATIONAL;
		case PRI_TON_NET_SPECIFIC:
			return Q932_TON_NET_SPECIFIC;
		case PRI_TON_SUBSCRIBER:
			return Q932_TON_SUBSCRIBER;
		case PRI_TON_ABBREVIATED:
			return Q932_TON_ABBREVIATED;
		case PRI_TON_RESERVED:
		default:
			pri_message(pri, "!! Unsupported Q.931 TypeOfNumber value (%d)\n", ton);
			/* fall through */
		case PRI_TON_UNKNOWN:
			return Q932_TON_UNKNOWN;
	}
}

static int typeofnumber_for_q931(struct pri *pri, int ton)
{
	switch (ton) {
		case Q932_TON_UNKNOWN:
			return PRI_TON_UNKNOWN;
		case Q932_TON_INTERNATIONAL:
			return PRI_TON_INTERNATIONAL;
		case Q932_TON_NATIONAL:
			return PRI_TON_NATIONAL;
		case Q932_TON_NET_SPECIFIC:
			return PRI_TON_NET_SPECIFIC;
		case Q932_TON_SUBSCRIBER:
			return PRI_TON_SUBSCRIBER;
		case Q932_TON_ABBREVIATED:
			return PRI_TON_ABBREVIATED;
		default:
			pri_message(pri, "!! Invalid Q.932 TypeOfNumber %d\n", ton);
			return PRI_TON_UNKNOWN;
	}
}

static int presentation_to_subscription(struct pri *pri, int presentation)
{
	/* derive subscription value from presentation value */

	switch (presentation & PRES_RESTRICTION) {
	case PRES_ALLOWED:
		return QSIG_NOTIFICATION_WITH_DIVERTED_TO_NR;
	case PRES_RESTRICTED:
		return QSIG_NOTIFICATION_WITHOUT_DIVERTED_TO_NR;
	case PRES_UNAVAILABLE:			/* Number not available due to interworking */
		return QSIG_NOTIFICATION_WITHOUT_DIVERTED_TO_NR;	/* ?? QSIG_NO_NOTIFICATION */
	default:
		pri_message(pri, "!! Unknown Q.SIG presentationIndicator 0x%02x\n", presentation);
		return QSIG_NOTIFICATION_WITHOUT_DIVERTED_TO_NR;
	}
}

int asn1_name_decode(void *data, int len, char *namebuf, int buflen)
{
	struct rose_component *comp = (struct rose_component*)data;
	int datalen = 0, res = 0;

	if (comp->len == ASN1_LEN_INDEF) {
		datalen = strlen((char *)comp->data);
		res = datalen + 2;
	} else
		datalen = res = comp->len;

	if (datalen > buflen - 1) {
		/* Truncate */
		datalen = buflen;
	}
	memcpy(namebuf, comp->data, datalen);
	namebuf[datalen] = '\0';

	return res + 2;
}

int asn1_string_encode(unsigned char asn1_type, void *data, int len, int max_len, void *src, int src_len)
{
	struct rose_component *comp = NULL;
	
	if (len < 2 + src_len)
		return -1;

	if (max_len && (src_len > max_len))
		src_len = max_len;

	comp = (struct rose_component *)data;
	comp->type = asn1_type;
	comp->len = src_len;
	memcpy(comp->data, src, src_len);
	
	return 2 + src_len;
}

int asn1_copy_string(char * buf, int buflen, struct rose_component *comp)
{
	int res;
	int datalen;

	if ((comp->len > buflen) && (comp->len != ASN1_LEN_INDEF))
		return -1;

	if (comp->len == ASN1_LEN_INDEF) {
		datalen = strlen((char*)comp->data);
		res = datalen + 2;
	} else
		res = datalen = comp->len;

	memcpy(buf, comp->data, datalen);
	buf[datalen] = 0;

	return res;
}

static int rose_namedata_decode(struct pri *pri, unsigned char *data, int len, int implicit, struct nameelements_namedata *value)
{
	int i = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;
	int res;

	do {
		/* NameData */
		if (!implicit) {
			GET_COMPONENT(comp, i, vdata, len);
			CHECK_COMPONENT(comp, ASN1_OCTETSTRING, "Don't know what to do if NameData is of type 0x%x\n");

			data = comp->data;
			if (comp->len == ASN1_LEN_INDEF) {
				len = strlen((char *)comp->data);
				res = len + 2 + 2;
			} else {
				len = comp->len;
				res = len + 2;
			}
		} else
			res = len;

		if (len > sizeof(value->name)-1) {
			pri_message(pri, "!! Oversized NameData component (%d)\n", len);
			return -1;
		}

		memcpy(value->name, data, len);
		value->name[len] = '\0';

		return res;
	}
	while(0);

	return -1;
}

static int rose_namedata_encode(struct pri *pri, unsigned char *dst, int implicit, char *name)
{
	int size = 0;
	struct rose_component *comp;
	int namesize;

	namesize = strlen(name);
	if (namesize > 50 ) {
		pri_message(pri, "!! Encoding of oversized NameData component failed (%d)\n", namesize);
		return -1;
	} else if (namesize == 0){
		pri_message(pri, "!! Encoding of undersized NameData component failed (%d)\n", namesize);
		return -1;
	}

	if (!implicit) {
		/* constructor component  (0x04,len) */
		comp = (struct rose_component *)dst;
		comp->type = ASN1_OCTETSTRING;
		comp->len = 2 + namesize;
		size += 2;
		dst += 2;
	}

	memcpy(dst, name, namesize);
	size += namesize;

	return size;
}

static int rose_nameset_decode(struct pri *pri, unsigned char *data, int len, int implicit, struct nameelements_nameset *value)
{
	int size;
	int i = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;
 	int characterset;

	value->characterset = CHARACTER_SET_ISO8859_1;

	do {
		/* NameSet */
		if (!implicit) {
			GET_COMPONENT(comp, i, vdata, len);
			CHECK_COMPONENT(comp, ASN1_SEQUENCE, "Don't know what to do if NameSet is of type 0x%x\n");
			SUB_COMPONENT(comp, i);
		}

		/* nameData NameData */
		GET_COMPONENT(comp, i, vdata, len);
		size = rose_namedata_decode(pri, (u_int8_t *)comp, len, 0, (struct nameelements_namedata *)value);
		if (size < 0)
			return -1;
		i += size;

		if (i < len) {
			/* characterSet CharacterSet OPTIONAL */
			GET_COMPONENT(comp, i, vdata, len);
			CHECK_COMPONENT(comp, ASN1_INTEGER, "Don't know what to do if CharacterSet is of type 0x%x\n");
			ASN1_GET_INTEGER(comp, characterset);
			NEXT_COMPONENT(comp, i);
			if (pri->debug & PRI_DEBUG_APDU)
				pri_message(pri, "     NameSet: Received characterSet=%s(%d)\n", characterset_to_str(characterset), characterset);
			value->characterset = characterset;
		}

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "     NameSet: '%s', characterSet=%s(%d) i=%d len=%d\n", value->name, characterset_to_str(value->characterset), value->characterset, i, len);

		return i;
	}
	while(0);
	
	return -1;
}

static int rose_name_decode(struct pri *pri, unsigned char *data, int len, struct nameelements_name *value)
{
	int i = 0;
	int size = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;

	value->name[0] = '\0';
	value->characterset = CHARACTER_SET_UNKNOWN;
	value->namepres = -1;

	do {
		GET_COMPONENT(comp, i, vdata, len);

		switch(comp->type) {
		case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0):			/* [0] namePresentationAllowedSimple */
			size = rose_namedata_decode(pri, comp->data, comp->len, 1, (struct nameelements_namedata *)value);
			if (size < 0)
				return -1;
			i += (size + 2);
			value->characterset = CHARACTER_SET_ISO8859_1;
			value->namepres = 1;
			break;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1):	/* [1] namePresentationAllowedExtended */
			size = rose_nameset_decode(pri, comp->data, comp->len, 1, (struct nameelements_nameset *)value);
			if (size < 0)
				return -1;
			i += (size + 2);
			value->namepres = 1;
			break;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2):			/* [2] namePresentationRestrictedSimple */
			size = rose_namedata_decode(pri, comp->data, comp->len, 1, (struct nameelements_namedata *)value);
			if (size < 0)
				return -1;
			i += (size + 2);
			value->characterset = CHARACTER_SET_ISO8859_1;
			value->namepres = 0;
			break;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_3):	/* [3] namePresentationRestrictedExtended */
			size = rose_nameset_decode(pri, comp->data, comp->len, 1, (struct nameelements_nameset *)value);
			if (size < 0)
				return -1;
			i += (size + 2);
			value->namepres = 0;
			break;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_4):			/* [4] nameNotAvailable */
		case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_7):			/* [7] namePresentationRestrictedNull */
			i += (comp->len + 2);
			value->name[0] = '\0';
			value->characterset = CHARACTER_SET_UNKNOWN;
			value->namepres = 0;
			break;
		default:
			pri_message(pri, "!! Unknown Name component received 0x%x\n", comp->type);
			return -1;
		}

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "     Name: '%s' i=%d len=%d\n", value->name, i, len);
		return i;
	}
	while (0);

	return -1;
}

static int rose_number_digits_decode(struct pri *pri, unsigned char *data, int len, int implicit, struct addressingdataelements_presentednumberunscreened *value)
{
	int i = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;
	int res = 0;

	do {
		if (!implicit) {
			GET_COMPONENT(comp, i, vdata, len);
			CHECK_COMPONENT(comp, ASN1_NUMERICSTRING, "Don't know what to do with NumberDigits ROSE component type 0x%x\n");

			data = comp->data;
			if (comp->len == ASN1_LEN_INDEF) {
				len = strlen((char *)comp->data);
				res = len + 2 + 2;
			} else {
				len = comp->len;
				res = len + 2;
			}
		} else
			res = len;

		if (len > sizeof(value->partyaddress)-1) {
			pri_message(pri, "!! Oversized NumberDigits component (%d)\n", len);
			return -1;
		}

		memcpy(value->partyaddress, data, len);
		value->partyaddress[len] = '\0';

		return res;
	}
	while(0);

	return -1;
}

static int rose_public_party_number_decode(struct pri *pri, unsigned char *data, int len, int implicit, struct addressingdataelements_presentednumberunscreened *value)
{
	int i = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;
	int ton;
	int res = 0;

	if (len < 2)
		return -1;

	do {
		if (!implicit) {
			GET_COMPONENT(comp, i, vdata, len);
			CHECK_COMPONENT(comp, ASN1_SEQUENCE, "Don't know what to do if PublicPartyNumber is of type 0x%x\n");
			SUB_COMPONENT(comp, i);
		}

		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, ASN1_ENUMERATED, "Don't know what to do with PublicPartyNumber ROSE component type 0x%x\n");
		ASN1_GET_INTEGER(comp, ton);
		NEXT_COMPONENT(comp, i);
		ton = typeofnumber_for_q931(pri, ton);

		res = rose_number_digits_decode(pri, &vdata[i], len-i, 0, value);
		if (res < 0)
			return -1;
		value->ton = ton;

		return res + 3;

	} while(0);
	return -1;
}

static int rose_public_party_number_encode(struct pri *pri, unsigned char *dst, int implicit, unsigned char ton, char *num)
{
	int i = 0, compsp = 0;
	struct rose_component *comp, *compstk[10];
	int numsize;

	numsize = strlen(num);
	if (numsize > 20 ) {
		pri_message(pri, "!! Encoding of oversized PublicPartyNumber component failed (%d)\n", numsize);
		return -1;
	}

	if (!implicit) {
		/* constructor component  (0x30,len) */
		ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), dst, i);
		ASN1_PUSH(compstk, compsp, comp);
	} else
		comp = (struct rose_component *)dst;

	/* publicTypeOfNumber   (0x0a,0x01,ton)*/
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, dst, i, ton);

	/* publicNumberDigits */

	/* tag component NumericString  (0x12,len) */
	ASN1_ADD_SIMPLE(comp, ASN1_NUMERICSTRING, dst, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* NumericString */
	memcpy(comp->data, num, numsize);
	i += numsize;

	ASN1_FIXUP(compstk, compsp, dst, i);

	if (!implicit)
		ASN1_FIXUP(compstk, compsp, dst, i);

	return i;
}

static int rose_private_party_number_decode(struct pri *pri, unsigned char *data, int len, int implicit, struct addressingdataelements_presentednumberunscreened *value)
{
	int i = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;
	int ton;
	int res = 0;

	if (len < 2)
		return -1;

	do {
		if (!implicit) {
			GET_COMPONENT(comp, i, vdata, len);
			CHECK_COMPONENT(comp, ASN1_SEQUENCE, "Don't know what to do if PrivatePartyNumber is of type 0x%x\n");
			SUB_COMPONENT(comp, i);
		}

		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, ASN1_ENUMERATED, "Don't know what to do with PrivatePartyNumber ROSE component type 0x%x\n");
		ASN1_GET_INTEGER(comp, ton);
		NEXT_COMPONENT(comp, i);
		ton = typeofnumber_for_q931(pri, ton);

		res = rose_number_digits_decode(pri, &vdata[i], len-i, 0, value);
		if (res < 0)
			return -1;
		value->ton = ton;

		return res + 3;

	} while(0);
	return -1;
}

static int rose_address_decode(struct pri *pri, unsigned char *data, int len, struct addressingdataelements_presentednumberunscreened *value)
{
	int i = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;
	int res = 0;

	do {
		GET_COMPONENT(comp, i, vdata, len);

		/* PartyNumber */
		switch(comp->type) {
		case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0):	/* [0] unknownPartyNumber, IMPLICIT NumberDigits */
			res = rose_number_digits_decode(pri, comp->data, comp->len, 1, value);
			if (res < 0)
				return -1;
			value->npi = PRI_NPI_UNKNOWN;
			value->ton = PRI_TON_UNKNOWN;
			break;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1):	/* [1] publicPartyNumber, IMPLICIT PublicPartyNumber */
			res = rose_public_party_number_decode(pri, comp->data, comp->len, 1, value);
			if (res < 0)
				return -1;
			value->npi = PRI_NPI_E163_E164;
			break;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_3):	/* [3] dataPartyNumber, IMPLICIT NumberDigits */
			res = rose_number_digits_decode(pri, comp->data, comp->len, 1, value);
			if (res < 0)
				return -1;
			value->npi = PRI_NPI_X121 /* ??? */;
			value->ton = PRI_TON_UNKNOWN /* ??? */;
			pri_message(pri, "!! dataPartyNumber isn't handled\n");
			return -1;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_4):	/* [4] telexPartyNumber, IMPLICIT NumberDigits */
			res = rose_number_digits_decode(pri, comp->data, comp->len, 1, value);
			if (res < 0)
				return -1;
			value->npi = PRI_NPI_F69 /* ??? */;
			value->ton = PRI_TON_UNKNOWN /* ??? */;
			pri_message(pri, "!! telexPartyNumber isn't handled\n");
			return -1;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_5):	/* [5] privatePartyNumber, IMPLICIT PrivatePartyNumber */
			res = rose_private_party_number_decode(pri, comp->data, comp->len, 1, value);
			if (res < 0)
				return -1;
			value->npi = PRI_NPI_PRIVATE;
			break;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_8):	/* [8] nationalStandardPartyNumber, IMPLICIT NumberDigits */
			res = rose_number_digits_decode(pri, comp->data, comp->len, 1, value);
			if (res < 0)
				return -1;
			value->npi = PRI_NPI_NATIONAL;
			value->ton = PRI_TON_NATIONAL;
			break;
		default:
			pri_message(pri, "!! Unknown PartyNumber component received 0x%X\n", comp->type);
			return -1;
		}
		ASN1_FIXUP_LEN(comp, res);
		NEXT_COMPONENT(comp, i);

		/* PartySubaddress OPTIONAL */
		if (i < len)
			pri_message(pri, "!! not all information is handled from Address component\n");
		return res + 2;
	}
	while (0);

	return -1;
}

static int rose_party_number_encode(struct pri *pri, unsigned char *dst, unsigned char ton, char *num)
{
	int i = 0, compsp = 0;
	struct rose_component *comp, *compstk[10];
	int numsize, size;

	numsize = strlen(num);
	if (numsize > 20 ) {
		pri_message(pri, "!! Encoding of oversized PartyNumber component failed (%d)\n", numsize);
		return -1;
	}

#if 0
	/* tag component unknownPartyNumber  (0x80,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), dst, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* unknownPartyNumber, implicid NumberDigits */
	memcpy(comp->data, num, numsize);
	i += numsize;

	ASN1_FIXUP(compstk, compsp, dst, i);
#endif

	/* tag component publicPartyNumber  (0xa1,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1), dst, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* publicPartyNumber, implicid PublicPartyNumber */
	size = rose_public_party_number_encode(pri, comp->data, 1, ton, num);
	if (size < 0)
		return -1;
	i += size;

	ASN1_FIXUP(compstk, compsp, dst, i);

	return i;
}

static int rose_party_number_decode(struct pri *pri, unsigned char *data, int len, struct addressingdataelements_presentednumberunscreened *value)
{
	int i = 0;
	int size = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;

	do {
		GET_COMPONENT(comp, i, vdata, len);

		switch(comp->type) {
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0):   /* [0] IMPLICIT NumberDigits -- default: unknownPartyNumber */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PartyNumber: UnknownPartyNumber len=%d\n", len);
				size = rose_number_digits_decode(pri, comp->data, comp->len, 1, value);
				if (size < 0)
					return -1;
				value->npi = PRI_NPI_UNKNOWN;
				value->ton = PRI_TON_UNKNOWN;
				break;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1):   /* [1] IMPLICIT PublicPartyNumber */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PartyNumber: PublicPartyNumber len=%d\n", len);
				size = rose_public_party_number_decode(pri, comp->data, comp->len, 1, value);
				if (size < 0)
					return -1;
				value->npi = PRI_NPI_E163_E164;
				break;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_3):   /* [3] IMPLICIT NumberDigits -- not used: dataPartyNumber */
				pri_message(pri, "!! PartyNumber: dataPartyNumber is reserved!\n");
				size = rose_number_digits_decode(pri, comp->data, comp->len, 1, value);
				if (size < 0)
					return -1;
				value->npi = PRI_NPI_X121 /* ??? */;
				value->ton = PRI_TON_UNKNOWN /* ??? */;
				break;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_4):   /* [4] IMPLICIT NumberDigits -- not used: telexPartyNumber */
				pri_message(pri, "!! PartyNumber: telexPartyNumber is reserved!\n");
				size = rose_number_digits_decode(pri, comp->data, comp->len, 1, value);
				if (size < 0)
					return -1;
				value->npi = PRI_NPI_F69 /* ??? */;
				value->ton = PRI_TON_UNKNOWN /* ??? */;
				break;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_5):   /* [5] IMPLICIT PrivatePartyNumber */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PartyNumber: PrivatePartyNumber len=%d\n", len);
				size = rose_private_party_number_decode(pri, comp->data, comp->len, 1, value);
				if (size < 0)
					return -1;
 				value->npi = PRI_NPI_PRIVATE;
				break;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_8):   /* [8] IMPLICIT NumberDigits -- not used: nationalStandatdPartyNumber */
				pri_message(pri, "!! PartyNumber: nationalStandardPartyNumber is reserved!\n");
				size = rose_number_digits_decode(pri, comp->data, comp->len, 1, value);
				if (size < 0)
					return -1;
				value->npi = PRI_NPI_NATIONAL;
				value->ton = PRI_TON_NATIONAL;
				break;

			default:
				pri_message(pri, "Invalid PartyNumber component 0x%X\n", comp->type);
				return -1;
		}
		ASN1_FIXUP_LEN(comp, size);
		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "     PartyNumber: '%s' size=%d len=%d\n", value->partyaddress, size, len);
		return size;
	}
	while (0);

	return -1;
}

static int rose_presented_number_unscreened_encode(struct pri *pri, unsigned char *dst, unsigned char presentation, unsigned char ton, char *num)
{
	int i = 0, compsp = 0;
	struct rose_component *comp, *compstk[10];
	int numsize, size;

	numsize = strlen(num);
	if (numsize > 20 ) {
		pri_message(pri, "!! Encoding of oversized PresentedNumberUnscreened component failed (%d)\n", numsize);
		return -1;
	}

	switch (presentation & PRES_RESTRICTION) {
		case PRES_ALLOWED:
			/* tag component [0] presentationAllowedAddress (0xa0,len) */
			ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_0), dst, i);
			ASN1_PUSH(compstk, compsp, comp);

			/* PartyNumber */
			size = rose_party_number_encode(pri, comp->data, ton, num);
			if (size < 0)
				return -1;
			i += size;
			ASN1_FIXUP(compstk, compsp, dst, i);
			break;
		case PRES_RESTRICTED:
			/* tag component [1] presentationRestricted (0x81,len) */
			ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_1), dst, i);
			break;
		case PRES_UNAVAILABLE:
			/* tag component [2] numberNotAvailableDueToInterworking (0x82,len) */
			ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), dst, i);
			ASN1_FIXUP(compstk, compsp, dst, i);
			break;
		default:
			pri_message(pri, "!! Undefined presentation value for PresentedNumberUnscreened: 0x%x\n", presentation);
			return -1;
	}

	return i;
}

static int rose_presented_number_unscreened_decode(struct pri *pri, unsigned char *data, int len, struct addressingdataelements_presentednumberunscreened *value)
{
	int i = 0;
	int size = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;

	/* Fill in default values */
	value->partyaddress[0] = '\0';
	value->ton = PRI_TON_UNKNOWN;
	value->npi = PRI_NPI_E163_E164;
	value->pres = -1;	/* Data is not available */

	do {
		GET_COMPONENT(comp, i, vdata, len);

		switch(comp->type) {
		case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_0):		/* [0] presentationAllowedNumber */
			value->pres = PRES_ALLOWED_USER_NUMBER_NOT_SCREENED;
			size = rose_address_decode(pri, comp->data, comp->len, value);
			ASN1_FIXUP_LEN(comp, size);
			return size + 2;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_1):		/* [1] IMPLICIT presentationRestricted */
			if (comp->len != 0) { /* must be NULL */
				pri_error(pri, "!! Invalid PresentationRestricted component received (len != 0)\n");
				return -1;
			}
			value->pres = PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
			return 2;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2):		/* [2] IMPLICIT numberNotAvailableDueToInterworking */
			if (comp->len != 0) { /* must be NULL */
				pri_error(pri, "!! Invalid NumberNotAvailableDueToInterworking component received (len != 0)\n");
				return -1;
			}
			value->pres = PRES_NUMBER_NOT_AVAILABLE;
			return 2;
		case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_3):		/* [3] presentationRestrictedNumber */
			value->pres = PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
			size = rose_address_decode(pri, comp->data, comp->len, value) + 2;
			ASN1_FIXUP_LEN(comp, size);
			return size + 2;
		default:
			pri_message(pri, "Invalid PresentedNumberUnscreened component 0x%X\n", comp->type);
		}
		return -1;
	}
	while (0);

	return -1;
}

static int rose_number_screened_encode(struct pri *pri, unsigned char *dst, int implicit, unsigned char ton, unsigned char screenind, char *num)
{
	int i = 0, compsp = 0;
	struct rose_component *comp, *compstk[10];
	int numsize, size;

	numsize = strlen(num);
	if (numsize > 20 ) {
		pri_message(pri, "!! Encoding of oversized NumberScreened component failed (%d)\n", numsize);
		return -1;
	}

	if (!implicit) {
		/* constructor component  (0x30,len) */
		ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), dst, i);
		ASN1_PUSH(compstk, compsp, comp);
	} else
		comp = (struct rose_component *)dst;

	/* PartyNumber */
	size = rose_party_number_encode(pri, (u_int8_t *)comp, ton, num);
	if (size < 0)
		return -1;
	i += size;

	/* ScreeningIndicator  (0x0a,0x01,screenind) */
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, dst, i, screenind);

	if (!implicit)
		ASN1_FIXUP(compstk, compsp, dst, i);

	return i;
}

static int rose_number_screened_decode(struct pri *pri, unsigned char *data, int len, struct addressingdataelements_presentednumberscreened *value)
{
	int i = 0;
	int size = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;

	int scrind = -1;

	do {
		/* Party Number */
		GET_COMPONENT(comp, i, vdata, len);
		size = rose_party_number_decode(pri, (u_int8_t *)comp, comp->len + 2, (struct addressingdataelements_presentednumberunscreened*) value);
		if (size < 0)
			return -1;
		comp->len = size;
		NEXT_COMPONENT(comp, i);

		/* Screening Indicator */
		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, ASN1_ENUMERATED, "Don't know what to do with NumberScreened ROSE component type 0x%x\n");
		ASN1_GET_INTEGER(comp, scrind);
		// Todo: scrind = screeningindicator_for_q931(pri, scrind);
		NEXT_COMPONENT(comp, i);

		value->scrind = scrind;

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "     NumberScreened: '%s' ScreeningIndicator=%d  i=%d  len=%d\n", value->partyaddress, scrind, i, len);

		return i-2;  // We do not have a sequence header here.
	}
	while (0);

	return -1;
}

static int rose_presented_number_screened_decode(struct pri *pri, unsigned char *data, int len, struct addressingdataelements_presentednumberscreened *value)
{
	int i = 0;
	int size = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;

	/* Fill in default values */
	value->partyaddress[0] = '\0';
	value->ton = PRI_TON_UNKNOWN;
	value->npi = PRI_NPI_UNKNOWN;
	value->pres = -1; /* Data is not available */
	value->scrind = 0;

	do {
		GET_COMPONENT(comp, i, vdata, len);

		switch(comp->type) {
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_0):   /* [0] IMPLICIT presentationAllowedNumber */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PresentedNumberScreened: presentationAllowedNumber comp->len=%d\n", comp->len);
				value->pres = PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN;
				size = rose_number_screened_decode(pri, comp->data, comp->len, value);
				if (size < 0)
					return -1;
				ASN1_FIXUP_LEN(comp, size);
				return size + 2;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_1):    /* [1] IMPLICIT presentationRestricted */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PresentedNumberScreened: presentationRestricted comp->len=%d\n", comp->len);
				if (comp->len != 0) { /* must be NULL */
					pri_error(pri, "!! Invalid PresentationRestricted component received (len != 0)\n");
					return -1;
				}
				value->pres = PRES_PROHIB_USER_NUMBER_PASSED_SCREEN;
				return 2;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2):    /* [2] IMPLICIT numberNotAvailableDueToInterworking */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PresentedNumberScreened: NumberNotAvailableDueToInterworking comp->len=%d\n", comp->len);
				if (comp->len != 0) { /* must be NULL */
					pri_error(pri, "!! Invalid NumberNotAvailableDueToInterworking component received (len != 0)\n");
					return -1;
				}
				value->pres = PRES_NUMBER_NOT_AVAILABLE;
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PresentedNumberScreened: numberNotAvailableDueToInterworking Type=0x%X  i=%d len=%d size=%d\n", comp->type, i, len);
				return 2;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_3):    /* [3] IMPLICIT presentationRestrictedNumber */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PresentedNumberScreened: presentationRestrictedNumber comp->len=%d\n", comp->len);
				value->pres = PRES_PROHIB_USER_NUMBER_PASSED_SCREEN;
				size = rose_number_screened_decode(pri, comp->data, comp->len, value);
				if (size < 0)
					return -1;
				ASN1_FIXUP_LEN(comp, size);
				return size + 2;

			default:
				pri_message(pri, "Invalid PresentedNumberScreened component 0x%X\n", comp->type);
		}
		return -1;
	}
	while (0);

	return -1;
}

static int rose_partysubaddress_decode(struct pri *pri, unsigned char *data, int len, struct addressingdataelements_partysubaddress *value)
{
	int i = 0;
	int size = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;

	int odd_count_indicator = -1;
	value->partysubaddress[0] = '\0';

	do {
		GET_COMPONENT(comp, i, vdata, len);

		switch(comp->type) {
		case (ASN1_CONSTRUCTOR | ASN1_SEQUENCE):    /* UserSpecifiedSubaddress */
			/* SubaddressInformation */
			SUB_COMPONENT(comp, i);
			GET_COMPONENT(comp, i, vdata, len);
			CHECK_COMPONENT(comp, ASN1_OCTETSTRING, "Don't know what to do if SubaddressInformation is of type 0x%x\n");
			size = asn1_name_decode(comp->data, comp->len, value->partysubaddress, sizeof(value->partysubaddress));
			if (size < 0)
				return -1;
			i += size;

			/* oddCountIndicator BOOLEAN OPTIONAL */
			if (i < len) {
				GET_COMPONENT(comp, i, vdata, len);
				CHECK_COMPONENT(comp, ASN1_BOOLEAN, "Don't know what to do if SubaddressInformation is of type 0x%x\n");

				ASN1_GET_INTEGER(comp, odd_count_indicator);
				NEXT_COMPONENT(comp, i);
			}
		case (ASN1_OCTETSTRING):    /* NSAPSubaddress */
			size = asn1_name_decode((u_int8_t *)comp, comp->len + 2, value->partysubaddress, sizeof(value->partysubaddress));
			if (size < 0)
				return -1;
			i += size;
			break;
		default:
			pri_message(pri, "Invalid PartySubaddress component 0x%X\n", comp->type);
			return -1;
		}

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "     PartySubaddress: '%s', oddCountIndicator=%d, i=%d len=%d\n", value->partysubaddress, odd_count_indicator, i, len);

		return i;
	}
	while (0);

	return -1;
}

static int rose_address_screened_decode(struct pri *pri, unsigned char *data, int len, struct addressingdataelements_addressscreened *value)
{
	int i = 0;
	int size = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;

	int scrind;
	value->partysubaddress[0] = '\0';

	/* SEQUENCE AddressScreened */
	do {
		/* PartyNumber */
		GET_COMPONENT(comp, i, vdata, len);
		size = rose_party_number_decode(pri, (u_int8_t *)comp, comp->len + 2, (struct addressingdataelements_presentednumberunscreened *)value);
		if (size < 0)
			return -1;
		comp->len = size;
		NEXT_COMPONENT(comp, i);

		/* ScreeningIndicator */
		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, ASN1_ENUMERATED, "Don't know what to do with AddressScreened ROSE component type 0x%x\n");
		ASN1_GET_INTEGER(comp, scrind);
		NEXT_COMPONENT(comp, i);

		if (i < len) {
			/* PartySubaddress OPTIONAL */
			GET_COMPONENT(comp, i, vdata, len);
			size = rose_partysubaddress_decode(pri, (u_int8_t *)comp, comp->len + 2, (struct addressingdataelements_partysubaddress *)value);
			if (size < 0)
				return -1;
			i += size;
		}

		value->scrind = scrind;

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "     AddressScreened: '%s' ScreeningIndicator=%d  i=%d  len=%d\n", value->partyaddress, scrind, i, len);

		return i-2;
	}
	while (0);

	return -1;
}

static int rose_presented_address_screened_decode(struct pri *pri, unsigned char *data, int len, struct addressingdataelements_presentedaddressscreened *value)
{
	int i = 0;
	int size = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = data;

	/* Fill in default values */
	value->partyaddress[0] = '\0';
	value->partysubaddress[0] = '\0';
	value->npi = PRI_NPI_UNKNOWN;
	value->ton = PRI_TON_UNKNOWN;
	value->pres = -1; /* Data is not available */
	value->scrind = 0;

	do {
		GET_COMPONENT(comp, i, vdata, len);

		switch(comp->type) {
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_0):   /* [0] IMPLICIT presentationAllowedAddress */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PresentedAddressScreened: presentationAllowedAddress comp->len=%d\n", comp->len);
				value->pres = PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN;
				size = rose_address_screened_decode(pri, comp->data, comp->len, (struct addressingdataelements_addressscreened *)value);
				if (size < 0)
					return -1;
				ASN1_FIXUP_LEN(comp, size);
				return size + 2;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_1):    /* [1] IMPLICIT presentationRestricted */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PresentedAddressScreened: presentationRestricted comp->len=%d\n", comp->len);
				if (comp->len != 0) { /* must be NULL */
					pri_error(pri, "!! Invalid PresentationRestricted component received (len != 0)\n");
					return -1;
				}
				value->pres = PRES_PROHIB_USER_NUMBER_PASSED_SCREEN;
				return 2;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2):    /* [2] IMPLICIT numberNotAvailableDueToInterworking */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PresentedAddressScreened: NumberNotAvailableDueToInterworking comp->len=%d\n", comp->len);
				if (comp->len != 0) { /* must be NULL */
					pri_error(pri, "!! Invalid NumberNotAvailableDueToInterworking component received (len != 0)\n");
					return -1;
				}
				value->pres = PRES_NUMBER_NOT_AVAILABLE;
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PresentedAddressScreened: numberNotAvailableDueToInterworking Type=0x%X  i=%d len=%d size=%d\n", comp->type, i, len);
				return 2;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_3):    /* [3] IMPLICIT presentationRestrictedAddress */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     PresentedAddressScreened: presentationRestrictedAddress comp->len=%d\n", comp->len);
				value->pres = PRES_PROHIB_USER_NUMBER_PASSED_SCREEN;
				size = rose_address_screened_decode(pri, comp->data, comp->len, (struct addressingdataelements_addressscreened *)value);
				if (size < 0)
					return -1;
				ASN1_FIXUP_LEN(comp, size);
				return size + 2;

			default:
				pri_message(pri, "Invalid PresentedAddressScreened component 0x%X\n", comp->type);
		}
		return -1;
	}
	while (0);

	return -1;
}

static int rose_diverting_leg_information1_decode(struct pri *pri, q931_call *call, struct rose_component *sequence, int len)
{
	int i = 0;
	struct addressingdataelements_presentednumberunscreened nominatednr;
	int diversion_reason;
	int subscription_option;
	struct rose_component *comp = NULL;
	unsigned char *vdata = sequence->data;
	int size = 0;
	memset(&nominatednr, 0, sizeof(nominatednr));

	/* Data checks */
	if (sequence->type != (ASN1_CONSTRUCTOR | ASN1_SEQUENCE)) { /* Constructed Sequence */
		pri_message(pri, "Invalid DivertingLegInformation1Type argument\n");
		return -1;
	}

	if (sequence->len == ASN1_LEN_INDEF) {
		len -= 4;   /* For the 2 extra characters at the end
		               and two characters of header */
	} else
		len -= 2;

	do {
		/* diversionReason DiversionReason */
		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, ASN1_ENUMERATED, "Invalid diversionReason type 0x%X of ROSE divertingLegInformation1 component received\n");
		ASN1_GET_INTEGER(comp, diversion_reason);
		NEXT_COMPONENT(comp, i);

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "    Received diversionReason: %s(%d)\n", diversionreason_to_str(pri, diversion_reason), diversion_reason);

		diversion_reason = redirectingreason_for_q931(pri, diversion_reason);

		/* subscriptionOption SubscriptionOption */
		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, ASN1_ENUMERATED, "Invalid subscriptionOption type 0x%X of ROSE divertingLegInformation1 component received\n");
		ASN1_GET_INTEGER(comp, subscription_option);
		NEXT_COMPONENT(comp, i);

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "    Received subscriptionOption: %d\n", subscription_option);

		/* nominatedNr PartyNumber */
		GET_COMPONENT(comp, i, vdata, len);
		size = rose_party_number_decode(pri, (u_int8_t *)comp, comp->len + 2, &nominatednr);
		if (size < 0)
			return -1;

		if (pri->debug & PRI_DEBUG_APDU) {
			pri_message(pri, "    Received nominatedNr '%s'\n", nominatednr.partyaddress);
			pri_message(pri, "      ton = %d, npi = %d\n\n", nominatednr.ton, nominatednr.npi);
		}

		call->divleginfo1activeflag = 1;
		if (subscription_option == QSIG_NOTIFICATION_WITH_DIVERTED_TO_NR) {
			libpri_copy_string(call->divertedtonum, nominatednr.partyaddress, sizeof(call->divertedtonum));
		} else {
			call->divertedtonum[0] = '\0';
		}
		call->divertedtopres = (subscription_option == QSIG_NOTIFICATION_WITH_DIVERTED_TO_NR) ? PRES_ALLOWED_USER_NUMBER_NOT_SCREENED : PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
		call->divertedtoplan = ((nominatednr.ton & 0x07) << 4) | (nominatednr.npi & 0x0f);
		call->divertedtoreason = diversion_reason;
		call->divertedtocount++;

		return 0;
	}
	while (0);

	return -1;
}

int rose_diverting_leg_information1_encode(struct pri *pri, q931_call *call)
{
	int i = 0, compsp = 0;
	struct rose_component *comp, *compstk[10];
	unsigned char buffer[256];
	int size;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "    Encode divertingLegInformation1\n");

	/* Protocol Profile = 0x1f (Networking Extensions)  (0x9f) */
	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);

	/* Network Facility Extension */
	if (pri->switchtype == PRI_SWITCH_QSIG) {
		/* tag component NetworkFacilityExtension (0xaa, len ) */
		ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i);
		ASN1_PUSH(compstk, compsp, comp);

		/* sourceEntity  (0x80,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);	/* endPINX(0) */

		/* destinationEntity  (0x82,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);	/* endPINX(0) */
		ASN1_FIXUP(compstk, compsp, buffer, i);
	}

	/* Network Protocol Profile */
	/*  - not included - */

	/* Interpretation APDU  (0x8b,0x01,0x00) */
	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 0);	/* discardAnyUnrecognisedInvokePdu(0) */

	/* Service APDU(s): */

	/* ROSE InvokePDU  (0xa1,len) */
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* ROSE InvokeID  (0x02,0x01,invokeid) */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));

	/* ROSE operationId  (0x02,0x01,0x14)*/
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, ROSE_DIVERTING_LEG_INFORMATION1);

	/* constructor component  (0x30,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* diversionReason  (0x0a,0x01,diversionreason) */
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, redirectingreason_from_q931(pri, call->divertedtoreason));

	/* subscriptionOption  (0x0a,0x01,subscriptionoption) */
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, presentation_to_subscription(pri, call->divertedtopres));

	/* nominatedNr */

	/* tag component publicPartyNumber  (0xa1,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* publicPartyNumber, implicid PublicPartyNumber */
	size = rose_public_party_number_encode(pri, &buffer[i], 1, (call->divertedtoplan & 0x70) >> 4, call->divertedtonum);
	if (size < 0)
		return -1;
	i += size;

	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (pri_call_apdu_queue(call, Q931_FACILITY, buffer, i, NULL, NULL))
		return -1;

	return 0;
}

static int rose_diverting_leg_information2_decode(struct pri *pri, q931_call *call, struct rose_component *sequence, int len)
{
	int i = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = sequence->data;
	int size = 0;

	int diversion_counter;
	int diversion_reason;
	int original_diversion_reason = QSIG_DIVERT_REASON_UNKNOWN;
	struct nameelements_name redirectingname = { "", CHARACTER_SET_UNKNOWN, 0 };
	struct nameelements_name origcalledname = { "", CHARACTER_SET_UNKNOWN, 0 };;
	struct addressingdataelements_presentednumberunscreened divertingnr;
	struct addressingdataelements_presentednumberunscreened originalcallednr;
	memset(&divertingnr, 0, sizeof(divertingnr));
	memset(&originalcallednr, 0, sizeof(originalcallednr));

	/* Data checks */
	if (sequence->type != (ASN1_CONSTRUCTOR | ASN1_SEQUENCE)) { /* Constructed Sequence */
		pri_message(pri, "Invalid DivertingLegInformation2Type argument\n");
		return -1;
	}

	if (sequence->len == ASN1_LEN_INDEF) {
		len -= 4;   /* For the 2 extra characters at the end
		               and two characters of header */
	} else
		len -= 2;

	do {
		/* diversionCounter */
		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, ASN1_INTEGER, "Don't know what to do if diversionCounter is of type 0x%x\n");
		ASN1_GET_INTEGER(comp, diversion_counter);
		NEXT_COMPONENT(comp, i);

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "    Received diversionCounter: %d\n",  diversion_counter);

		/* diversionReason DiversionReason */
		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, ASN1_ENUMERATED, "Invalid diversionReason type 0x%X of ROSE divertingLegInformation2 component received\n");
		ASN1_GET_INTEGER(comp, diversion_reason);
		NEXT_COMPONENT(comp, i);

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "    Received diversionReason: %s(%d)\n", diversionreason_to_str(pri, diversion_reason), diversion_reason);

		diversion_reason = redirectingreason_for_q931(pri, diversion_reason);

		/* Type SEQUENCE specifies an ordered list of component types.           *
		 * We decode all components but for simplicity we don't check the order. */
		while (i < len) {
			GET_COMPONENT(comp, i, vdata, len);

			switch(comp->type) {
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0):
				/* originalDiversionReason */
				ASN1_GET_INTEGER(comp, original_diversion_reason);
				NEXT_COMPONENT(comp,i);
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "    Received originalDiversionReason: %s(%d)\n", diversionreason_to_str(pri, original_diversion_reason), original_diversion_reason);
				original_diversion_reason = redirectingreason_for_q931(pri, original_diversion_reason);
				break;
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1):
				/* divertingNr */
				size = rose_presented_number_unscreened_decode(pri, comp->data, comp->len, &divertingnr);
				if (size < 0)
					return -1;
				ASN1_FIXUP_LEN(comp, size);
				comp->len = size;
				NEXT_COMPONENT(comp,i);
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "    Received divertingNr '%s'\n", divertingnr.partyaddress);
					pri_message(pri, "      ton = %d, pres = %d, npi = %d\n", divertingnr.ton, divertingnr.pres, divertingnr.npi);
				}
				break;
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_2):
				/* originalCalledNr */
				size = rose_presented_number_unscreened_decode(pri, comp->data, comp->len, &originalcallednr);
				if (size < 0)
					return -1;
				ASN1_FIXUP_LEN(comp, size);
				comp->len = size;
				NEXT_COMPONENT(comp,i);
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "    Received originalCalledNr '%s'\n", originalcallednr.partyaddress);
					pri_message(pri, "      ton = %d, pres = %d, npi = %d\n", originalcallednr.ton, originalcallednr.pres, originalcallednr.npi);
				}
				break;
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_3):
				/* redirectingName */
				size = rose_name_decode(pri, comp->data, comp->len, &redirectingname);
				if (size < 0)
					return -1;
				i += (size + 2);
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     Received RedirectingName '%s', namepres %s(%d), characterset %s(%d)\n",
						        redirectingname.name, namepres_to_str(redirectingname.namepres), redirectingname.namepres,
						        characterset_to_str(redirectingname.characterset), redirectingname.characterset);
				break;
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_4):
				/* originalCalledName */
				size = rose_name_decode(pri, comp->data, comp->len, &origcalledname);
				if (size < 0)
					return -1;
				i += (size + 2);
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     Received OriginalCalledName '%s', namepres %s(%d), characterset %s(%d)\n",
						        origcalledname.name, namepres_to_str(origcalledname.namepres), origcalledname.namepres,
						        characterset_to_str(origcalledname.characterset), origcalledname.characterset);
				break;
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_5):	/* [5] IMPLICIT Extension */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_6):	/* [6] IMPLICIT SEQUENCE OF Extension */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "!!     Ignoring DivertingLegInformation2 component 0x%X\n", comp->type);
				NEXT_COMPONENT(comp, i);
				break;
			default:
				pri_message(pri, "!!     Invalid DivertingLegInformation2 component received 0x%X\n", comp->type);
				return -1;
			}
		}

		if (divertingnr.pres >= 0) {
			call->redirectingplan = ((divertingnr.ton & 0x07) << 4) | (divertingnr.npi & 0x0f);
			call->redirectingpres = divertingnr.pres;
			call->redirectingreason = diversion_reason;
			libpri_copy_string(call->redirectingnum, divertingnr.partyaddress, sizeof(call->redirectingnum));
		}
		if (originalcallednr.pres >= 0) {
			call->origcalledplan = ((originalcallednr.ton & 0x07) << 4) | (originalcallednr.npi & 0x0f);
			call->origcalledpres = originalcallednr.pres;
			libpri_copy_string(call->origcallednum, originalcallednr.partyaddress, sizeof(call->origcallednum));
		}

		if (redirectingname.namepres != 0) {
			libpri_copy_string(call->redirectingname, redirectingname.name, sizeof(call->redirectingname));
		} else {
			call->redirectingname[0] = '\0';
		}

		if (origcalledname.namepres != 0) {
			libpri_copy_string(call->origcalledname, origcalledname.name, sizeof(call->origcalledname));
		} else {
			call->origcalledname[0] = '\0';
		}

		call->origredirectingreason = original_diversion_reason;
		call->redirectingcount = diversion_counter;

		return 0;
	}
	while (0);

	return -1;
}

static int rose_diverting_leg_information2_encode(struct pri *pri, q931_call *call)
{
	int i = 0, compsp = 0;
	struct rose_component *comp, *compstk[10];
	unsigned char buffer[256];
	int size;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "    Encode divertingLegInformation2\n");

	/* Protocol Profile = 0x1f (Networking Extensions)  (0x9f) */
	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);

	/* Network Facility Extension */
	if (pri->switchtype == PRI_SWITCH_QSIG) {
		/* tag component NetworkFacilityExtension (0xaa, len ) */
		ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i);
		ASN1_PUSH(compstk, compsp, comp);

		/* sourceEntity  (0x80,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);	/* endPINX(0) */

		/* destinationEntity  (0x82,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);	/* endPINX(0) */
		ASN1_FIXUP(compstk, compsp, buffer, i);
	}

	/* Network Protocol Profile */
	/*  - not included - */

	/* Interpretation APDU  (0x8b,0x01,0x00) */
	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 0);	/* discardAnyUnrecognisedInvokePdu(0) */

	/* Service APDU(s): */

	/* ROSE InvokePDU  (0xa1,len) */
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* ROSE InvokeID  (0x02,0x01,invokeid) */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));

	/* ROSE operationId  (0x02,0x01,0x15)*/
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, ROSE_DIVERTING_LEG_INFORMATION2);

	/* constructor component  (0x30,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* diversionCounter always is 1 because other isn't available in the current design */
	/* diversionCounter  (0x02,0x01,0x01) */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, 1);

	/* diversionReason  (0x0a,0x01,redirectingreason) */
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, redirectingreason_from_q931(pri, call->redirectingreason));

	/* originalDiversionReason */
	/*  - not included - */

	/* divertingNr */

	/* tag component divertingNr  (0xa1,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	size = rose_presented_number_unscreened_encode(pri, &buffer[i], call->redirectingpres, typeofnumber_from_q931(pri, (call->redirectingplan & 0x70) >> 4), call->redirectingnum);
	if (size < 0)
		return -1;
	i += size;
	ASN1_FIXUP(compstk, compsp, buffer, i);

	/* originalCalledNr */
	/*  - not included - */

#if 0
	/* The originalCalledNr is unknown here. Its the same as divertingNr if the call *
	 * is diverted only once but we don't know if its diverted one ore more times.   */

	/* originalCalledNr */

	/* tag component originalCalledNr  (0xa2,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_2), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	size = rose_presented_number_unscreened_encode(pri, &buffer[i], call->redirectingpres, typeofnumber_from_q931(pri, (call->redirectingplan & 0x70) >> 4), call->redirectingnum);
	if (size < 0)
		return -1;
	i += size;
	ASN1_FIXUP(compstk, compsp, buffer, i);
#endif

	/* redirectingName */
	if (call->redirectingname[0]) {
		/* tag component redirectingName  (0xa3,len) */
		ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_3), buffer, i);
		ASN1_PUSH(compstk, compsp, comp);

		/* tag component namePresentationAllowedSimple  (0x80,len) */
		ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i);
		ASN1_PUSH(compstk, compsp, comp);

		/* namePresentationAllowedSimple, implicid NameData */
		size = rose_namedata_encode(pri, &buffer[i], 1, call->redirectingname);
		if (size < 0)
			return -1;
		i += size;

		ASN1_FIXUP(compstk, compsp, buffer, i);
		ASN1_FIXUP(compstk, compsp, buffer, i);
	}

	/* originalCalledName */
	/*  - not included - */

	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (pri_call_apdu_queue(call, Q931_SETUP, buffer, i, NULL, NULL))
		return -1;

	return 0;
}

static int rose_diverting_leg_information3_decode(struct pri *pri, q931_call *call, struct rose_component *sequence, int len)
{
	int i = 0;
	struct nameelements_name redirectionname = { "", CHARACTER_SET_UNKNOWN, 0 };
	int presentation_allowed_indicator;
	struct rose_component *comp = NULL;
	unsigned char *vdata = sequence->data;
	int size = 0;

	/* Data checks */
	if (sequence->type != (ASN1_CONSTRUCTOR | ASN1_SEQUENCE)) { /* Constructed Sequence */
		pri_message(pri, "Invalid DivertingLegInformation3Type argument\n");
		return -1;
	}

	if (sequence->len == ASN1_LEN_INDEF) {
		len -= 4;   /* For the 2 extra characters at the end
		               and two characters of header */
	} else
		len -= 2;

	do {
		/* presentationAllowedIndicator */
		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, ASN1_BOOLEAN, "Don't know what to do if presentationAllowedIndicator is of type 0x%x\n");
		ASN1_GET_INTEGER(comp, presentation_allowed_indicator);
		NEXT_COMPONENT(comp, i);

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "    Received presentationAllowedIndicator: %d\n", presentation_allowed_indicator);

		/* Type SEQUENCE specifies an ordered list of component types.           *
		 * We decode all components but for simplicity we don't check the order. */
		while (i < len) {
			GET_COMPONENT(comp, i, vdata, len);

			switch(comp->type) {
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_0):
				/* redirectionName */
				size = rose_name_decode(pri, comp->data, comp->len, &redirectionname);
				if (size < 0)
					return -1;
				i += (size + 2);
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     Received RedirectionName '%s', namepres %s(%d), characterset %s(%d)\n",
					            redirectionname.name, namepres_to_str(redirectionname.namepres), redirectionname.namepres,
					            characterset_to_str(redirectionname.characterset), redirectionname.characterset);
				break;
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1):	/* [1] IMPLICIT Extension */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_2):	/* [2] IMPLICIT SEQUENCE OF Extension */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "!!     Ignoring DivertingLegInformation3 component 0x%X\n", comp->type);
				NEXT_COMPONENT(comp, i);
				break;
			default:
				pri_message(pri, "!! Invalid DivertingLegInformation3 component received 0x%X\n", comp->type);
				return -1;
			}
		}

		call->divleginfo3activeflag = 1;
		if ((redirectionname.namepres != 0) && (presentation_allowed_indicator != 0)) {
			libpri_copy_string(call->divertedtoname, redirectionname.name, sizeof(call->divertedtoname));
		} else {
			call->divertedtoname[0] = '\0';
		}

		return 0;
	}
	while (0);

	return -1;
}

int rose_diverting_leg_information3_encode(struct pri *pri, q931_call *call, int messagetype)
{
	int i = 0, compsp = 0;
	struct rose_component *comp, *compstk[10];
	unsigned char buffer[256];
	int size;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "    Encode divertingLegInformation3\n");

	/* Protocol Profile = 0x1f (Networking Extensions)  (0x9f) */
	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);

	/* Network Facility Extension */
	if (pri->switchtype == PRI_SWITCH_QSIG) {
		/* tag component NetworkFacilityExtension (0xaa, len ) */
		ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i);
		ASN1_PUSH(compstk, compsp, comp);

		/* sourceEntity  (0x80,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);	/* endPINX(0) */

		/* destinationEntity  (0x82,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);	/* endPINX(0) */
		ASN1_FIXUP(compstk, compsp, buffer, i);
	}

	/* Network Protocol Profile */
	/*  - not included - */

	/* Interpretation APDU  (0x8b,0x01,0x00) */
	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 0);	/* discardAnyUnrecognisedInvokePdu(0) */

	/* Service APDU(s): */

	/* ROSE InvokePDU  (0xa1,len) */
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* ROSE InvokeID  (0x02,0x01,invokeid) */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));

	/* ROSE operationId  (0x02,0x01,0x16)*/
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, ROSE_DIVERTING_LEG_INFORMATION3);

	/* constructor component  (0x30,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* 'connectedpres' also indicates if name presentation is allowed */
	if (((call->divertedtopres & 0x60) >> 5) == 0) {
		/* presentation allowed */

		/* presentationAllowedIndicator  (0x01,0x01,0xff) */
		ASN1_ADD_BYTECOMP(comp, ASN1_BOOLEAN, buffer, i, 0xff);			/* true(255) */

		/* redirectionName */

		/* tag component redirectionName  (0xa0,len) */
		ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_0), buffer, i);
		ASN1_PUSH(compstk, compsp, comp);

		if (call->divertedtoname[0]) {
			/* tag component namePresentationAllowedSimple  (0x80,len) */
			ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i);
			ASN1_PUSH(compstk, compsp, comp);

			/* namePresentationAllowedSimple, implicid NameData */
			size = rose_namedata_encode(pri, &buffer[i], 1, call->divertedtoname);
			if (size < 0)
				return -1;
			i += size;

			ASN1_FIXUP(compstk, compsp, buffer, i);
		}

		ASN1_FIXUP(compstk, compsp, buffer, i);
	} else {
		/* presentation restricted */

		/* presentationAllowedIndicator  (0x01,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, ASN1_BOOLEAN, buffer, i, 0);			/* false(0) */

		/* - don't include redirectionName, component is optional - */
	}

	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (pri_call_apdu_queue(call, messagetype, buffer, i, NULL, NULL))
		return -1;

	return 0;
}

/* Send the rltThirdParty: Invoke */
int rlt_initiate_transfer(struct pri *pri, q931_call *c1, q931_call *c2)
{
	int i = 0;
	unsigned char buffer[256];
	struct rose_component *comp = NULL, *compstk[10];
	const unsigned char rlt_3rd_pty = RLT_THIRD_PARTY;
	q931_call *callwithid = NULL, *apdubearer = NULL;
	int compsp = 0;

	if (c2->transferable) {
		apdubearer = c1;
		callwithid = c2;
	} else if (c1->transferable) {
		apdubearer = c2;
		callwithid = c1;
	} else
		return -1;

	buffer[i++] = (Q932_PROTOCOL_ROSE);
	buffer[i++] = (0x80 | RLT_SERVICE_ID); /* Service Identifier octet */

	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* Invoke ID is set to the operation ID */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, rlt_3rd_pty);

	/* Operation Tag */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, rlt_3rd_pty);

	/* Additional RLT invoke info - Octet 12 */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	ASN1_ADD_WORDCOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, callwithid->rlt_call_id & 0xFFFFFF); /* Length is 3 octets */
	/* Reason for redirect - unused, set to 129 */
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_1), buffer, i, 0);
	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (pri_call_apdu_queue(apdubearer, Q931_FACILITY, buffer, i, NULL, NULL))
		return -1;

	if (q931_facility(apdubearer->pri, apdubearer)) {
		pri_message(pri, "Could not schedule facility message for call %d\n", apdubearer->cr);
		return -1;
	}
	return 0;
}

static int add_dms100_transfer_ability_apdu(struct pri *pri, q931_call *c)
{
	int i = 0;
	unsigned char buffer[256];
	struct rose_component *comp = NULL, *compstk[10];
	const unsigned char rlt_op_ind = RLT_OPERATION_IND;
	int compsp = 0;

	buffer[i++] = (Q932_PROTOCOL_ROSE);  /* Note to self: DON'T set the EXT bit */
	buffer[i++] = (0x80 | RLT_SERVICE_ID); /* Service Identifier octet */

	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* Invoke ID is set to the operation ID */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, rlt_op_ind);
	
	/* Operation Tag - basically the same as the invoke ID tag */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, rlt_op_ind);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (pri_call_apdu_queue(c, Q931_SETUP, buffer, i, NULL, NULL))
		return -1;
	else
		return 0;
}

/* Sending callername information functions */
static int add_callername_facility_ies(struct pri *pri, q931_call *c, int cpe)
{
	int res = 0;
	int i = 0;
	unsigned char buffer[256];
	unsigned char namelen = 0;
	struct rose_component *comp = NULL, *compstk[10];
	int compsp = 0;
	int mymessage = 0;
	static unsigned char op_tag[] = { 
		0x2a, /* informationFollowing 42 */
		0x86,
		0x48,
		0xce,
		0x15,
		0x00,
		0x04
	};
		
	if (!strlen(c->callername)) {
		return -1;
	}

	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);
	/* Interpretation component */

	if (pri->switchtype == PRI_SWITCH_QSIG) {
		ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i);
		ASN1_PUSH(compstk, compsp, comp);
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);
		ASN1_FIXUP(compstk, compsp, buffer, i);
	}

	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 0);

	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	/* Invoke ID */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));

	/* Operation Tag */
	res = asn1_string_encode(ASN1_OBJECTIDENTIFIER, &buffer[i], sizeof(buffer)-i, sizeof(op_tag), op_tag, sizeof(op_tag));
	if (res < 0)
		return -1;
	i += res;

	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 0);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (!cpe) {
		if (pri_call_apdu_queue(c, Q931_SETUP, buffer, i, NULL, NULL))
			return -1;
	}


	/* Now the APDU that contains the information that needs sent.
	 * We can reuse the buffer since the queue function doesn't
	 * need it. */

	i = 0;
	namelen = strlen(c->callername);
	if (namelen > 50) {
		namelen = 50; /* truncate the name */
	}

	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);
	/* Interpretation component */

	if (pri->switchtype == PRI_SWITCH_QSIG) {
		ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i);
		ASN1_PUSH(compstk, compsp, comp);
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);
		ASN1_FIXUP(compstk, compsp, buffer, i);
	}

	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 0);

	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* Invoke ID */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));

	/* Operation ID: Calling name */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, SS_CNID_CALLINGNAME);

	res = asn1_string_encode((ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), &buffer[i], sizeof(buffer)-i,  50, c->callername, namelen);
	if (res < 0)
		return -1;
	i += res;
	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (cpe) 
		mymessage = Q931_SETUP;
	else
		mymessage = Q931_FACILITY;

	if (pri_call_apdu_queue(c, mymessage, buffer, i, NULL, NULL))
		return -1;
	
	return 0;
}
/* End Callername */

/* MWI related encode and decode functions */
static void mwi_activate_encode_cb(void *data)
{
	return;
}

int mwi_message_send(struct pri* pri, q931_call *call, struct pri_sr *req, int activate)
{
	int i = 0;
	unsigned char buffer[255] = "";
	int destlen = strlen(req->called);
	struct rose_component *comp = NULL, *compstk[10];
	int compsp = 0;
	int res;

	if (destlen <= 0) {
		return -1;
	} else if (destlen > 20)
		destlen = 20;  /* Destination number cannot be greater then 20 digits */

	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);
	/* Interpretation component */

	ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 0);

	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));

	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, (activate) ? SS_MWI_ACTIVATE : SS_MWI_DEACTIVATE);
	ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	/* PartyNumber */
	res = asn1_string_encode((ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), &buffer[i], sizeof(buffer)-i, destlen, req->called, destlen);
	
	if (res < 0)
		return -1;
	i += res;

	/* Enumeration: basicService */
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 1 /* contents: Voice */);
	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	return pri_call_apdu_queue(call, Q931_SETUP, buffer, i, mwi_activate_encode_cb, NULL);
}
/* End MWI */

/* EECT functions */
int eect_initiate_transfer(struct pri *pri, q931_call *c1, q931_call *c2)
{
	int i = 0;
	int res = 0;
	unsigned char buffer[255] = "";
	short call_reference = c2->cr ^ 0x8000;  /* Let's do the trickery to make sure the flag is correct */
	struct rose_component *comp = NULL, *compstk[10];
	int compsp = 0;
	static unsigned char op_tag[] = {
		0x2A,
		0x86,
		0x48,
		0xCE,
		0x15,
		0x00,
		0x08,
	};

	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_ROSE);

	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));

	res = asn1_string_encode(ASN1_OBJECTIDENTIFIER, &buffer[i], sizeof(buffer)-i, sizeof(op_tag), op_tag, sizeof(op_tag));
	if (res < 0)
		return -1;
	i += res;

	ASN1_ADD_SIMPLE(comp, (ASN1_SEQUENCE | ASN1_CONSTRUCTOR), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	ASN1_ADD_WORDCOMP(comp, ASN1_INTEGER, buffer, i, call_reference);
	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	res = pri_call_apdu_queue(c1, Q931_FACILITY, buffer, i, NULL, NULL);
	if (res) {
		pri_message(pri, "Could not queue APDU in facility message\n");
		return -1;
	}

	/* Remember that if we queue a facility IE for a facility message we
	 * have to explicitly send the facility message ourselves */

	res = q931_facility(c1->pri, c1);
	if (res) {
		pri_message(pri, "Could not schedule facility message for call %d\n", c1->cr);
		return -1;
	}

	return 0;
}
/* End EECT */

/* QSIG CF CallRerouting */
int qsig_cf_callrerouting(struct pri *pri, q931_call *c, const char* dest, const char* original, const char* reason)
{
/*CallRerouting ::= OPERATION
    -- Sent from the Served User PINX to the Rerouting PINX
    ARGUMENT SEQUENCE
    { reroutingReason DiversionReason,
    originalReroutingReason [0] IMPLICIT DiversionReason OPTIONAL,
    calledAddress Address,
    diversionCounter INTEGER (1..15),
    pSS1InfoElement PSS1InformationElement,
    -- The basic call information elements Bearer capability, High layer compatibility, Low
    -- layer compatibity, Progress indicator and Party category can be embedded in the
    -- pSS1InfoElement in accordance with 6.5.3.1.5
    lastReroutingNr [1] PresentedNumberUnscreened,
    subscriptionOption [2] IMPLICIT SubscriptionOption,

    callingPartySubaddress [3] PartySubaddress OPTIONAL,

    callingNumber [4] PresentedNumberScreened,

    callingName [5] Name OPTIONAL,
    originalCalledNr [6] PresentedNumberUnscreened OPTIONAL,
    redirectingName [7] Name OPTIONAL,
    originalCalledName [8] Name OPTIONAL,
    extension CHOICE {
      [9] IMPLICIT Extension ,
      [10] IMPLICIT SEQUENCE OF Extension } OPTIONAL }
*/

	int i = 0, j;
	int res = 0;
	unsigned char buffer[255] = "";
	int len = 253;
	struct rose_component *comp = NULL, *compstk[10];
	int compsp = 0;
	static unsigned char op_tag[] = {
		0x13,
	};

	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);
	/* Interpretation component */

	ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 2);    /* reject - to get feedback from QSIG switch */

	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));

	res = asn1_string_encode(ASN1_INTEGER, &buffer[i], sizeof(buffer)-i, sizeof(op_tag), op_tag, sizeof(op_tag));
	if (res < 0)
		return -1;
	i += res;

	/* call rerouting argument */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* reroutingReason DiversionReason */

	if (reason) {
		if (!strcasecmp(reason, "cfu"))
			ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 1); /* cfu */
		else if (!strcasecmp(reason, "cfb"))
			 ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 2); /* cfb */
		else if (!strcasecmp(reason, "cfnr"))
			ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 3); /* cfnr */
	} else {
		ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 0); /* unknown */
	}


	/* calledAddress Address */
	/* explicit sequence tag for Address */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	/* implicit choice public party number tag */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	/* type of public party number = unknown */
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 0);
	/* NumberDigits of public party number */
	j = asn1_string_encode(ASN1_NUMERICSTRING, &buffer[i], len - i, 20, (char*)dest, strlen(dest));
	if (j < 0)
		return -1;

	i += j;
	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	/* diversionCounter INTEGER (1..15) */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, 1);

	/* pSS1InfoElement */
	ASN1_ADD_SIMPLE(comp, (ASN1_APPLICATION | ASN1_TAG_0 ), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	buffer[i++] = (0x04); /*  add BC */
	buffer[i++] = (0x03);
	buffer[i++] = (0x80);
	buffer[i++] = (0x90);
	buffer[i++] = (0xa3);
	buffer[i++] = (0x95);
	buffer[i++] = (0x32);
	buffer[i++] = (0x01);
	buffer[i++] = (0x81);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	/* lastReroutingNr [1]*/
	/* implicit optional lastReroutingNr tag */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* implicit choice presented number unscreened tag */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_0), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* implicit choice public party number  tag */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	/* type of public party number = unknown */
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 0);
	j = asn1_string_encode(ASN1_NUMERICSTRING, &buffer[i], len - i, 20, original?(char*)original:c->callednum, original?strlen(original):strlen(c->callednum));
	if (j < 0)
		return -1;

	i += j;
	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	/* subscriptionOption [2]*/
	/* implicit optional lastReroutingNr tag */
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);	 /* noNotification */

	/* callingNumber [4]*/
	/* implicit optional callingNumber tag */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_4), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* implicit choice presented number screened tag */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_0), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* implicit choice presentationAllowedAddress tag */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	/* type of public party number = subscriber number */
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 4);
	j = asn1_string_encode(ASN1_NUMERICSTRING, &buffer[i], len - i, 20, c->callernum, strlen(c->callernum));
	if (j < 0)
		return -1;

	i += j;
	ASN1_FIXUP(compstk, compsp, buffer, i);

	/* Screeening Indicator network provided */
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 3);

	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	/**/

	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	res = pri_call_apdu_queue(c, Q931_FACILITY, buffer, i, NULL, NULL);
	if (res) {
		pri_message(pri, "Could not queue ADPU in facility message\n");
		return -1;
	}

	/* Remember that if we queue a facility IE for a facility message we
	 * have to explicitly send the facility message ourselves */

	res = q931_facility(c->pri, c);
	if (res) {
		pri_message(pri, "Could not schedule facility message for call %d\n", c->cr);
		return -1;
	}

	return 0;
}
/* End QSIG CC-CallRerouting */

static int anfpr_pathreplacement_respond(struct pri *pri, q931_call *call, q931_ie *ie)
{
	int res;
	
	res = pri_call_apdu_queue_cleanup(call->bridged_call);
	if (res) {
	        pri_message(pri, "Could not Clear queue ADPU\n");
	        return -1;
	}
	
	/* Send message */
	res = pri_call_apdu_queue(call->bridged_call, Q931_FACILITY, ie->data, ie->len, NULL, NULL);
	if (res) {
	        pri_message(pri, "Could not queue ADPU in facility message\n");
	        return -1;
	}
	
	/* Remember that if we queue a facility IE for a facility message we
	 * have to explicitly send the facility message ourselves */
	
	res = q931_facility(call->bridged_call->pri, call->bridged_call);
	if (res) {
		pri_message(pri, "Could not schedule facility message for call %d\n", call->bridged_call->cr);
		return -1;
	}

	return 0;
}
/* AFN-PR */
int anfpr_initiate_transfer(struct pri *pri, q931_call *c1, q931_call *c2)
{
	/* Did all the tests to see if we're on the same PRI and
	 * are on a compatible switchtype */
	/* TODO */
	int i = 0;
	int res = 0;
	unsigned char buffer[255] = "";
	unsigned short call_reference = c2->cr;
	struct rose_component *comp = NULL, *compstk[10];
	unsigned char buffer2[255] = "";
	int compsp = 0;
	static unsigned char op_tag[] = {
		0x0C,
	};
	
	/* Channel 1 */
	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);
	/* Interpretation component */
	
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);
	ASN1_FIXUP(compstk, compsp, buffer, i);
	
	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 2);    /* reject - to get feedback from QSIG switch */
	
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));
	
	res = asn1_string_encode(ASN1_INTEGER, &buffer[i], sizeof(buffer)-i, sizeof(op_tag), op_tag, sizeof(op_tag));
	if (res < 0)
		return -1;
	i += res;
	
	ASN1_ADD_SIMPLE(comp, (ASN1_SEQUENCE | ASN1_CONSTRUCTOR), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	buffer[i++] = (0x0a);
	buffer[i++] = (0x01);
	buffer[i++] = (0x00);
	buffer[i++] = (0x81);
	buffer[i++] = (0x00);
	buffer[i++] = (0x0a);
	buffer[i++] = (0x01);
	buffer[i++] = (0x01);
	ASN1_ADD_WORDCOMP(comp, ASN1_INTEGER, buffer, i, call_reference);
	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);
	
	res = pri_call_apdu_queue(c1, Q931_FACILITY, buffer, i, NULL, NULL);
	if (res) {
		pri_message(pri, "Could not queue ADPU in facility message\n");
		return -1;
	}
	
	/* Remember that if we queue a facility IE for a facility message we
	 * have to explicitly send the facility message ourselves */
	
	res = q931_facility(c1->pri, c1);
	if (res) {
		pri_message(pri, "Could not schedule facility message for call %d\n", c1->cr);
		return -1;
	}
	
	/* Channel 2 */
	i = 0;
	res = 0;
	compsp = 0;
	
	buffer2[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);
	/* Interpretation component */
	
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer2, i);
	ASN1_PUSH(compstk, compsp, comp);
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer2, i, 0);
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer2, i, 0);
	ASN1_FIXUP(compstk, compsp, buffer2, i);
	
	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer2, i, 2);  /* reject */
	
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer2, i);
	ASN1_PUSH(compstk, compsp, comp);
	
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer2, i, get_invokeid(pri));
	
	res = asn1_string_encode(ASN1_INTEGER, &buffer2[i], sizeof(buffer2)-i, sizeof(op_tag), op_tag, sizeof(op_tag));
	if (res < 0)
		return -1;
	i += res;
	
	ASN1_ADD_SIMPLE(comp, (ASN1_SEQUENCE | ASN1_CONSTRUCTOR), buffer2, i);
	ASN1_PUSH(compstk, compsp, comp);
	buffer2[i++] = (0x0a);
	buffer2[i++] = (0x01);
	buffer2[i++] = (0x01);
	buffer2[i++] = (0x81);
	buffer2[i++] = (0x00);
	buffer2[i++] = (0x0a);
	buffer2[i++] = (0x01);
	buffer2[i++] = (0x01);
	ASN1_ADD_WORDCOMP(comp, ASN1_INTEGER, buffer2, i, call_reference);
	ASN1_FIXUP(compstk, compsp, buffer2, i);
	ASN1_FIXUP(compstk, compsp, buffer2, i);
	
	
	res = pri_call_apdu_queue(c2, Q931_FACILITY, buffer2, i, NULL, NULL);
	if (res) {
		pri_message(pri, "Could not queue ADPU in facility message\n");
		return -1;
	}
	
	/* Remember that if we queue a facility IE for a facility message we
	 * have to explicitly send the facility message ourselves */
	
	res = q931_facility(c2->pri, c2);
	if (res) {
		pri_message(pri, "Could not schedule facility message for call %d\n", c1->cr);
		return -1;
	}
	
	return 0;
}
/* End AFN-PR */

/* AOC */
static int aoc_aoce_charging_request_decode(struct pri *pri, q931_call *call, unsigned char *data, int len) 
{
	int chargingcase = -1;
	unsigned char *vdata = data;
	struct rose_component *comp = NULL;
	int pos1 = 0;

	if (pri->debug & PRI_DEBUG_AOC)
		dump_apdu (pri, data, len);

	do {
		GET_COMPONENT(comp, pos1, vdata, len);
		CHECK_COMPONENT(comp, ASN1_ENUMERATED, "!! Invalid AOC Charging Request argument. Expected Enumerated (0x0A) but Received 0x%02X\n");
		ASN1_GET_INTEGER(comp, chargingcase);				
		if (chargingcase >= 0 && chargingcase <= 2) {
			if (pri->debug & PRI_DEBUG_APDU)
				pri_message(pri, "Channel %d/%d, Call %d  - received AOC charging request - charging case: %i\n", 
					call->ds1no, call->channelno, call->cr, chargingcase);
		} else {
			pri_message(pri, "!! unkown AOC ChargingCase: 0x%02X", chargingcase);
			chargingcase = -1;
		}
		NEXT_COMPONENT(comp, pos1);
	} while (pos1 < len);
	if (pos1 < len) {
		pri_message(pri, "!! Only reached position %i in %i bytes long AOC-E structure:", pos1, len );
		dump_apdu (pri, data, len);
		return -1;	/* Aborted before */
	}
	return 0;
}
	

static int aoc_aoce_charging_unit_decode(struct pri *pri, q931_call *call, unsigned char *data, int len) 
{
	long chargingunits = 0, chargetype = -1, temp, chargeIdentifier = -1;
	unsigned char *vdata = data;
	struct rose_component *comp1 = NULL, *comp2 = NULL, *comp3 = NULL;
	int pos1 = 0, pos2, pos3, sublen2, sublen3;
	struct addressingdataelements_presentednumberunscreened chargednr;

	if (pri->debug & PRI_DEBUG_AOC)
		dump_apdu (pri, data, len);

	do {
		GET_COMPONENT(comp1, pos1, vdata, len);	/* AOCEChargingUnitInfo */
		CHECK_COMPONENT(comp1, ASN1_SEQUENCE, "!! Invalid AOC-E Charging Unit argument. Expected Sequence (0x30) but Received 0x%02X\n");
		SUB_COMPONENT(comp1, pos1);
		GET_COMPONENT(comp1, pos1, vdata, len);
		switch (comp1->type) {
			case (ASN1_SEQUENCE | ASN1_CONSTRUCTOR):	/* specificChargingUnits */
				sublen2 = comp1->len; 
				pos2 = pos1;
				comp2 = comp1;
				SUB_COMPONENT(comp2, pos2);
				do {
					GET_COMPONENT(comp2, pos2, vdata, len);
					switch (comp2->type) {
						case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1):	/* RecordedUnitsList (0xA1) */
							SUB_COMPONENT(comp2, pos2);
							GET_COMPONENT(comp2, pos2, vdata, len);
							CHECK_COMPONENT(comp2, ASN1_SEQUENCE, "!! Invalid AOC-E Charging Unit argument. Expected Sequence (0x30) but received 0x02%X\n");	/* RecordedUnits */
							sublen3 = pos2 + comp2->len;
							pos3 = pos2;
							comp3 = comp2;
							SUB_COMPONENT(comp3, pos3);
							do {
								GET_COMPONENT(comp3, pos3, vdata, len);
								switch (comp3->type) {
									case ASN1_INTEGER:	/* numberOfUnits */
										ASN1_GET_INTEGER(comp3, temp);
										chargingunits += temp;
									case ASN1_NULL:		/* notAvailable */
										break;
									default:
										pri_message(pri, "!! Don't know how to handle 0x%02X in AOC-E RecordedUnits\n", comp3->type);
								}
								NEXT_COMPONENT(comp3, pos3);
							} while (pos3 < sublen3);
							if (pri->debug & PRI_DEBUG_AOC)
								pri_message(pri, "Channel %d/%d, Call %d - received AOC-E charging: %i unit%s\n", 
									call->ds1no, call->channelno, call->cr, chargingunits, (chargingunits == 1) ? "" : "s");
							break;
						case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_2):	/* AOCEBillingID (0xA2) */
							SUB_COMPONENT(comp2, pos2);
							GET_COMPONENT(comp2, pos2, vdata, len);
							ASN1_GET_INTEGER(comp2, chargetype);
							pri_message(pri, "!! not handled: Channel %d/%d, Call %d - received AOC-E billing ID: %i\n", 
								call->ds1no, call->channelno, call->cr, chargetype);
							break;
						default:
							pri_message(pri, "!! Don't know how to handle 0x%02X in AOC-E RecordedUnitsList\n", comp2->type);
					}
					NEXT_COMPONENT(comp2, pos2);
				} while (pos2 < sublen2);
				break;
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_1): /* freeOfCharge (0x81) */
				if (pri->debug & PRI_DEBUG_AOC)
					pri_message(pri, "Channel %d/%d, Call %d - received AOC-E free of charge\n", call->ds1no, call->channelno, call->cr);
				chargingunits = 0;
				break;
			default:
				pri_message(pri, "!! Invalid AOC-E specificChargingUnits. Expected Sequence (0x30) or Object Identifier (0x81/0x01) but received 0x%02X\n", comp1->type);
		}
		NEXT_COMPONENT(comp1, pos1);
		GET_COMPONENT(comp1, pos1, vdata, len); /* get optional chargingAssociation. will 'break' when reached end of structure */
		switch (comp1->type) {
			/* TODO: charged number is untested - please report! */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_0): /* chargedNumber (0xA0) */
				if(rose_presented_number_unscreened_decode(pri, comp1->data, comp1->len, &chargednr) != 0)
					return -1;
				pri_message(pri, "!! not handled: Received ChargedNr '%s' \n", chargednr.partyaddress);
				pri_message(pri, "  ton = %d, pres = %d, npi = %d\n", chargednr.ton, chargednr.pres, chargednr.npi);
				break;
			case ASN1_INTEGER:
				ASN1_GET_INTEGER(comp1, chargeIdentifier);
				break;
			default:
				pri_message(pri, "!! Invalid AOC-E chargingAssociation. Expected Object Identifier (0xA0) or Integer (0x02) but received 0x%02X\n", comp1->type);
		}
		NEXT_COMPONENT(comp1, pos1);
	} while (pos1 < len);

	if (pos1 < len) {
		pri_message(pri, "!! Only reached position %i in %i bytes long AOC-E structure:", pos1, len );
		dump_apdu (pri, data, len);
		return -1;	/* oops - aborted before */
	}
	call->aoc_units = chargingunits;
	
	return 0;
}

static int aoc_aoce_charging_unit_encode(struct pri *pri, q931_call *c, long chargedunits)
{
	/* sample data: [ 91 a1 12 02 02 3a 78 02 01 24 30 09 30 07 a1 05 30 03 02 01 01 ] */
	int i = 0, res = 0, compsp = 0;
	unsigned char buffer[255] = "";
	struct rose_component *comp = NULL, *compstk[10];

	/* ROSE protocol (0x91)*/
	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_ROSE);

	/* ROSE Component (0xA1,len)*/
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp); 

	/* ROSE invokeId component (0x02,len,id)*/
	ASN1_ADD_WORDCOMP(comp, INVOKE_IDENTIFIER, buffer, i, ++pri->last_invoke);

	/* ROSE operationId component (0x02,0x01,0x24)*/
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, ROSE_AOC_AOCE_CHARGING_UNIT);

	/* AOCEChargingUnitInfo (0x30,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	if (chargedunits > 0) {
		/* SpecificChargingUnits (0x30,len) */
		ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i);
		ASN1_PUSH(compstk, compsp, comp);

		/* RecordedUnitsList (0xA1,len) */
		ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1), buffer, i);
		ASN1_PUSH(compstk, compsp, comp);
		
		/* RecordedUnits (0x30,len) */
		ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i);
		ASN1_PUSH(compstk, compsp, comp);
		
		/* NumberOfUnits (0x02,len,charge) */
		ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, chargedunits);

		ASN1_FIXUP(compstk, compsp, buffer, i);
		ASN1_FIXUP(compstk, compsp, buffer, i);
		ASN1_FIXUP(compstk, compsp, buffer, i);
	} else {
		/* freeOfCharge (0x81,0) */
		ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_1), buffer, i);
	}
	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i); 
	
	if (pri->debug & PRI_DEBUG_AOC)
		dump_apdu (pri, buffer, i);
		
	/* code below is untested */
	res = pri_call_apdu_queue(c, Q931_FACILITY, buffer, i, NULL, NULL);
	if (res) {
		pri_message(pri, "Could not queue APDU in facility message\n");
		return -1;
	}

	/* Remember that if we queue a facility IE for a facility message we
	 * have to explicitly send the facility message ourselves */
	res = q931_facility(c->pri, c);
	if (res) {
		pri_message(pri, "Could not schedule facility message for call %d\n", c->cr);
		return -1;
	}

	return 0;
}
/* End AOC */

/* ===== Call Transfer Supplementary Service (ECMA-178) ===== */

static int rose_call_transfer_complete_decode(struct pri *pri, q931_call *call, struct rose_component *sequence, int len)
{
	int i = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = sequence->data;
	int size = 0;

	struct addressingdataelements_presentednumberscreened redirection_number;
	struct nameelements_name redirectionname = { "", CHARACTER_SET_UNKNOWN, 0 };
	char basiccallinfoelements[257] = "";
	int call_status = 0;	/* answered(0) */
	int end_designation;

	/* Data checks */
	if (sequence->type != (ASN1_CONSTRUCTOR | ASN1_SEQUENCE)) { /* Constructed Sequence */
		pri_message(pri, "Invalid callTransferComplete argument. (Not a sequence)\n");
		return -1;
	}

	if (sequence->len == ASN1_LEN_INDEF) {
		len -= 4;   /* For the 2 extra characters at the end
		               and two characters of header */
	} else
		len -= 2;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "     CT-Complete: len=%d\n", len);

	/* CTCompleteArg SEQUENCE */
	do {
		/* endDesignation EndDesignation */
		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, ASN1_ENUMERATED, "Invalid endDesignation type 0x%X of ROSE callTransferComplete component received\n");
		ASN1_GET_INTEGER(comp, end_designation);
		NEXT_COMPONENT(comp, i);
		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "     CT-Complete: Received endDesignation=%s(%d)\n", enddesignation_to_str(end_designation), end_designation);

		/* redirectionNumber PresentedNumberScreened */
		GET_COMPONENT(comp, i, vdata, len);
		size = rose_presented_number_screened_decode(pri, (u_int8_t *)comp, comp->len + 2, &redirection_number);
		if (size < 0)
			return -1;
		comp->len = size;
		NEXT_COMPONENT(comp, i);
		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "     CT-Complete: Received redirectionNumber=%s\n", redirection_number.partyaddress);

		/* Type SEQUENCE specifies an ordered list of component types.           *
		 * We decode all components but for simplicity we don't check the order. */
		while (i < len) {
			GET_COMPONENT(comp, i, vdata, len);

			switch(comp->type) {
 			case (ASN1_APPLICATION):
				/* basicCallInfoElements PSS1InformationElement OPTIONAL */
				size = asn1_name_decode((u_int8_t *)comp, comp->len + 2, basiccallinfoelements, sizeof(basiccallinfoelements));
				if (size < 0)
					return -1;
				i += size;
				if (pri->debug & PRI_DEBUG_APDU) {
					int j;
					pri_message(pri, "     CT-Complete: Received basicCallInfoElements\n");
					pri_message(pri, "                  ");
					for (j = 0; basiccallinfoelements[j] != '\0'; j++)
						pri_message(pri, "%02x ", (u_int8_t)basiccallinfoelements[j]);
					pri_message(pri, "\n");
				}
				break;
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0):                      /* [0] namePresentationAllowedSimple */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1):   /* [1] namePresentationAllowedExtended */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2):                      /* [2] namePresentationRestrictedSimple */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_3):   /* [3] namePresentationRestrictedExtended */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_4):                      /* [4] nameNotAvailable */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_7):                      /* [7] namePresentationRestrictedNull */
				/* redirectionName Name OPTIONAL */
				size = rose_name_decode(pri, (u_int8_t *)comp, comp->len + 2, &redirectionname);
				if (size < 0)
					return -1;
				i += size;
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     CT-Complete: Received RedirectionName '%s', namepres %s(%d), characterset %s(%d)\n",
						    redirectionname.name, namepres_to_str(redirectionname.namepres), redirectionname.namepres,
						    characterset_to_str(redirectionname.characterset), redirectionname.characterset);
				break;
			case (ASN1_ENUMERATED):
				/* callStatus CallStatus DEFAULT answered */
				ASN1_GET_INTEGER(comp, call_status);
				NEXT_COMPONENT(comp,i);
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     CT-Complete: Received callStatus=%s(%d)\n", callstatus_to_str(call_status), call_status);
				break;
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_9):	/* [9] IMPLICIT Extension */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_10):	/* [10] IMPLICIT SEQUENCE OF Extension */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "!!     CT-Complete: Ignoring CallTransferComplete component 0x%X\n", comp->type);
				NEXT_COMPONENT(comp, i);
				break;
			default:
				pri_message(pri, "!!     CT-Complete: Invalid CallTransferComplete component received 0x%X\n", comp->type);
				return -1;
			}
		}

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "     CT-Complete: callStatus=%s(%d)\n", callstatus_to_str(call_status), call_status);

		call->ctcompleteflag = 1;
		if ((redirection_number.pres & PRES_RESTRICTION) == PRES_ALLOWED) {
			libpri_copy_string(call->ctcompletenum, redirection_number.partyaddress, sizeof(call->ctcompletenum));
		} else {
			call->ctcompletenum[0] = '\0';
		}
		call->ctcompletepres = redirection_number.pres;
		call->ctcompleteplan = ((redirection_number.ton & 0x07) << 4) | (redirection_number.npi & 0x0f);
		call->ctcompletecallstatus = call_status;

		if (redirectionname.namepres != 0) {
			libpri_copy_string(call->ctcompletename, redirectionname.name, sizeof(call->ctcompletename));
		} else {
			call->ctcompletename[0] = '\0';
		}

		return 0;
	}
	while (0);

	return -1;
}

static int rose_call_transfer_complete_encode(struct pri *pri, q931_call *call)
{
	int i = 0, compsp = 0;
	struct rose_component *comp, *compstk[10];
	unsigned char buffer[256];
	int size;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "    Encode CallTransferComplete\n");

	/* Protocol Profile = 0x1f (Networking Extensions)  (0x9f) */
	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);

	/* Network Facility Extension */
	if (pri->switchtype == PRI_SWITCH_QSIG) {
		/* tag component NetworkFacilityExtension (0xaa, len ) */
		ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i);
		ASN1_PUSH(compstk, compsp, comp);

		/* sourceEntity  (0x80,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);	/* endPINX(0) */

		/* destinationEntity  (0x82,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);	/* endPINX(0) */
		ASN1_FIXUP(compstk, compsp, buffer, i);
	}

	/* Network Protocol Profile */
	/*  - not included - */

	/* Interpretation APDU  (0x8b,0x01,0x00) */
	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 0);	/* discardAnyUnrecognisedInvokePdu(0) */

	/* Service APDU(s): */

	/* ROSE InvokePDU  (0xa1,len) */
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* ROSE InvokeID  (0x02,0x01,invokeid) */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));

	/* ROSE operationId  (0x02,0x01,0x0c)*/
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, ROSE_CALL_TRANSFER_COMPLETE);


	/* CTCompleteArg */

	/* constructor component  (0x30,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);


	/* endDesignation  (0x0a,0x01,0x00) */
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 0);		/* primaryEnd(0) */


	/* redirectionNumber PresentedNumberScreened */

	/* tag component presentationAllowedAddress  (0xa0,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_0), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* presentationAllowedAddress, implicit NumberScreened */
	size = rose_number_screened_encode(pri, &buffer[i], 1, typeofnumber_from_q931(pri, (call->connectedplan & 0x70) >> 4), call->connectedpres & 0x03, call->connectednum);
	if (size < 0)
		return -1;
	i += size;

	ASN1_FIXUP(compstk, compsp, buffer, i);

	/* basicCallInfoElements */
	/*  - not included -  */

#if 0
	/* basicCallInfoElements  (0x40,0x00) */
	ASN1_ADD_SIMPLE(comp, (ASN1_APPLICATION| ASN1_TAG_0), buffer, i);
#endif

	/* redirectionName */
	if (call->connectedname[0]) {
		/* tag component namePresentationAllowedSimple  (0x80,len) */
		ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i);
		ASN1_PUSH(compstk, compsp, comp);

		/* namePresentationAllowedSimple, implicid NameData */
		size = rose_namedata_encode(pri, &buffer[i], 1, call->connectedname);
		if (size < 0)
			return -1;
		i += size;

		ASN1_FIXUP(compstk, compsp, buffer, i);
	}

	/* callStatus */
	/*  - not included, default: answered(0) -  */

#if 0
	/* callStatus  (0x0a,0x01,0x00) */
	ASN1_ADD_BYTECOMP(comp, ASN1_ENUMERATED, buffer, i, 0);		/* answered(0) */
#endif

	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (pri_call_apdu_queue(call, Q931_FACILITY, buffer, i, NULL, NULL))
		return -1;

	return 0;
}

static int rose_call_transfer_active_decode(struct pri *pri, q931_call *call, struct rose_component *sequence, int len)
{
	int i = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = sequence->data;
	int size = 0;

	struct addressingdataelements_presentedaddressscreened connectedaddress;
	struct nameelements_name connectedname = { "", CHARACTER_SET_UNKNOWN, 0 };
	char basiccallinfoelements[257] = "";

	/* Data checks */
	if (sequence->type != (ASN1_CONSTRUCTOR | ASN1_SEQUENCE)) { /* Constructed Sequence */
		pri_message(pri, "Invalid callTransferActive argument. (Not a sequence)\n");
		return -1;
	}

	if (sequence->len == ASN1_LEN_INDEF) {
		len -= 4;   /* For the 2 extra characters at the end
		               and two characters of header */
	} else
		len -= 2;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "     CT-Active: len=%d\n", len);

	/* CTActiveArg SEQUENCE */
	do {
		/* connectedAddress PresentedAddressScreened */
		GET_COMPONENT(comp, i, vdata, len);
		size = rose_presented_address_screened_decode(pri, (u_int8_t *)comp, comp->len + 2, &connectedaddress);
		if (size < 0)
			return -1;
		comp->len = size;
		NEXT_COMPONENT(comp, i);
		if (pri->debug & PRI_DEBUG_APDU) {
			pri_message(pri, "     CT-Active: Received connectedAddress=%s\n", connectedaddress.partyaddress);
		}

		/* Type SEQUENCE specifies an ordered list of component types.           *
		 * We decode all components but for simplicity we don't check the order. */
		while (i < len) {
			GET_COMPONENT(comp, i, vdata, len);

			switch(comp->type) {
 			case (ASN1_APPLICATION):
				/* basiccallinfoelements PSS1InformationElement OPTIONAL */
				size = asn1_name_decode((u_int8_t *)comp, comp->len + 2, basiccallinfoelements, sizeof(basiccallinfoelements));
				if (size < 0)
					return -1;
				i += size;
				if (pri->debug & PRI_DEBUG_APDU) {
					int j;
					pri_message(pri, "     CT-Active: Received basicCallInfoElements\n");
					pri_message(pri, "                ");
					for (j = 0; basiccallinfoelements[j] != '\0'; j++)
						pri_message(pri, "%02x ", (u_int8_t)basiccallinfoelements[j]);
					pri_message(pri, "\n");
				}
				break;
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0):                      /* [0] namePresentationAllowedSimple */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1):   /* [1] namePresentationAllowedExtended */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2):                      /* [2] namePresentationRestrictedSimple */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_3):   /* [3] namePresentationRestrictedExtended */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_4):                      /* [4] nameNotAvailable */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_7):                      /* [7] namePresentationRestrictedNull */
				/* connectedName Name OPTIONAL */
				size = rose_name_decode(pri, (u_int8_t *)comp, comp->len + 2, &connectedname);
				if (size < 0)
					return -1;
				i += size;
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     CT-Active: Received ConnectedName '%s', namepres %s(%d), characterset %s(%d)\n",
						    connectedname.name, namepres_to_str(connectedname.namepres), connectedname.namepres,
						    characterset_to_str(connectedname.characterset), connectedname.characterset);
				break;
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_9):   /* [9] IMPLICIT Extension */
			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_10):  /* [10] IMPLICIT SEQUENCE OF Extension */
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "!!     CT-Active: Ignoring CallTransferActive component 0x%X\n", comp->type);
				NEXT_COMPONENT(comp, i);
				break;
 			default:
 				pri_message(pri, "!!     CT-Active: Invalid CallTransferActive component received 0x%X\n", comp->type);
 				return -1;
 			}
		}

		call->ctactiveflag = 1;
		if ((connectedaddress.pres & PRES_RESTRICTION) == PRES_ALLOWED) {
			libpri_copy_string(call->ctactivenum, connectedaddress.partyaddress, sizeof(call->ctactivenum));
		} else {
			call->ctactivenum[0] = '\0';
		}
		call->ctactivepres = connectedaddress.pres;
		call->ctactiveplan = ((connectedaddress.ton & 0x07) << 4) | (connectedaddress.npi & 0x0f);

		if (connectedname.namepres != 0) {
			libpri_copy_string(call->ctactivename, connectedname.name, sizeof(call->ctactivename));
		} else {
			call->ctactivename[0] = '\0';
		}

		return 0;
	}
	while (0);

	return -1;
}

#if 0
static int rose_call_transfer_update_decode(struct pri *pri, q931_call *call, struct rose_component *sequence, int len)
{
	int i = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = sequence->data;
	int res = 0;

	struct addressingdataelements_presentednumberscreened redirection_number;
	redirection_number.partyaddress[0] = 0;
	char redirection_name[51] = "";
	call->callername[0] = 0;
	call->callernum[0] = 0;

	/* Data checks */
	if (sequence->type != (ASN1_CONSTRUCTOR | ASN1_SEQUENCE)) { /* Constructed Sequence */
		pri_message(pri, "Invalid callTransferUpdate argument. (Not a sequence)\n");
		return -1;
	}

	if (sequence->len == ASN1_LEN_INDEF) {
		len -= 4;   /* For the 2 extra characters at the end
		               and two characters of header */
	} else
		len -= 2;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "     CT-Update: len=%d\n", len);

	do {
		/* Redirection Number */
		GET_COMPONENT(comp, i, vdata, len);
		res = rose_presented_number_screened_decode(pri, (u_int8_t *)comp, comp->len + 2, &redirection_number);
		if (res < 0)
			return -1;
		comp->len = res;
		if (res > 2) {
			if (pri->debug & PRI_DEBUG_APDU)
				pri_message(pri, "     CT-Update: Received redirectionNumber=%s\n", redirection_number.partyaddress);
			strncpy(call->callernum, redirection_number.partyaddress, 20);
			call->callernum[20] = 0;
		}
		NEXT_COMPONENT(comp, i);

		/* Redirection Name */
		GET_COMPONENT(comp, i, vdata, len);
		res = asn1_name_decode((u_int8_t *)comp, comp->len + 2, redirection_name, sizeof(redirection_name));
		if (res < 0)
			return -1;
		memcpy(call->callername, comp->data, comp->len);
		call->callername[comp->len] = 0;
		ASN1_FIXUP_LEN(comp, res);
		comp->len = res;
		NEXT_COMPONENT(comp, i);
		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "     CT-Update: Received redirectionName '%s'\n", redirection_name);


#if 0 /* This one is optional. How do we check if it is there? */
		/* Basic Call Info Elements */
		GET_COMPONENT(comp, i, vdata, len);
		NEXT_COMPONENT(comp, i);
#endif


		/* Argument Extension */
#if 0 /* Not supported */
		GET_COMPONENT(comp, i, vdata, len);
		switch (comp->type) {
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_9):   /* [9] IMPLICIT Extension */
				res = rose_extension_decode(pri, call, comp->data, comp->len, &redirection_number);
				if (res < 0)
					return -1;
				ASN1_FIXUP_LEN(comp, res);
				comp->len = res;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_10):    /* [10] IMPLICIT SEQUENCE OF Extension */
				res = rose_sequence_of_extension_decode(pri, call, comp->data, comp->len, &redirection_number);
				if (res < 0)
					return -1;
				ASN1_FIXUP_LEN(comp, res);
				comp->len = res;

			default:
				pri_message(pri, "     CT-Update: !! Unknown argumentExtension received 0x%X\n", comp->type);
				return -1;
		}
#else
		GET_COMPONENT(comp, i, vdata, len);
		ASN1_FIXUP_LEN(comp, res);
		NEXT_COMPONENT(comp, i);
#endif

		if(i < len)
			pri_message(pri, "     CT-Update: !! not all information is handled !! i=%d / len=%d\n", i, len);

		return 0;
	}
	while (0);

	return -1;
}
#endif

/* ===== End Call Transfer Supplementary Service (ECMA-178) ===== */



static int rose_calling_name_decode(struct pri *pri, q931_call *call, struct rose_component *sequence, int len)
{
	struct nameelements_name callingname;
	int res;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "    Handle callingName\n");

	res = rose_name_decode(pri, (u_int8_t *)sequence, sequence->len + 2, &callingname);
	if (res < 0)
		return -1;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "     Received CallingName '%s', namepres: %s(%d), characterset %s(%d)\n",
		            callingname.name, namepres_to_str(callingname.namepres), callingname.namepres,
		            characterset_to_str(callingname.characterset), callingname.characterset);

	if (callingname.namepres >= 0) {
		libpri_copy_string(call->callername, callingname.name, sizeof(call->callername));
	}

	return 0;
}

static int rose_called_name_decode(struct pri *pri, q931_call *call, struct rose_component *sequence, int len)
{
	struct nameelements_name calledname;
	int res;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "    Handle calledName\n");

	res = rose_name_decode(pri, (u_int8_t *)sequence, sequence->len + 2, &calledname);
	if (res < 0)
		return -1;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "     Received CalledName '%s', namepres %s(%d), characterset %s(%d)\n",
		            calledname.name, namepres_to_str(calledname.namepres), calledname.namepres,
		            characterset_to_str(calledname.characterset), calledname.characterset);

	if (calledname.namepres != 0) {
		libpri_copy_string(call->calledname, calledname.name, sizeof(call->calledname));
	} else {
		call->calledname[0] = '\0';
	}

	return 0;
}

int rose_called_name_encode(struct pri *pri, q931_call *call, int messagetype)
{
	int i = 0, compsp = 0;
	struct rose_component *comp, *compstk[10];
	unsigned char buffer[256];
	int size;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "    Encode calledName\n");

	/* Protocol Profile = 0x1f (Networking Extensions)  (0x9f) */
	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);

	/* Network Facility Extension */
	if (pri->switchtype == PRI_SWITCH_QSIG) {
		/* tag component NetworkFacilityExtension (0xaa, len ) */
		ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i);
		ASN1_PUSH(compstk, compsp, comp);

		/* sourceEntity  (0x80,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);	/* endPINX(0) */

		/* destinationEntity  (0x82,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);	/* endPINX(0) */
		ASN1_FIXUP(compstk, compsp, buffer, i);
	}

	/* Network Protocol Profile */
	/*  - not included - */

	/* Interpretation APDU  (0x8b,0x01,0x00) */
	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 0);	/* discardAnyUnrecognisedInvokePdu(0) */

	/* Service APDU(s): */

	/* ROSE InvokePDU  (0xa1,len) */
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* ROSE InvokeID  (0x02,0x01,invokeid) */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));

	/* ROSE operationId  (0x02,0x01,0x02)*/
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, SS_CNOP_CALLEDNAME);

	/* tag component namePresentationAllowedSimple  (0x80,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* namePresentationAllowedSimple, implicid NameData */
	size = rose_namedata_encode(pri, &buffer[i], 1, call->connectedname);
	if (size < 0)
		return -1;
	i += size;

	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (pri_call_apdu_queue(call, messagetype, buffer, i, NULL, NULL))
		return -1;

	return 0;
}

static int rose_connected_name_decode(struct pri *pri, q931_call *call, struct rose_component *sequence, int len)
{
	struct nameelements_name connectedname;
	int res;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "    Handle connectedName\n");

	res = rose_name_decode(pri, (u_int8_t *)sequence, sequence->len + 2, &connectedname);
	if (res < 0)
		return -1;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "     Received ConnectedName '%s', namepres %s(%d), characterset %s(%d)\n",
		            connectedname.name, namepres_to_str(connectedname.namepres), connectedname.namepres,
		            characterset_to_str(connectedname.characterset), connectedname.characterset);

	if (connectedname.namepres != 0) {
		libpri_copy_string(call->connectedname, connectedname.name, sizeof(call->connectedname));
	} else {
		call->connectedname[0] = '\0';
	}

	return 0;
}

int rose_connected_name_encode(struct pri *pri, q931_call *call, int messagetype)
{
	int i = 0, compsp = 0;
	struct rose_component *comp, *compstk[10];
	unsigned char buffer[256];
	int size;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "    Encode connectedName\n");

	/* Protocol Profile = 0x1f (Networking Extensions)  (0x9f) */
	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);

	/* Network Facility Extension */
	if (pri->switchtype == PRI_SWITCH_QSIG) {
		/* tag component NetworkFacilityExtension (0xaa, len ) */
		ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i);
		ASN1_PUSH(compstk, compsp, comp);

		/* sourceEntity  (0x80,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);	/* endPINX(0) */

		/* destinationEntity  (0x82,0x01,0x00) */
		ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);	/* endPINX(0) */
		ASN1_FIXUP(compstk, compsp, buffer, i);
	}

	/* Network Protocol Profile */
	/*  - not included - */

	/* Interpretation APDU  (0x8b,0x01,0x00) */
	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 0);	/* discardAnyUnrecognisedInvokePdu(0) */

	/* Service APDU(s): */

	/* ROSE InvokePDU  (0xa1,len) */
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* ROSE InvokeID  (0x02,0x01,invokeid) */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));

	/* ROSE operationId  (0x02,0x01,0x02)*/
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, SS_CNOP_CONNECTEDNAME);

	/* tag component namePresentationAllowedSimple  (0x80,len) */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);

	/* namePresentationAllowedSimple, implicid NameData */
	size = rose_namedata_encode(pri, &buffer[i], 1, call->connectedname);
	if (size < 0)
		return -1;
	i += size;

	ASN1_FIXUP(compstk, compsp, buffer, i);
	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (pri_call_apdu_queue(call, messagetype, buffer, i, NULL, NULL))
		return -1;

	return 0;
}

/* ===== Begin Call Completion Supplementary Service (ETS 300 366/ECMA 186) ===== */
/* operationId e.g. QSIG_CCBSRINGOUT, QSIG_CC_CANCEL */
int add_qsigCcInv_facility_ie (struct pri *pri, q931_call *c, int messagetype)
{
	int i = 0;
	unsigned char buffer[256];
	struct rose_component *comp = NULL, *compstk[10];
	int compsp = 0;
	u_int8_t operationId = c->ccoperation;

	/* 1 Byte 	   0x80 | 0x1F = 9F	 Protocol Profile */
	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);

	/* Interpretation component */
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i); /* 2. Byte NEtwork Facility Extension 0xAA = ASN1_CONTEXT_SPECIFIC(0x80) | (ASN1_CONSTRUCTOR 0x20)  0x0A (Tag laut Standard) */
	ASN1_PUSH(compstk, compsp, comp);
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);	/* (0x80, 0x01(len), 0x00) endPTNX */
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);	/* (0x82, 0x01(len), 0x00) endPTNX */
	ASN1_FIXUP(compstk, compsp, buffer, i);

	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 0);   /* 0x8B, 0x01(len), 0x00 discard */
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);			 /* 0xA1, 0xXX (len of Invoke Sequenz) invoke APDU */
	ASN1_PUSH(compstk, compsp, comp);

	/* Invoke ID */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));   /* 0x02 0x01 0xXX */

	/* Operation ID: QSIG_CCBSRINGOUT, QSIG_CC_CANCEL */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, operationId); /* 0x02 0x01 0x1f/0x1c */

	/* CcExtension */
	ASN1_ADD_SIMPLE(comp, ASN1_NULL, buffer, i); /* 0x05 0x00 */

	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (pri_call_apdu_queue(c, messagetype, buffer, i, NULL, NULL))
		return -1;
	
	return 0;
}

static int rose_cc_ringout_inv_decode(struct pri *pri, struct qsig_cc_extension *cc_extension, struct rose_component *choice, int len) {
	int i = 0;
	cc_extension->cc_extension_tag = 0;

	do {
		switch(choice->type) {
		case (ASN1_NULL):   /* none NULL */
			cc_extension->cc_extension_tag = ASN1_NULL;
			return 0;

		case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_14):   /* single [14] IMPLICIT Extension */
			cc_extension->cc_extension_tag = ASN1_TAG_14;
			return 0;


		case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_15):   /* multiple [15] IMPLICIT SEQUENCE OF Extension */
			cc_extension->cc_extension_tag = ASN1_TAG_15;
			return 0;

		default:
			if (choice->type == 0 && choice->len == 0) {
				return 0;
			}
			pri_message(pri, "!! Invalid ss-cc-optional-Arg component received 0x%X\n", choice->type);
			return -1;
		}

		if (i < len)
			pri_message(pri, "     ss-cc-extension: !! not all information is handled !! i=%d / len=%d\n", i, len);

		return 0;
	}
	while (0);

	return -1;
}

static int rose_cc_optional_arg_decode(struct pri *pri, q931_call *call, struct qsig_cc_optional_arg *cc_optional_arg , struct rose_component *choice, int len) {
	int i = 0;
	int res = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = choice->data;
	struct addressingdataelements_presentednumberunscreened numberA;
	struct addressingdataelements_presentednumberunscreened numberB;

	cc_optional_arg->cc_extension.cc_extension_tag = 0;
	cc_optional_arg->number_A[0] = '\0';
	cc_optional_arg->number_B[0] = '\0';

	do {
		switch(choice->type) {
		case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_0):   /* fullArg [0] IMPLICIT SEQUENCE */
			if (pri->debug & PRI_DEBUG_APDU)
				pri_message(pri, "     ss-cc-optional-Arg: len=%d\n", len);

			numberA.partyaddress[0] = '\0';

			/* numberA */
			GET_COMPONENT(comp, i, vdata, len);
			res += rose_party_number_decode(pri, (u_int8_t *)comp, comp->len + 2, &numberA);
			if (res < 0)
				return -1;
			comp->len = res;
			if (res > 2) {
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     ss-cc-optional-Arg: Received numberA=%s\n", numberA.partyaddress);
				strncpy(cc_optional_arg->number_A, numberA.partyaddress, 20);
				cc_optional_arg->number_A[20] = '\0';
			}
			NEXT_COMPONENT(comp, i);

			numberB.partyaddress[0] = '\0';

			/* numberB */
			GET_COMPONENT(comp, i, vdata, len);
			res = rose_party_number_decode(pri, (u_int8_t *)comp, comp->len + 2, &numberB);
			if (res < 0)
				return -1;
			comp->len = res;
			if (res > 2) {
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     ss-cc-optional-Arg: Received numberB=%s\n", numberB.partyaddress);
				strncpy(cc_optional_arg->number_B, numberB.partyaddress, 20);
				cc_optional_arg->number_B[20] = '\0';
			}
			NEXT_COMPONENT(comp, i);

			/* service */ /* PSS1InformationElement */
			GET_COMPONENT(comp, i, vdata, len);
			NEXT_COMPONENT(comp, i);

			/* optional */
			for (; i < len; NEXT_COMPONENT(comp, i)) {
				GET_COMPONENT(comp, i, vdata, len);
				switch(comp->type) {
				case (ASN1_NULL):   /*  */
					cc_optional_arg->cc_extension.cc_extension_tag = ASN1_NULL;
					NEXT_COMPONENT(comp, i);
					break;
				case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_14):   /*  */
					NEXT_COMPONENT(comp, i);
					break;
				case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_15):   /*  */
					NEXT_COMPONENT(comp, i);
					break;
				default:
					if (comp->type == 0 && comp->len == 0) {
						return 0;
						break; /* Found termination characters */
					}
					pri_message(pri, "!! Invalid ss-cc-optional-Arg component received 0x%X\n", comp->type);
					return -1;
				}
			}

			if (i < len)
				pri_message(pri, "     ss-cc-optional-Arg: !! not all information is handled !! i=%d / len=%d\n", i, len);

			return 0;

		/* extArg CcExtension */
		case (ASN1_NULL):   /* none NULL */
			cc_optional_arg->cc_extension.cc_extension_tag = ASN1_NULL;
			return 0;

		case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_14):   /* single [14] IMPLICIT Extension */
			cc_optional_arg->cc_extension.cc_extension_tag = ASN1_TAG_14;
			return 0;


		case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_15):   /* multiple [15] IMPLICIT SEQUENCE OF Extension */
			cc_optional_arg->cc_extension.cc_extension_tag = ASN1_TAG_15;
			return 0;

		default:
			if (choice->type == 0 && choice->len == 0) {
				return 0;
			}
			pri_message(pri, "!! Invalid ss-cc-optional-Arg component received 0x%X\n", choice->type);
			return -1;
		}

		if (i < len)
			pri_message(pri, "     ss-cc-optional-Arg: !! not all information is handled !! i=%d / len=%d\n", i, len);

		return 0;
	}
	while (0);

	return -1;
}

static int rose_cc_request_result_decode(struct pri *pri, struct qsig_cc_request_res *cc_request_res , struct rose_component *sequence, int len)
{
	int i = 0;
	struct rose_component *comp = NULL;
	unsigned char *vdata = sequence->data;

	cc_request_res->no_path_reservation           = 0;  /* Default FALSE */
	cc_request_res->retain_service                = 0;  /* Default FALSE */
	cc_request_res->cc_extension.cc_extension_tag = 0;

	/* Data checks */
	if (sequence->type != (ASN1_CONSTRUCTOR | ASN1_SEQUENCE)) { /* Constructed Sequence */
		pri_message(pri, "Invalid cc request result argument. (Not a sequence)\n");
		return -1;
	}

	if (sequence->len == ASN1_LEN_INDEF) {
		len -= 4; /* For the 2 extra characters at the end
					   * and two characters of header */
	} else
		len -= 2;

	if (pri->debug & PRI_DEBUG_APDU)
		pri_message(pri, "     CC-request-Return-Result: len=%d\n", len);

	do {
		/* defaults and optional */
		for (; i < len; NEXT_COMPONENT(comp, i)) {
			GET_COMPONENT(comp, i, vdata, len);
			switch(comp->type) {
			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0):
				/* no-path-reservation */
				ASN1_GET_INTEGER(comp, cc_request_res->no_path_reservation);
				NEXT_COMPONENT(comp, i);
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     cc request result: Received noPathReservation=%d\n", cc_request_res->no_path_reservation);
				break;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_1):
				/* retain_service */
				ASN1_GET_INTEGER(comp, cc_request_res->retain_service);
				NEXT_COMPONENT(comp, i);
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "     cc request result: Received retainService=%d\n", cc_request_res->retain_service);
				break;

			case (ASN1_NULL):   /*  */
				cc_request_res->cc_extension.cc_extension_tag = ASN1_NULL;
				NEXT_COMPONENT(comp, i);
				break;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_14):
				cc_request_res->cc_extension.cc_extension_tag = ASN1_TAG_14;
				NEXT_COMPONENT(comp, i);
				break;

			case (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_15):
				cc_request_res->cc_extension.cc_extension_tag = ASN1_TAG_15;
				NEXT_COMPONENT(comp, i);
				break;

			default:
				if (comp->type == 0 && comp->len == 0) {
					break; /* Found termination characters */
				}
				pri_message(pri, "!! Invalid ss-cc-optional-Arg component received 0x%X\n", comp->type);
				return -1;
			}
		}

		if (i < len)
			pri_message(pri, "     ss-cc-optional-Arg: !! not all information is handled !! i=%d / len=%d\n", i, len);
		return 0;
	}
	while (0);

	return -1;
}

static int rose_ccbs_request_result_decode(struct pri *pri, struct qsig_cc_request_res *cc_request_res , struct rose_component *sequence, int len)
{
	return rose_cc_request_result_decode(pri, cc_request_res , sequence, len);
}

static int rose_ccnr_request_result_decode(struct pri *pri, struct qsig_cc_request_res *cc_request_res , struct rose_component *sequence, int len)
{
	return rose_cc_request_result_decode(pri, cc_request_res , sequence, len);
}
/* ===== End Call Completion Supplementary Service (ETS 300 366/ECMA 186) ===== */


int rose_reject_decode(struct pri *pri, q931_call *call, q931_ie *ie, unsigned char *data, int len)
{
	int i = 0;
	int problemtag = -1;
	int problem = -1;
	int invokeidvalue = -1;
	unsigned char *vdata = data;
	struct rose_component *comp = NULL;
	char *problemtagstr, *problemstr;
	
	do {
		/* Invoke ID stuff */
		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, INVOKE_IDENTIFIER, "Don't know what to do if first ROSE component is of type 0x%x\n");
		ASN1_GET_INTEGER(comp, invokeidvalue);
		NEXT_COMPONENT(comp, i);

		GET_COMPONENT(comp, i, vdata, len);
		problemtag = comp->type;
		problem = comp->data[0];

		if (pri->switchtype == PRI_SWITCH_DMS100) {
			switch (problemtag) {
			case 0x80:
				problemtagstr = "General problem";
				break;
			case 0x81:
				problemtagstr = "Invoke problem";
				break;
			case 0x82:
				problemtagstr = "Return result problem";
				break;
			case 0x83:
				problemtagstr = "Return error problem";
				break;
			default:
				problemtagstr = "Unknown";
			}

			switch (problem) {
			case 0x00:
				problemstr = "Unrecognized component";
				break;
			case 0x01:
				problemstr = "Mistyped component";
				break;
			case 0x02:
				problemstr = "Badly structured component";
				break;
			default:
				problemstr = "Unknown";
			}

			pri_error(pri, "ROSE REJECT:\n");
			pri_error(pri, "\tINVOKE ID: 0x%X\n", invokeidvalue);
			pri_error(pri, "\tPROBLEM TYPE: %s (0x%x)\n", problemtagstr, problemtag);
			pri_error(pri, "\tPROBLEM: %s (0x%x)\n", problemstr, problem);

			return 0;
		} else {
			pri_message(pri, "Unable to handle reject on switchtype %d!\n", pri->switchtype);
			return -1;
		}

	} while(0);
	
	return -1;
}


static subcommand *get_ptr_subcommand(subcommands *sub)
{
	if (sub->counter_subcmd < MAX_SUBCOMMANDS) {
		int count = sub->counter_subcmd;
		sub->counter_subcmd++;
		return &sub->subcmd[count];
	}

	return NULL;
}


int rose_return_error_decode(struct pri *pri, q931_call *call, q931_ie *ie, unsigned char *data, int len)
{
	int i = 0;
	int errorvalue = -1;
	int invokeidvalue = -1;
	unsigned char *vdata = data;
	struct rose_component *comp = NULL;
	char *invokeidstr, *errorstr;
	struct subcommand *c_subcmd;
	
	do {
		/* Invoke ID stuff */
		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, INVOKE_IDENTIFIER, "Don't know what to do if first ROSE component is of type 0x%x\n");
		ASN1_GET_INTEGER(comp, invokeidvalue);
		NEXT_COMPONENT(comp, i);

		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, ASN1_INTEGER, "Don't know what to do if second component in return error is 0x%x\n");
		ASN1_GET_INTEGER(comp, errorvalue);

		if (pri->switchtype == PRI_SWITCH_DMS100) {
			switch (invokeidvalue) {
			case RLT_OPERATION_IND:
				invokeidstr = "RLT_OPERATION_IND";
				break;
			case RLT_THIRD_PARTY:
				invokeidstr = "RLT_THIRD_PARTY";
				break;
			default:
				invokeidstr = "Unknown";
			}

			switch (errorvalue) {
			case 0x10:
				errorstr = "RLT Bridge Fail";
				break;
			case 0x11:
				errorstr = "RLT Call ID Not Found";
				break;
			case 0x12:
				errorstr = "RLT Not Allowed";
				break;
			case 0x13:
				errorstr = "RLT Switch Equip Congs";
				break;
			default:
				errorstr = "Unknown";
			}

			pri_error(pri, "ROSE RETURN ERROR:\n");
			pri_error(pri, "\tOPERATION: %s\n", invokeidstr);
			pri_error(pri, "\tERROR: %s\n", errorstr);

			return 0;
		} else if (pri->switchtype == PRI_SWITCH_QSIG) {
			switch (errorvalue) {
			case 1008:
				errorstr = "Unspecified";
				break;
			case 1012:
				errorstr = "Remote user busy again";
				break;
			case 1013:
				errorstr = "Failure to match";
				break;
			default:
				errorstr = "Unknown";
			}

			c_subcmd = get_ptr_subcommand(&call->subcmds);
			if (!c_subcmd) {
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE RETURN ERROR %i - more than %d facilities !\n", errorvalue, MAX_SUBCOMMANDS);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			}

			if (pri->debug & PRI_DEBUG_APDU)
			{
				pri_message(pri, "ROSE RETURN RESULT %i:   %s\n", errorvalue, errorstr);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
			}
			c_subcmd->cmd = CMD_CC_ERROR;
			c_subcmd->cc_error.error_value = errorvalue;
			return 0;
		} else {
			pri_message(pri, "Unable to handle return error on switchtype %d!\n", pri->switchtype);
		}

	} while(0);
	
	return -1;
}

int rose_return_result_decode(struct pri *pri, q931_call *call, q931_ie *ie, unsigned char *data, int len)
{
	int i = 0;
	int operationidvalue = -1;
	int invokeidvalue = -1;
	unsigned char *vdata = data;
	struct rose_component *comp = NULL;
	int res;
	struct subcommand *c_subcmd;
	
	do {
		/* Invoke ID stuff */
		GET_COMPONENT(comp, i, vdata, len);
		CHECK_COMPONENT(comp, INVOKE_IDENTIFIER, "Don't know what to do if first ROSE component is of type 0x%x\n");
		ASN1_GET_INTEGER(comp, invokeidvalue);
		NEXT_COMPONENT(comp, i);

		if (pri->switchtype == PRI_SWITCH_DMS100) {
			switch (invokeidvalue) {
			case RLT_THIRD_PARTY:
				if (pri->debug & PRI_DEBUG_APDU) pri_message(pri, "Successfully completed RLT transfer!\n");
				return 0;
			case RLT_OPERATION_IND:
				if (pri->debug & PRI_DEBUG_APDU) pri_message(pri, "Received RLT_OPERATION_IND\n");
				/* Have to take out the rlt_call_id */
				GET_COMPONENT(comp, i, vdata, len);
				CHECK_COMPONENT(comp, ASN1_SEQUENCE, "Protocol error detected in parsing RLT_OPERATION_IND return result!\n");

				/* Traverse the contents of this sequence */
				/* First is the Operation Value */
				SUB_COMPONENT(comp, i);
				GET_COMPONENT(comp, i, vdata, len);
				CHECK_COMPONENT(comp, ASN1_INTEGER, "RLT_OPERATION_IND should be of type ASN1_INTEGER!\n");
				ASN1_GET_INTEGER(comp, operationidvalue);

				if (operationidvalue != RLT_OPERATION_IND) {
					pri_message(pri, "Invalid Operation ID value (0x%x) in return result!\n", operationidvalue);
					return -1;
				}

				/*  Next is the Call ID */
				NEXT_COMPONENT(comp, i);
				GET_COMPONENT(comp, i, vdata, len);
				CHECK_COMPONENT(comp, ASN1_TAG_0, "Error check failed on Call ID!\n");
				ASN1_GET_INTEGER(comp, call->rlt_call_id);
				/* We have enough data to transfer the call */
				call->transferable = 1;

				return 0;
				
			default:
				pri_message(pri, "Could not parse invoke of type 0x%x!\n", invokeidvalue);
				return -1;
			}
		} else if (pri->switchtype == PRI_SWITCH_QSIG) {
			int operation_tag;

			/* sequence is optional */
			if (i >= len) 
				return 0;

			/* Data checks, sequence is optional */
			GET_COMPONENT(comp, i, vdata, len);
			if (comp->type != (ASN1_CONSTRUCTOR | ASN1_SEQUENCE)) { /* Constructed Sequence */
				pri_message(pri, "No arguments on cc-return result\n");
				return 0;
			}

			if (comp->len == ASN1_LEN_INDEF) {
				len -= 2; /* For the 2 extra characters at the end*/
			}

			/* Traverse the contents of this sequence */
			SUB_COMPONENT(comp, i);

			/* Operation Tag */
			GET_COMPONENT(comp, i, vdata, len);
			CHECK_COMPONENT(comp, ASN1_INTEGER, "Don't know what to do if second ROSE component is of type 0x%x\n");
			ASN1_GET_INTEGER(comp, operation_tag);
			NEXT_COMPONENT(comp, i);

			/* No argument - return with error */
			if (i >= len) 
				return -1;

			/* Arguement Tag */
			GET_COMPONENT(comp, i, vdata, len);
			if (!comp->type)
				return -1;

			if (pri->debug & PRI_DEBUG_APDU)
				pri_message(pri, "  [ Handling operation %d ]\n", operation_tag);
			switch (operation_tag) {
			case QSIG_CF_CALLREROUTING:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "Successfully completed QSIG CF callRerouting!\n");
				return 0;

			case QSIG_CC_CCBSREQUEST:
				c_subcmd = get_ptr_subcommand(&call->subcmds);
				if (!c_subcmd) {
					if (pri->debug & PRI_DEBUG_APDU)
						pri_message(pri, "ROSE %i:  return_result CcCcbsRequest - more than %d facilities !\n", operation_tag, MAX_SUBCOMMANDS);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
					return -1;
				}
				if (pri->debug & PRI_DEBUG_APDU)
				{
					pri_message(pri, "ROSE %i:   Handle CcCcbsRequest\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				c_subcmd->cmd = CMD_CC_CCBSREQUEST_RR;
				res = rose_ccbs_request_result_decode(pri, &c_subcmd->cc_ccbs_rr.cc_request_res, comp, len-i);
				return res;

			case QSIG_CC_CCNRREQUEST:
				c_subcmd = get_ptr_subcommand(&call->subcmds);
				if (!c_subcmd) {
					if (pri->debug & PRI_DEBUG_APDU)
						pri_message(pri, "ROSE %i:  return_result CcCcnrRequest - more than %d facilities !\n", operation_tag, MAX_SUBCOMMANDS);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
					return -1;
				}
				if (pri->debug & PRI_DEBUG_APDU)
				{
					pri_message(pri, "ROSE %i:   Handle CcCcnrRequest\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				c_subcmd->cmd = CMD_CC_CCNRREQUEST_RR;
				res = rose_ccnr_request_result_decode(pri, &c_subcmd->cc_ccnr_rr.cc_request_res, comp, len-i);
				return res;

			default:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "!! Unable to handle ROSE operation %d", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return -1;
			}
		} else {
			pri_message(pri, "Unable to handle return result on switchtype %d!\n", pri->switchtype);
			return -1;
		}

	} while(0);
	
	return -1;
}

int rose_invoke_decode(struct pri *pri, q931_call *call, q931_ie *ie, unsigned char *data, int len)
{
	int i = 0;
	int res = 0;
	int operation_tag;
	unsigned char *vdata = data;
	struct rose_component *comp = NULL, *invokeid = NULL, *operationid = NULL;
	struct subcommand *c_subcmd;
	
	do {
		/* Invoke ID stuff */
		GET_COMPONENT(comp, i, vdata, len);
#if 0
		CHECK_COMPONENT(comp, INVOKE_IDENTIFIER, "Don't know what to do if first ROSE component is of type 0x%x\n");
#endif
		invokeid = comp;
		NEXT_COMPONENT(comp, i);

		/* Operation Tag */
		GET_COMPONENT(comp, i, vdata, len);
#if 0
		CHECK_COMPONENT(comp, ASN1_INTEGER, "Don't know what to do if second ROSE component is of type 0x%x\n");
#endif
		operationid = comp;
		ASN1_GET_INTEGER(comp, operation_tag);
		NEXT_COMPONENT(comp, i);

		/* No argument - return with error */
		if (i >= len) 
			return -1;

		/* Arguement Tag */
		GET_COMPONENT(comp, i, vdata, len);
		if (!comp->type)
			return -1;

		if (pri->debug & PRI_DEBUG_APDU)
			pri_message(pri, "  [ Handling operation %d ]\n", operation_tag);

		if (pri->switchtype == PRI_SWITCH_QSIG) {

			switch (operation_tag) {
			case SS_CNID_CALLINGNAME:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "ROSE %i:   Handle CallingName\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return rose_calling_name_decode(pri, call, comp, len-i);
			case SS_CNOP_CALLEDNAME:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "ROSE %i:   Handle CalledName\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return rose_called_name_decode(pri, call, comp, len-i);
			case SS_CNOP_CONNECTEDNAME:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "ROSE %i:   Handle ConnectedName\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return rose_connected_name_decode(pri, call, comp, len-i);
			case ROSE_CALL_TRANSFER_IDENTIFY:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   CallTransferIdentify - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_CALL_TRANSFER_ABANDON:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   CallTransferAbandon - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_CALL_TRANSFER_INITIATE:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   CallTransferInitiate - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_CALL_TRANSFER_SETUP:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   CallTransferSetup - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_CALL_TRANSFER_ACTIVE:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   Handle CallTransferActive\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return rose_call_transfer_active_decode(pri, call, comp, len-i);
			case ROSE_CALL_TRANSFER_COMPLETE:
				if (pri->debug & PRI_DEBUG_APDU)
				{
					pri_message(pri, "ROSE %i:   Handle CallTransferComplete\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return rose_call_transfer_complete_decode(pri, call, comp, len-i);
			case ROSE_CALL_TRANSFER_UPDATE:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   CallTransferUpdate - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_SUBADDRESS_TRANSFER:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   SubaddressTransfer - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_DIVERTING_LEG_INFORMATION1:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "  Handle DivertingLegInformation1\n");
				return rose_diverting_leg_information1_decode(pri, call, comp, len-i);
			case ROSE_DIVERTING_LEG_INFORMATION2:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "  Handle DivertingLegInformation2\n");
				return rose_diverting_leg_information2_decode(pri, call, comp, len-i);
			case ROSE_DIVERTING_LEG_INFORMATION3:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "  Handle DivertingLegInformation3\n");
				return rose_diverting_leg_information3_decode(pri, call, comp, len-i);
			case SS_ANFPR_PATHREPLACEMENT:
				/* Clear Queue */
				res = pri_call_apdu_queue_cleanup(call->bridged_call);
				if (res) {
					pri_message(pri, "Could not Clear queue ADPU\n");
					return -1;
				}
				anfpr_pathreplacement_respond(pri, call, ie);
				break;
			case QSIG_CC_CCBSREQUEST:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:  invoke CcbsRequest - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case QSIG_CC_CCNRREQUEST:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:  invoke CcnrRequest - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case QSIG_CC_CANCEL:
				c_subcmd = get_ptr_subcommand(&call->subcmds);
				if (!c_subcmd) {
					if (pri->debug & PRI_DEBUG_APDU)
						pri_message(pri, "ROSE %i:  invoke CcCancel - more than %d facilities !\n", operation_tag, MAX_SUBCOMMANDS);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
					return -1;
				}
				if (pri->debug & PRI_DEBUG_APDU)
				{
					pri_message(pri, "ROSE %i:   Handle CcCancel\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				c_subcmd->cmd = CMD_CC_CANCEL_INV;
				res = rose_cc_optional_arg_decode(pri, call, &c_subcmd->cc_cancel_inv.cc_optional_arg, comp, len-i);
				return res;
			case QSIG_CC_EXECPOSIBLE:
				c_subcmd = get_ptr_subcommand(&call->subcmds);
				if (!c_subcmd) {
					if (pri->debug & PRI_DEBUG_APDU)
						pri_message(pri, "ROSE %i:  invoke CcExecposible - more than %d facilities !\n", operation_tag, MAX_SUBCOMMANDS);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
					return -1;
				}
				if (pri->debug & PRI_DEBUG_APDU)
				{
					pri_message(pri, "ROSE %i:   Handle CcExecposible\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				c_subcmd->cmd = CMD_CC_EXECPOSIBLE_INV;
				res = rose_cc_optional_arg_decode(pri, call, &c_subcmd->cc_execposible_inv.cc_optional_arg, comp, len-i);
				return res;
			case QSIG_CC_PATHRESERVE:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:  invoke CcPathreserve - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case QSIG_CC_RINGOUT:
				c_subcmd = get_ptr_subcommand(&call->subcmds);
				if (!c_subcmd) {
					if (pri->debug & PRI_DEBUG_APDU)
						pri_message(pri, "ROSE %i:  invoke CcRingout - more than %d facilities !\n", operation_tag, MAX_SUBCOMMANDS);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
					return -1;
				}
				if (pri->debug & PRI_DEBUG_APDU)
				{
					pri_message(pri, "ROSE %i:   Handle CcRingout\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				c_subcmd->cmd = CMD_CC_RINGOUT_INV;
				res = rose_cc_ringout_inv_decode(pri, &c_subcmd->cc_ringout_inv.cc_extension, comp, len-i);
				return res;
			case QSIG_CC_SUSPEND:
				if (pri->debug & PRI_DEBUG_APDU)
				{
					pri_message(pri, "ROSE %i:   Handle CcSuspend\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return 0;
			case QSIG_CC_RESUME:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:  invoke CcResume - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			default:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "!! Unable to handle ROSE operation %d", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return -1;
			}
		} else { /* pri->switchtype == PRI_SWITCH_QSIG */

			switch (operation_tag) {
			case SS_CNID_CALLINGNAME:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "ROSE %i:   Handle CallingName\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return rose_calling_name_decode(pri, call, comp, len-i);
			case ROSE_CALL_TRANSFER_IDENTIFY:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   CallTransferIdentify - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_CALL_TRANSFER_ABANDON:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   CallTransferAbandon - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_CALL_TRANSFER_INITIATE:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   CallTransferInitiate - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_CALL_TRANSFER_SETUP:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   CallTransferSetup - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_CALL_TRANSFER_ACTIVE:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   Handle CallTransferActive\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return rose_call_transfer_active_decode(pri, call, comp, len-i);
			case ROSE_CALL_TRANSFER_COMPLETE:
				if (pri->debug & PRI_DEBUG_APDU)
				{
					pri_message(pri, "ROSE %i:   Handle CallTransferComplete\n", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return rose_call_transfer_complete_decode(pri, call, comp, len-i);
			case ROSE_CALL_TRANSFER_UPDATE:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   CallTransferUpdate - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_SUBADDRESS_TRANSFER:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "ROSE %i:   SubaddressTransfer - not handled!\n", operation_tag);
				dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				return -1;
			case ROSE_DIVERTING_LEG_INFORMATION2:
				if (pri->debug & PRI_DEBUG_APDU)
					pri_message(pri, "  Handle DivertingLegInformation2\n");
				return rose_diverting_leg_information2_decode(pri, call, comp, len-i);
			case ROSE_AOC_NO_CHARGING_INFO_AVAILABLE:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "ROSE %i: AOC No Charging Info Available - not handled!", operation_tag);
					dump_apdu (pri, comp->data, comp->len);
				}
				return -1;
			case ROSE_AOC_CHARGING_REQUEST:
				return aoc_aoce_charging_request_decode(pri, call, (u_int8_t *)comp, comp->len + 2);
			case ROSE_AOC_AOCS_CURRENCY:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "ROSE %i: AOC-S Currency - not handled!", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return -1;
			case ROSE_AOC_AOCS_SPECIAL_ARR:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "ROSE %i: AOC-S Special Array - not handled!", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return -1;
			case ROSE_AOC_AOCD_CURRENCY:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "ROSE %i: AOC-D Currency - not handled!", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return -1;
			case ROSE_AOC_AOCD_CHARGING_UNIT:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "ROSE %i: AOC-D Charging Unit - not handled!", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return -1;
			case ROSE_AOC_AOCE_CURRENCY:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "ROSE %i: AOC-E Currency - not handled!", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return -1;
			case ROSE_AOC_AOCE_CHARGING_UNIT:
				return aoc_aoce_charging_unit_decode(pri, call, (u_int8_t *)comp, comp->len + 2);
				if (0) { /* the following function is currently not used - just to make the compiler happy */
					aoc_aoce_charging_unit_encode(pri, call, call->aoc_units); /* use this function to forward the aoc-e on a bridged channel */
					return 0;
				}
			case ROSE_AOC_IDENTIFICATION_OF_CHARGE:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "ROSE %i: AOC Identification Of Charge - not handled!", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return -1;
			case SS_ANFPR_PATHREPLACEMENT:
				/* Clear Queue */
				res = pri_call_apdu_queue_cleanup(call->bridged_call);
				if (res) {
					pri_message(pri, "Could not Clear queue ADPU\n");
					return -1;
				}
				anfpr_pathreplacement_respond(pri, call, ie);
				break;
			default:
				if (pri->debug & PRI_DEBUG_APDU) {
					pri_message(pri, "!! Unable to handle ROSE operation %d", operation_tag);
					dump_apdu (pri, (u_int8_t *)comp, comp->len + 2);
				}
				return -1;
			}
		}
	} while(0);
	
	return -1;
}

int pri_call_apdu_queue(q931_call *call, int messagetype, void *apdu, int apdu_len, void (*function)(void *data), void *data)
{
	struct apdu_event *cur = NULL;
	struct apdu_event *new_event = NULL;

	if (!call || !messagetype || !apdu || (apdu_len < 1) || (apdu_len > 255))
		return -1;

	if (!(new_event = calloc(1, sizeof(*new_event)))) {
		pri_error(call->pri, "!! Malloc failed!\n");
		return -1;
	}

	new_event->message = messagetype;
	new_event->callback = function;
	new_event->data = data;
	memcpy(new_event->apdu, apdu, apdu_len);
	new_event->apdu_len = apdu_len;
	
	if (call->apdus) {
		cur = call->apdus;
		while (cur->next) {
			cur = cur->next;
		}
		cur->next = new_event;
	} else
		call->apdus = new_event;

	return 0;
}

int pri_call_apdu_queue_cleanup(q931_call *call)
{
	struct apdu_event *cur_event = NULL, *free_event = NULL;

	if (call && call->apdus) {
		cur_event = call->apdus;
		while (cur_event) {
			/* TODO: callbacks, some way of giving return res on status of apdu */
			free_event = cur_event;
			cur_event = cur_event->next;
			free(free_event);
		}
		call->apdus = NULL;
	}

	return 0;
}

/* ===== Begin Call Completion Supplementary Service (ETS 300 366/ECMA 186) ===== */
/* operationId e.g. QSIG_CC_CCBS_REQUEST and QSIG_CC_CCNR_REQUEST */
static int add_qsigCcRequestArg_facility_ie (struct pri *pri, q931_call *c)
{
	int size = 0;
	int i = 0;
	unsigned char buffer[256];
	struct rose_component *comp = NULL, *compstk[10];
	int compsp = 0;
	u_int8_t operationId = c->ccoperation;
	char *numberA = c->callernum;
	char *numberB = c->callednum;
 
	/* 1 Byte 	   0x80 | 0x1F = 9F	 Protocol Profile (0x93 wäre altes QSIG oder DDS1) */
	buffer[i++] = (ASN1_CONTEXT_SPECIFIC | Q932_PROTOCOL_EXTENSIONS);

	/* Interpretation component */
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_NFE, buffer, i); /* 2. Byte NEtwork Facility Extension 0xAA = ASN1_CONTEXT_SPECIFIC(0x80) | (ASN1_CONSTRUCTOR 0x20)  0x0A (Tag laut Standard) */
	ASN1_PUSH(compstk, compsp, comp);

	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_0), buffer, i, 0);	/* (0x80, 0x01(len), 0x00) endPTNX */
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_2), buffer, i, 0);	/* (0x82, 0x01(len), 0x00) endPTNX */

	ASN1_FIXUP(compstk, compsp, buffer, i);

#if 0
	ASN1_ADD_BYTECOMP(comp, COMP_TYPE_INTERPRETATION, buffer, i, 0);   /* 0x8B, 0x01(len), 0x00 discard */
#endif
	ASN1_ADD_SIMPLE(comp, COMP_TYPE_INVOKE, buffer, i);			 /* 0xA1, 0xXX (len of Invoke Sequenz) invoke APDU */
	ASN1_PUSH(compstk, compsp, comp);

	/* Invoke ID */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, get_invokeid(pri));   /* InvokeID 0x02 0x01 0xXX */

	/*CcbsRequest ::= 40 or CcnrRequest ::= 27 */
	/* Operation ID: CCBS/CCNR */
	ASN1_ADD_BYTECOMP(comp, ASN1_INTEGER, buffer, i, operationId); /* 0x02 0x01 0x28/0x1b */

	/* ccbs/ccnr request argument */
	/* PresentedNumberUnscreened */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONSTRUCTOR | ASN1_SEQUENCE), buffer, i); /*0x30 0xXX (len)*/
	ASN1_PUSH(compstk, compsp, comp);
	/* (0xA0, 0x01(len)) presentationAlloweAddress  [0] PartyNumber */
	/* (0xA1, 0xXX (len) publicPartyNumber [1] IMPLICIT PublicPartyNumber */
	/* (0x0A, 0x01, 0x00 ) type of public party number = subscriber number */
	/* (0x12, 0xXX (len), 0xXX .. 0xXX) numeric string */
	size = rose_presented_number_unscreened_encode(pri, &buffer[i], PRES_ALLOWED, Q932_TON_UNKNOWN, numberA);
	if (size < 0)
		return -1;
	i += size;

	/* (0xA1, 0xXX (len) [1] IMPLICIT PublicPartyNumber */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	/* (0x0A, 0x01, 0x00 ) type of public party number = subscriber number */
	/* (0x12, 0xXX (len), 0xXX .. 0xXX) numeric string */
	size = rose_public_party_number_encode(pri, comp->data, 1, Q932_TON_UNKNOWN, numberB);
	if (size < 0)
		return -1;
	i += size;
	ASN1_FIXUP(compstk, compsp, buffer, i);

	/* (0x40, 0xXX (len), 0xXX .. 0xXX) pSS1InfoElement */
	ASN1_ADD_SIMPLE(comp, (ASN1_APPLICATION | ASN1_TAG_0 ), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	buffer[i++] = (0x04);	/*  add Bearer Capability IE */
	buffer[i++] = (0x03);	/* len*/
	buffer[i++] = (0x80);	/* ETSI Standard, Speech */
	buffer[i++] = (0x90);	/* circuit mode, 64kbit/s */
	buffer[i++] = (0xa3);	/* level1 protocol, a-law */
	ASN1_FIXUP(compstk, compsp, buffer, i);
#if 0
	/* can-retain-service [12] IMPLICIT BOOLEAN DEFAULT FALSE,*/
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_12), buffer, i, 0);   /* 0x1C, 0x01(len), 0x00 false */
#endif
	/* retain-sig-connection [13] IMPLICIT BOOLEAN OPTIONAL, --TRUE: sign. connection to be retained */	 
	ASN1_ADD_BYTECOMP(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_TAG_13), buffer, i, 1);   /* 0x1D, 0x01(len), 0x01 true */

	ASN1_FIXUP(compstk, compsp, buffer, i);

	ASN1_FIXUP(compstk, compsp, buffer, i);

	if (pri_call_apdu_queue(c, Q931_SETUP, buffer, i, NULL, NULL))
		return -1;
	
	return 0;
}
/* ===== End Call Completion Supplementary Service (ETS 300 366/ECMA 186) ===== */

int pri_call_add_standard_apdus(struct pri *pri, q931_call *call)
{
	if (!pri->sendfacility)
		return 0;

	if (pri->switchtype == PRI_SWITCH_QSIG) { /* For Q.SIG it does network and cpe operations */
		if (call->redirectingnum[0])
			rose_diverting_leg_information2_encode(pri, call);
		add_callername_facility_ies(pri, call, 1);
		if (call->ccoperation) {
			switch(call->ccoperation) {
			case 0:
				break;
			case QSIG_CC_CCBSREQUEST:
			case QSIG_CC_CCNRREQUEST:
				add_qsigCcRequestArg_facility_ie(pri, call);
				break;
			case QSIG_CC_RINGOUT:
				add_qsigCcInv_facility_ie(pri, call, Q931_SETUP);
				break;
			default:
				break;
			}
		}
		return 0;
	}

#if 0
	if (pri->localtype == PRI_NETWORK) {
		switch (pri->switchtype) {
			case PRI_SWITCH_NI2:
				add_callername_facility_ies(pri, call, 0);
				break;
			default:
				break;
		}
		return 0;
	} else if (pri->localtype == PRI_CPE) {
		switch (pri->switchtype) {
			case PRI_SWITCH_NI2:
				add_callername_facility_ies(pri, call, 1);
				break;
			default:
				break;
		}
		return 0;
	}
#else
	if (pri->switchtype == PRI_SWITCH_NI2)
		add_callername_facility_ies(pri, call, (pri->localtype == PRI_CPE));
#endif

	if ((pri->switchtype == PRI_SWITCH_DMS100) && (pri->localtype == PRI_CPE)) {
		add_dms100_transfer_ability_apdu(pri, call);
	}



	return 0;
}

int qsig_initiate_diverting_leg_information1(struct pri *pri, q931_call *call)
{
	rose_diverting_leg_information1_encode(pri, call);

	if (q931_facility(pri, call)) {
		pri_message(pri, "Could not schedule facility message for divertingLegInfo1\n");
		return -1;
	}

	return 0;
}

int qsig_initiate_call_transfer_complete(struct pri *pri, q931_call *call)
{
	rose_call_transfer_complete_encode(pri, call);

	if (q931_facility(pri, call)) {
		pri_message(pri, "Could not schedule facility message for callTransferComplete\n");
		return -1;
	}

	return 0;
}
