/*
 *  FIXME: All network stuff should go here (and to net.c)
 */

//#ifdef SYSLOG_INET

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

//#endif
