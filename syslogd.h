/* common header for syslogd */
#if defined(__GLIBC__)
#define dprintf mydprintf
#endif /* __GLIBC__ */
void dprintf(char *, ...);

#include "rsyslog.h"
