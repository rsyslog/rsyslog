#ifndef	GSS_MISC_H_INCLUDED
#define	GSS_MISC_H_INCLUDED 1

#include <gssapi.h>

int recv_token(int s, gss_buffer_t tok);
int send_token(int s, gss_buffer_t tok);
void display_status(char *m, OM_uint32 maj_stat, OM_uint32 min_stat);
void display_ctx_flags(OM_uint32 flags);

#endif /* #ifndef GSS_MISC_H_INCLUDED */
