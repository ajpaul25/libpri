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

/*
 * This program tests libpri call reception using a zaptel interface.
 * Its state machines are setup for RECEIVING CALLS ONLY, so if you
 * are trying to both place and receive calls you have to a bit more.
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <linux/zaptel.h>
#include <zap.h>
#include "libpri.h"

#define PRI_DEF_NODETYPE	PRI_CPE
#define PRI_DEF_SWITCHTYPE	PRI_SWITCH_NI2

#define MAX_CHAN		32

static int offset = 0;

static void do_channel(ZAP *z)
{
	/* This is the part that runs on a given channel */
	zap_playf(z, "raw.ulaw", 0);
}

struct pri_chan {
	pid_t pid;
	int needhangup;
	int alreadyhungup;
	q931_call *call;
} chans[MAX_CHAN];

static int str2node(char *node)
{
	if (!strcasecmp(node, "cpe"))
		return PRI_CPE;
	if (!strcasecmp(node, "network"))
		return PRI_NETWORK;
	return -1;
}

static void chan_ended(int sig)
{
	int status;
	int x;
	struct rusage rusage;
	pid_t pid;
	pid = wait4(-1, &status, WNOHANG, &rusage);

	for (x=0;x<MAX_CHAN;x++) {
		if (pid == chans[x].pid) {
			printf("-- PID %d ended, channel %d\n", pid, x);
			chans[x].pid = 0;
			if (!chans[x].alreadyhungup) {
				/* It died, we need to hangup now */
				chans[x].needhangup = 1;
			} else {
				/* We've already been hungup, just clear it */
				chans[x].alreadyhungup = 0;
				chans[x].call = NULL;
			}
			return;
		}
	}

	if (pid > -1) {
		fprintf(stderr, "--!! Unknown PID %d exited\n", pid);
		return;
	}
}
static int str2switch(char *swtype)
{
	if (!strcasecmp(swtype, "ni2"))
		return PRI_SWITCH_NI2;
	if (!strcasecmp(swtype, "dms100"))
		return PRI_SWITCH_DMS100;
	if (!strcasecmp(swtype, "lucent5e"))
		return PRI_SWITCH_LUCENT5E;
	if (!strcasecmp(swtype, "att4ess"))
		return PRI_SWITCH_ATT4ESS;
	if (!strcasecmp(swtype, "euroisdn"))
		return PRI_SWITCH_EUROISDN_E1;
	return -1;
}

static void hangup_channel(int channo)
{
	if (chans[channo].pid) {

#if 0
		printf("Killing channel %d (pid = %d)\n", channo, chans[channo].pid);
#endif
		chans[channo].alreadyhungup = 1;
		kill(chans[channo].pid, SIGTERM);
	} else if (chans[channo].needhangup)
		chans[channo].needhangup = 0;
}

static void launch_channel(int channo)
{
	pid_t pid;
	ZAP *z;
	char ch[80];

	/* Make sure hangup state is reset */
	chans[channo].needhangup = 0;
	chans[channo].alreadyhungup = 0;

	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "--!! Unable to fork\n");
		chans[channo].needhangup = 1;
	}
	if (pid) {
		printf("-- Launching process %d to handle channel %d\n", pid, channo);
		chans[channo].pid = pid;
	} else {
		sprintf(ch, "%d", channo + offset);
		z = zap_open(ch, 0);
		if (z) {
			do_channel(z);
			exit(0);
		} else {
			fprintf(stderr, "--!! Unable to open channel %d\n", channo);
			exit(1);
		}
	}
	
}

static void start_channel(struct pri *pri, pri_event *e)
{
	int channo = e->ring.channel;

	/* Make sure it's a valid number */
	if ((channo >= MAX_CHAN) || (channo < 0)) { 
		fprintf(stderr, "--!! Channel %d is out of range\n", channo);
		return;
	}

	/* Make sure nothing is there */
	if (chans[channo].pid) {
		fprintf(stderr, "--!! Channel %d still has a call on it, ending it...\n", channo);
		hangup_channel(channo);
		/* Wait for it to die */
		while(chans[channo].pid)
			usleep(100);
	}

	/* Record call number */
	chans[channo].call = e->ring.call;

	/* Answer the line */
	pri_answer(pri, chans[channo].call, channo, 1);

	/* Launch a process to handle it */
	launch_channel(channo);

}

static void handle_pri_event(struct pri *pri, pri_event *e)
{
	switch(e->e) {
	case PRI_EVENT_DCHAN_UP:
		printf("-- D-Channel is now up!  :-)\n");
		break;
	case PRI_EVENT_DCHAN_DOWN:
		printf("-- D-Channel is now down! :-(\n");
		break;
	case PRI_EVENT_RESTART:
		printf("-- Restarting channel %d\n", e->restart.channel);
		hangup_channel(e->restart.channel);
		break;
	case PRI_EVENT_CONFIG_ERR:
		printf("-- Configuration error detected: %s\n", e->err.err);
		break;
	case PRI_EVENT_RING:
		printf("-- Ring on channel %d (from %s to %s), answering...\n", e->ring.channel, e->ring.callingnum, e->ring.callednum);
		start_channel(pri, e);
		break;
	case PRI_EVENT_HANGUP:
		printf("-- Hanging up channel %d\n", e->hangup.channel);
		hangup_channel(e->hangup.channel);
		break;
	case PRI_EVENT_RINGING:
	case PRI_EVENT_ANSWER:
		fprintf(stderr, "--!! What?  We shouldn't be making any calls...\n");
		break;
	case PRI_EVENT_HANGUP_ACK:
		/* Ignore */
		break;
	default:
		fprintf(stderr, "--!! Unknown PRI event %d\n", e->e);
	}
}

static int run_pri(int dfd, int swtype, int node)
{
	struct pri *pri;
	pri_event *e;
	struct timeval tv = {0,0}, *next;
	fd_set rfds, efds;
	int res,x;

	pri = pri_new(dfd, node, swtype);
	if (!pri) {
		fprintf(stderr, "Unable to create PRI\n");
		return -1;
	}
	for (;;) {
		
		/* Run the D-Channel */
		FD_ZERO(&rfds);
		FD_ZERO(&efds);
		FD_SET(dfd, &rfds);
		FD_SET(dfd, &efds);

		if ((next = pri_schedule_next(pri))) {
			gettimeofday(&tv, NULL);
			tv.tv_sec = next->tv_sec - tv.tv_sec;
			tv.tv_usec = next->tv_usec - tv.tv_usec;
			if (tv.tv_usec < 0) {
				tv.tv_usec += 1000000;
				tv.tv_sec -= 1;
			}
			if (tv.tv_sec < 0) {
				tv.tv_sec = 0;
				tv.tv_usec = 0;
			}
		}
		res = select(dfd + 1, &rfds, NULL, &efds, next ? &tv : NULL);
		e = NULL;

		if (!res) {
			e = pri_schedule_run(pri);
		} else if (res > 0) {
			e = pri_check_event(pri);
		} else if (errno == ELAST) {
			res = ioctl(dfd, ZT_GETEVENT, &x);
			printf("Got Zaptel event: %d\n", x);
		} else if (errno != EINTR) 
			fprintf(stderr, "Error (%d) on select: %s\n", ELAST, strerror(errno));

		if (e) {
			handle_pri_event(pri, e);
		}

		res = ioctl(dfd, ZT_GETEVENT, &x);

		if (!res && x) {
			fprintf(stderr, "Got event on PRI interface: %d\n", x);
		}

		/* Check for lines that need hangups */
		for (x=0;x<MAX_CHAN;x++)
			if (chans[x].needhangup) {
				chans[x].needhangup = 0;
				pri_release(pri, chans[x].call, PRI_CAUSE_NORMAL_CLEARING);
			}

	}
	return 0;
}

int main(int argc, char *argv[]) 
{
	int dfd;
	int swtype = PRI_DEF_SWITCHTYPE;
	int node = PRI_DEF_NODETYPE;
	struct zt_params p;
	if (argc < 2) {
		fprintf(stderr, "Usage: pritest <dchannel> [swtypetype] [nodetype]\n");
		exit(1);
	}
	dfd = open(argv[1], O_RDWR);
	if (dfd < 0) {
		fprintf(stderr, "Failed to open dchannel '%s': %s\n", argv[1], strerror(errno));
		exit(1);
	}
	if (ioctl(dfd, ZT_GET_PARAMS, &p)) {
		fprintf(stderr, "Unable to get parameters on '%s': %s\n", argv[1], strerror(errno));
		exit(1);
	}
	if ((p.sigtype != ZT_SIG_HDLCRAW) && (p.sigtype != ZT_SIG_HDLCFCS)) {
		fprintf(stderr, "%s is in %d signalling, not FCS HDLC or RAW HDLC mode\n", argv[1], p.sigtype);
		exit(1);
	}

	if (argc > 2) {
		swtype = str2switch(argv[2]);
		if (swtype < 0) {
			fprintf(stderr, "Valid switchtypes are: ni2, dms100, lucent5e, att4ess, and euroisdn\n");	
			exit(1);
		}
	}

	if (argc > 3) {
		node = str2node(argv[3]);
		if (node < 0) {
			fprintf(stderr, "Valid node types are: network and cpe\n");	
			exit(1);
		}
	}

	signal(SIGCHLD, chan_ended);

	if (run_pri(dfd, swtype, node))
		exit(1);
	exit(0);

	return 0;
}
