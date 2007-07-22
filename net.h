/* Definitions for network-related stuff.
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */

#ifndef INCLUDED_NET_H
#define INCLUDED_NET_H

#ifdef SYSLOG_INET
#include <netinet/in.h>

#define   F_SET(where, flag) (where)|=(flag)
#define F_ISSET(where, flag) ((where)&(flag))==(flag)
#define F_UNSET(where, flag) (where)&=~(flag)

#define ADDR_NAME 0x01 /* address is hostname wildcard) */
#define ADDR_PRI6 0x02 /* use IPv6 address prior to IPv4 when resolving */

#ifdef BSD
#ifndef _KERNEL
#define s6_addr32 __u6_addr.__u6_addr32
#endif
#endif

struct NetAddr {
  uint8_t flags;
  union {
    struct sockaddr *NetAddr;
    char *HostWildcard;
  } addr;
};

#ifndef BSD
	int should_use_so_bsdcompat(void);
#else
#	define should_use_so_bsdcompat() 1
#endif	/* #ifndef BSD */

#ifndef SO_BSDCOMPAT
	/* this shall prevent compiler errors due to undfined name */
#	define SO_BSDCOMPAT 0
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

int cvthname(struct sockaddr_storage *f, uchar *pszHost, uchar *pszHostFQDN);

#endif /* #ifdef SYSLOG_INET */
#endif /* #ifndef INCLUDED_NET_H */
