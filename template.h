/* This is the header for template processing code of rsyslog.
 * Please see syslogd.c for license information.
 * This code is placed under the GPL.
 * begun 2004-11-17 rgerhards
 */
struct template {
	char *pszName;
	char *pszTemplate;
};

struct template* tplConstruct(void);

/*
 * vi:set ai:
 */
