#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libestr.h>
#include "utils.h"

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

struct nvlst*
nvlstNew(es_str_t *name, es_str_t *value)
{
	struct nvlst *lst;

	if((lst = malloc(sizeof(struct nvlst))) != NULL) {
		lst->next = NULL;
		lst->name = name;
		lst->value = value;
	}

	return lst;
}

void
nvlstDestruct(struct nvlst *lst)
{
	struct nvlst *toDel;

	while(lst != NULL) {
		toDel = lst;
		lst = lst->next;
		es_deleteStr(toDel->name);
		es_deleteStr(toDel->value);
		free(toDel);
	}
}


void
nvlstPrint(struct nvlst *lst)
{
	char *name, *value;
	printf("nvlst %p:\n", lst);
	while(lst != NULL) {
		name = es_str2cstr(lst->name, NULL);
		value = es_str2cstr(lst->value, NULL);
		printf("\tname: '%s', value '%s'\n", name, value);
		free(name);
		free(value);
		lst = lst->next;
	}
}

struct cnfobj*
cnfobjNew(enum cnfobjType objType, struct nvlst *lst)
{
	struct cnfobj *o;

	if((o = malloc(sizeof(struct nvlst))) != NULL) {
		o->objType = objType;
		o->nvlst = lst;
	}

	return o;
}

void
cnfobjDestruct(struct cnfobj *o)
{
	if(o != NULL) {
		nvlstDestruct(o->nvlst);
		free(o);
	}
}

void
cnfobjPrint(struct cnfobj *o)
{
	printf("obj: '%s'\n", cnfobjType2str(o->objType));
	nvlstPrint(o->nvlst);
}
