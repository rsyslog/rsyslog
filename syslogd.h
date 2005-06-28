/* common header for syslogd */
#if defined(__GLIBC__)
#define dprintf mydprintf
#endif /* __GLIBC__ */
void dprintf(char *, ...);

#ifndef	NOLARGEFILE
#	define _GNU_SOURCE
#	define _LARGEFILE_SOURCE  
#	define _LARGEFILE64_SOURCE  
#	define _FILE_OFFSET_BITS 64
#endif
