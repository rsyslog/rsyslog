/* This is the header for template processing code of rsyslog.
 * Please see syslogd.c for license information.
 * This code is placed under the GPL.
 * begun 2004-11-17 rgerhards
 */
struct template {
	struct template *pNext;
	char *pszName;
	int iLenName;
	struct templateEntry *pEntryRoot;
	struct templateEntry *pEntryLast;
};

enum EntryTypes { UNDEFINED = 0, CONSTANT = 1, FIELD = 2 };

/* a specific parse entry */
struct templateEntry {
	struct templateEntry *pNext;
	enum EntryTypes eEntryType;
	union {
		char *pConstant;	/* pointer to constant value */
		char *pPropRepl;	/* pointer to property replacer string */
	};
};

struct template* tplConstruct(void);
struct template *tplAddLine(char* pName, char** pRestOfConfLine);
struct template *tplFind(char *pName, int iLenName);

/*
 * vi:set ai:
 */
