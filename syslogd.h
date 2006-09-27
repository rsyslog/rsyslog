/* common header for syslogd */
#if defined(__GLIBC__)
#define dprintf mydprintf
#endif /* __GLIBC__ */
void dprintf(char *, ...);
void logerror(char *type);
void logerrorSz(char *type, char *errMsg);
void logerrorInt(char *type, int iErr);

#include "rsyslog.h"
