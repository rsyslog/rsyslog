/* This is the header for template processing code of rsyslog.
 * Please see syslogd.c for license information.
 * This code is placed under the GPL.
 * begun 2004-11-17 rgerhards
 */
struct template {
	struct template *pNext;
	char *pszName;
	int iLenName;
	int tpenElements; /* number of elements in templateEntry list */
	struct templateEntry *pEntryRoot;
	struct templateEntry *pEntryLast;
	/* following are options. All are 0/1 defined (either on or off).
	 * we use chars because they are faster than bit fields and smaller
	 * than short...
	 */
	char optFormatForSQL;	/* in text fields, escape quotes by double quotes  */
};

enum EntryTypes { UNDEFINED = 0, CONSTANT = 1, FIELD = 2 };
enum tplFormatTypes { tplFmtDefault = 0, tplFmtMySQLDate = 1 };
enum tplFormatCaseConvTypes { tplCaseConvNo = 0, tplCaseConvUpper = 1, tplCaseConvLower = 2 };

/* a specific parse entry */
struct templateEntry {
	struct templateEntry *pNext;
	enum EntryTypes eEntryType;
	union {
		struct {
			char *pConstant;	/* pointer to constant value */
			int iLenConstant;	/* its length */
		} constant;
		struct {
			char *pPropRepl;	/* pointer to property replacer string */
			unsigned iFromPos;	/* for partial strings only chars from this position ... */
			unsigned iToPos;	/* up to that one... */
			enum tplFormatTypes eDateFormat;
			enum tplFormatCaseConvTypes eCaseConv;
			struct {
			} options;		/* options as bit fields */
		} field;
	} data;
};

struct template* tplConstruct(void);
struct template *tplAddLine(char* pName, char** pRestOfConfLine);
struct template *tplFind(char *pName, int iLenName);
int tplGetEntryCount(struct template *pTpl);
void tplPrintList(void);

/*
 * vi:set ai:
 */
