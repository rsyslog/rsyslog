#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "libestr.h"

void
readConfFile(FILE *fp, es_str_t **str)
{
	int c;
	char ln[10240];
	int len, i;
	int start;	/* start index of to be submitted text */
	char *fgetsRet;
	int bContLine = 0;

	*str = es_newStr(4096);

	while(fgets(ln, sizeof(ln), fp) != NULL) {
		len = strlen(ln);
		/* if we are continuation line, we need to drop leading WS */
		if(bContLine) {
			for(start = 0 ; start < len && isspace(ln[start]) ; ++start)
				/* JUST SCAN */;
		} else {
			start = 0;
		}
		for(i = len - 1 ; i >= start && isspace(ln[i]) ; --i)
			/* JUST SCAN */;
		if(i >= 0) {
			if(ln[i] == '\\') {
				--i;
				bContLine = 1;
			} else {
				bContLine = 0;
			}
			/* add relevant data to buffer */
			es_addBuf(str, ln+start, i+1 - start);
			if(!bContLine)
				es_addChar(str, '\n');
		}
	}
	/* indicate end of buffer to flex */
	es_addChar(str, '\0');
	es_addChar(str, '\0');
}
