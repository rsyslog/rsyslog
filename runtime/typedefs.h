/* This defines some types commonly used. Do NOT include any other
 * rsyslog runtime file.
 *
 * Begun 2010-11-25 RGerhards
 *
 * Copyright (C) 2005-2008 by Rainer Gerhards and Adiscon GmbH
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#ifndef INCLUDED_TYPEDEFS_H
#define INCLUDED_TYPEDEFS_H

/* some universal fixed size integer defines ... */
typedef long long int64;
typedef long long unsigned uint64;
typedef int64 number_t; /* type to use for numbers - TODO: maybe an autoconf option? */
typedef char intTiny; 	/* 0..127! */
typedef unsigned char uintTiny;	/* 0..255! */

/* define some base data types */

typedef unsigned char uchar;/* get rid of the unhandy "unsigned char" */
typedef struct aUsrp_s aUsrp_t;
typedef struct thrdInfo thrdInfo_t;
typedef struct obj_s obj_t;
typedef struct ruleset_s ruleset_t;
typedef struct rule_s rule_t;
//typedef struct filed selector_t;/* TODO: this so far resides in syslogd.c, think about modularization */
typedef struct NetAddr netAddr_t;
typedef struct netstrms_s netstrms_t;
typedef struct netstrm_s netstrm_t;
typedef struct nssel_s nssel_t;
typedef struct nspoll_s nspoll_t;
typedef enum nsdsel_waitOp_e nsdsel_waitOp_t;
typedef struct nsd_ptcp_s nsd_ptcp_t;
typedef struct nsd_gtls_s nsd_gtls_t;
typedef struct nsd_gsspi_s nsd_gsspi_t;
typedef struct nsd_nss_s nsd_nss_t;
typedef struct nsdsel_ptcp_s nsdsel_ptcp_t;
typedef struct nsdsel_gtls_s nsdsel_gtls_t;
typedef struct nsdpoll_ptcp_s nsdpoll_ptcp_t;
typedef struct wti_s wti_t;
typedef struct msg msg_t;
typedef struct queue_s qqueue_t;
typedef struct prop_s prop_t;
typedef struct interface_s interface_t;
typedef struct objInfo_s objInfo_t;
typedef enum rsRetVal_ rsRetVal; /**< friendly type for global return value */
typedef rsRetVal (*errLogFunc_t)(uchar*); /* this is a trick to store a function ptr to a function returning a function ptr... */
typedef struct permittedPeers_s permittedPeers_t; /* this should go away in the long term -- rgerhards, 2008-05-19 */
typedef struct permittedPeerWildcard_s permittedPeerWildcard_t; /* this should go away in the long term -- rgerhards, 2008-05-19 */
typedef struct tcpsrv_s tcpsrv_t;
typedef struct tcps_sess_s tcps_sess_t;
typedef struct strmsrv_s strmsrv_t;
typedef struct strms_sess_s strms_sess_t;
typedef struct vmstk_s vmstk_t;
typedef struct batch_obj_s batch_obj_t;
typedef struct batch_s batch_t;
typedef struct wtp_s wtp_t;
typedef struct modInfo_s modInfo_t;
typedef struct parser_s parser_t;
typedef struct parserList_s parserList_t;
typedef struct strgen_s strgen_t;
typedef struct strgenList_s strgenList_t;
typedef struct statsobj_s statsobj_t;
typedef struct nsd_epworkset_s nsd_epworkset_t;
typedef struct templates_s templates_t;
typedef struct queuecnf_s queuecnf_t;
typedef struct rulesets_s rulesets_t;
typedef struct globals_s globals_t;
typedef struct defaults_s defaults_t;
typedef struct actions_s actions_t;
typedef struct rsconf_s rsconf_t;
typedef struct cfgmodules_s cfgmodules_t;
typedef struct cfgmodules_etry_s cfgmodules_etry_t;
typedef struct outchannels_s outchannels_t;
typedef struct modConfData_s modConfData_t;
typedef struct instanceConf_s instanceConf_t;
typedef struct ratelimit_s ratelimit_t;
typedef struct action_s action_t;
typedef int rs_size_t; /* we do never need more than 2Gig strings, signed permits to
			* use -1 as a special flag. */
typedef rsRetVal (*prsf_t)(struct vmstk_s*, int);	/* pointer to a RainerScript function */
typedef uint64 qDeqID;	/* queue Dequeue order ID. 32 bits is considered dangerously few */

typedef struct tcpLstnPortList_s tcpLstnPortList_t; // TODO: rename?
typedef struct strmLstnPortList_s strmLstnPortList_t; // TODO: rename?

/* under Solaris (actually only SPARC), we need to redefine some types
 * to be void, so that we get void* pointers. Otherwise, we will see
 * alignment errors.
 */
#ifdef OS_SOLARIS
	typedef void * obj_t_ptr;
	typedef void nsd_t;
	typedef void nsdsel_t;
	typedef void nsdpoll_t;
#else
	typedef obj_t *obj_t_ptr;
	typedef obj_t nsd_t;
	typedef obj_t nsdsel_t;
	typedef obj_t nsdpoll_t;
#endif


#ifdef __hpux
typedef unsigned int u_int32_t; /* TODO: is this correct? */
typedef int socklen_t;
#endif

typedef struct epoll_event epoll_event_t;

typedef char sbool;		/* (small bool) I intentionally use char, to keep it slim so that many fit into the CPU cache! */

/* settings for flow control
 * TODO: is there a better place for them? -- rgerhards, 2008-03-14
 */
typedef enum {
	eFLOWCTL_NO_DELAY = 0,		/**< UDP and other non-delayable sources */
	eFLOWCTL_LIGHT_DELAY = 1,	/**< some light delay possible, but no extended period of time */
	eFLOWCTL_FULL_DELAY = 2	/**< delay possible for extended period of time */
} flowControl_t;

/* filter operations */
typedef enum {
	FIOP_NOP = 0,		/* do not use - No Operation */
	FIOP_CONTAINS  = 1,	/* contains string? */
	FIOP_ISEQUAL  = 2,	/* is (exactly) equal? */
	FIOP_STARTSWITH = 3,	/* starts with a string? */
	FIOP_REGEX = 4,		/* matches a (BRE) regular expression? */
	FIOP_EREREGEX = 5,	/* matches a ERE regular expression? */
	FIOP_ISEMPTY = 6	/* string empty <=> strlen(s) == 0 ?*/
} fiop_t;

/* types of configuration handlers
 */
typedef enum cslCmdHdlrType {
	eCmdHdlrInvalid = 0,		/* invalid handler type - indicates a coding error */
	eCmdHdlrCustomHandler,		/* custom handler, just call handler function */
	eCmdHdlrUID,
	eCmdHdlrGID,
	eCmdHdlrBinary,
	eCmdHdlrFileCreateMode,
	eCmdHdlrInt,
	eCmdHdlrNonNegInt,
	eCmdHdlrPositiveInt,
	eCmdHdlrSize,
	eCmdHdlrGetChar,
	eCmdHdlrFacility,
	eCmdHdlrSeverity,
	eCmdHdlrGetWord,
	eCmdHdlrString,
	eCmdHdlrArray,
	eCmdHdlrQueueType,
	eCmdHdlrGoneAway		/* statment existed, but is no longer supported */
} ecslCmdHdrlType;


/* the next type describes $Begin .. $End block object types
 */
typedef enum cslConfObjType {
	eConfObjGlobal = 0,	/* global directives */
	eConfObjAction,		/* action-specific directives */
	/* now come states that indicate that we wait for a block-end. These are
	 * states that permit us to do some safety checks and they hopefully ease
	 * migration to a "real" parser/grammar.
	 */
	eConfObjActionWaitEnd,
	eConfObjAlways		/* always valid, very special case (guess $End only!) */
} ecslConfObjType;


/* multi-submit support.
 * This is done via a simple data structure, which holds the number of elements
 * as well as an array of to-be-submitted messages.
 * rgerhards, 2009-06-16
 */
typedef struct multi_submit_s multi_submit_t;
struct multi_submit_s {
	short	maxElem;	/* maximum number of Elements */
	short	nElem;		/* current number of Elements, points to the next one FREE */
	msg_t	**ppMsgs;
};

#endif /* multi-include protection */
/* vim:set ai:
 */
