/* This is the header for the output channel code of rsyslog.
 * Please see syslogd.c for license information.
 * This code is placed under the GPL.
 * begun 2005-06-21 rgerhards
 */
struct outchannel {
	struct outchannel *pNext;
	char *pszName;
	int iLenName;
	uchar *pszFileTemplate;
	off_t	uSizeLimit;
	uchar *cmdOnSizeLimit;
};

struct outchannel* ochConstruct(void);
struct outchannel *ochAddLine(char* pName, unsigned char** pRestOfConfLine);
struct outchannel *ochFind(char *pName, int iLenName);
void ochDeleteAll(void);
void ochPrintList(void);

/*
 * vi:set ai:
 */
