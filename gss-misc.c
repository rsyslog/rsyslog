#include "config.h"
#if defined(SYSLOG_INET) && defined(USE_GSSAPI)
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fnmatch.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#ifdef USE_PTHREADS
#include <pthread.h>
#else
#include <fcntl.h>
#endif
#include <gssapi.h>
#include "syslogd.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "net.h"
#include "omfwd.h"
#include "template.h"
#include "msg.h"
#include "tcpsyslog.h"
#include "module-template.h"
#include "gss-misc.h"


static void display_status_(char *m, OM_uint32 code, int type)
{
	OM_uint32 maj_stat, min_stat, msg_ctx = 0;
	gss_buffer_desc msg;

	do {
		maj_stat = gss_display_status(&min_stat, code, type, GSS_C_NO_OID, &msg_ctx, &msg);
		if (maj_stat != GSS_S_COMPLETE) {
			logerrorSz("GSS-API error in gss_display_status called from <%s>\n", m);
			break;
		} else {
			char buf[1024];
			snprintf(buf, sizeof(buf), "GSS-API error %s: %s\n", m, (char *) msg.value);
			buf[sizeof(buf)/sizeof(char) - 1] = '\0';
			logerror(buf);
		}
		if (msg.length != 0)
			gss_release_buffer(&min_stat, &msg);
	} while (msg_ctx);
}


void display_status(char *m, OM_uint32 maj_stat, OM_uint32 min_stat)
{
	display_status_(m, maj_stat, GSS_C_GSS_CODE);
	display_status_(m, min_stat, GSS_C_MECH_CODE);
}


void display_ctx_flags(OM_uint32 flags)
{
    if (flags & GSS_C_DELEG_FLAG)
	dbgprintf("GSS_C_DELEG_FLAG\n");
    if (flags & GSS_C_MUTUAL_FLAG)
	dbgprintf("GSS_C_MUTUAL_FLAG\n");
    if (flags & GSS_C_REPLAY_FLAG)
	dbgprintf("GSS_C_REPLAY_FLAG\n");
    if (flags & GSS_C_SEQUENCE_FLAG)
	dbgprintf("GSS_C_SEQUENCE_FLAG\n");
    if (flags & GSS_C_CONF_FLAG)
	dbgprintf("GSS_C_CONF_FLAG\n");
    if (flags & GSS_C_INTEG_FLAG)
	dbgprintf("GSS_C_INTEG_FLAG\n");
}


static int read_all(int fd, char *buf, unsigned int nbyte)
{
    int     ret;
    char   *ptr;
    fd_set  rfds;
    struct timeval tv;

    for (ptr = buf; nbyte; ptr += ret, nbyte -= ret) {
	    FD_ZERO(&rfds);
	    FD_SET(fd, &rfds);
	    tv.tv_sec = 1;
	    tv.tv_usec = 0;

	    if ((ret = select(FD_SETSIZE, &rfds, NULL, NULL, &tv)) <= 0
		|| !FD_ISSET(fd, &rfds))
		    return ret;
	    ret = recv(fd, ptr, nbyte, 0);
	    if (ret < 0) {
		    if (errno == EINTR)
			    continue;
		    return (ret);
	    } else if (ret == 0) {
		    return (ptr - buf);
	    }
    }

    return (ptr - buf);
}


static int write_all(int fd, char *buf, unsigned int nbyte)
{
    int     ret;
    char   *ptr;

    for (ptr = buf; nbyte; ptr += ret, nbyte -= ret) {
	ret = send(fd, ptr, nbyte, 0);
	if (ret < 0) {
	    if (errno == EINTR)
		continue;
	    return (ret);
	} else if (ret == 0) {
	    return (ptr - buf);
	}
    }

    return (ptr - buf);
}


int recv_token(int s, gss_buffer_t tok)
{
	int ret;
	unsigned char lenbuf[4];
	unsigned int len;

	ret = read_all(s, (char *) lenbuf, 4);
	if (ret < 0) {
		logerror("GSS-API error reading token length");
		return -1;
	} else if (!ret) {
		return 0;
	} else if (ret != 4) {
		logerror("GSS-API error reading token length");
		return -1;
	}

	len = ((lenbuf[0] << 24)
	       | (lenbuf[1] << 16)
	       | (lenbuf[2] << 8)
	       | lenbuf[3]);
	tok->length = ntohl(len);

	tok->value = (char *) malloc(tok->length ? tok->length : 1);
	if (tok->length && tok->value == NULL) {
		logerror("Out of memory allocating token data\n");
		return -1;
	}

	ret = read_all(s, (char *) tok->value, tok->length);
	if (ret < 0) {
		logerror("GSS-API error reading token data");
		free(tok->value);
		return -1;
	} else if (ret != (int) tok->length) {
		logerror("GSS-API error reading token data");
		free(tok->value);
		return -1;
	}

	return 1;
}


int send_token(int s, gss_buffer_t tok)
{
	int     ret;
	unsigned char lenbuf[4];
	unsigned int len;

	if (tok->length > 0xffffffffUL)
		abort();  /* TODO: we need to reconsider this, abort() is not really a solution - degrade, but keep running */
	len = htonl(tok->length);
	lenbuf[0] = (len >> 24) & 0xff;
	lenbuf[1] = (len >> 16) & 0xff;
	lenbuf[2] = (len >> 8) & 0xff;
	lenbuf[3] = len & 0xff;

	ret = write_all(s, (char *) lenbuf, 4);
	if (ret < 0) {
		logerror("GSS-API error sending token length");
		return -1;
	} else if (ret != 4) {
		logerror("GSS-API error sending token length");
		return -1;
	}

	ret = write_all(s, tok->value, tok->length);
	if (ret < 0) {
		logerror("GSS-API error sending token data");
		return -1;
	} else if (ret != (int) tok->length) {
		logerror("GSS-API error sending token data");
		return -1;
	}

	return 0;
}

#endif /* #if defined(SYSLOG_INET) && defined(USE_GSSAPI) */
