/* Definitions for network-related stuff.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */

#ifndef INCLUDED_NET_H
#define INCLUDED_NET_H

#ifdef SYSLOG_INET
#include <netinet/in.h>
#include <sys/socket.h> /* this is needed on HP UX -- rgerhards, 2008-03-04 */

#define   F_SET(where, flag) (where)|=(flag)
#define F_ISSET(where, flag) ((where)&(flag))==(flag)
#define F_UNSET(where, flag) (where)&=~(flag)

#define ADDR_NAME 0x01 /* address is hostname wildcard) */
#define ADDR_PRI6 0x02 /* use IPv6 address prior to IPv4 when resolving */

#ifdef BSD
#	ifndef _KERNEL
#		define s6_addr32 __u6_addr.__u6_addr32
#	endif
#endif

struct NetAddr {
  uint8_t flags;
  union {
    struct sockaddr *NetAddr;
    char *HostWildcard;
  } addr;
};

#ifndef SO_BSDCOMPAT
	/* this shall prevent compiler errors due to undfined name */
#	define SO_BSDCOMPAT 0
#endif


/* IPv6 compatibility layer for older platforms
 * We need to handle a few things different if we are running
 * on an older platform which does not support all the glory
 * of IPv6. We try to limit toll on features and reliability,
 * but obviously it is better to run rsyslog on a platform that
 * supports everything...
 * rgerhards, 2007-06-22
 */
#ifndef AI_NUMERICSERV
#  define AI_NUMERICSERV 0
#endif


#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
#define SALEN(sa) ((sa)->sa_len)
#else
static inline size_t SALEN(struct sockaddr *sa) {
	switch (sa->sa_family) {
	case AF_INET:  return (sizeof (struct sockaddr_in));
	case AF_INET6: return (sizeof (struct sockaddr_in6));
	default:       return 0;
	}
}
#endif

struct AllowedSenders {
	struct NetAddr allowedSender; /* ip address allowed */
	uint8_t SignificantBits;      /* defines how many bits should be discarded (eqiv to mask) */
	struct AllowedSenders *pNext;
};


/* interfaces */
BEGINinterface(net) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*cvthname)(struct sockaddr_storage *f, uchar *pszHost, uchar *pszHostFQDN);
	/* things to go away after proper modularization */
	rsRetVal (*addAllowedSenderLine)(char* pName, uchar** ppRestOfConfLine);
	void (*PrintAllowedSenders)(int iListToPrint);
	void (*clearAllowedSenders) ();
	void (*debugListenInfo)(int fd, char *type);
	int *(*create_udp_socket)(uchar *hostname, uchar *LogPort, int bIsServer);
	void (*closeUDPListenSockets)(int *finet);
	int (*isAllowedSender)(struct AllowedSenders *pAllowRoot, struct sockaddr *pFrom, const char *pszFromHost);
	int (*should_use_so_bsdcompat)(void);
	/* data memebers - these should go away over time... TODO */
	int    *pACLAddHostnameOnFail; /* add hostname to acl when DNS resolving has failed */
	int    *pACLDontResolve;       /* add hostname to acl instead of resolving it to IP(s) */
	struct AllowedSenders *pAllowedSenders_UDP;
	struct AllowedSenders *pAllowedSenders_TCP;
	struct AllowedSenders *pAllowedSenders_GSS;
ENDinterface(net)
#define netCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */

/* prototypes */
PROTOTYPEObj(net);

/* the name of our library binary */
#define LM_NET_FILENAME "lmnet"

#endif /* #ifdef SYSLOG_INET */
#endif /* #ifndef INCLUDED_NET_H */
