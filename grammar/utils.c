#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libestr.h>
#include "utils.h"

void
readConfFile(FILE *fp, es_str_t **str)
{
	char ln[10240];
	int len, i;
	int start;	/* start index of to be submitted text */
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


struct cnfactlst*
cnfactlstNew(enum cnfactType actType, struct nvlst *lst, char *actLine)
{
	struct cnfactlst *actlst;

	if((actlst = malloc(sizeof(struct cnfactlst))) != NULL) {
		actlst->next = NULL;
		actlst->syslines = NULL;
		actlst->actType = actType;
		if(actType == CNFACT_V2)
			actlst->data.lst = lst;
		else
			actlst->data.legActLine = actLine;
	}
	return actlst;
}

struct cnfactlst*
cnfactlstAddSysline(struct cnfactlst* actlst, char *line)
{
	struct cnfcfsyslinelst *cflst;

	if((cflst = malloc(sizeof(struct cnfcfsyslinelst))) != NULL)   {
		cflst->next = NULL;
		cflst->line = line;
		if(actlst->syslines == NULL) {
			actlst->syslines = cflst;
		} else {
			cflst->next = actlst->syslines;
			actlst->syslines = cflst;
		}
	}
	return actlst;
}

void
cnfactlstDestruct(struct cnfactlst *actlst)
{
	struct cnfactlst *toDel;

	while(actlst != NULL) {
		toDel = actlst;
		actlst = actlst->next;
		if(toDel->actType == CNFACT_V2)
			nvlstDestruct(toDel->data.lst);
		else
			free(toDel->data.legActLine);
		free(toDel);
	}
	
}

static inline struct cnfcfsyslinelst*
cnfcfsyslinelstReverse(struct cnfcfsyslinelst *lst)
{
	struct cnfcfsyslinelst *curr, *prev;

	if(lst == NULL)
		return NULL;
	prev = lst;
	lst = lst->next;
	prev->next = NULL;
	while(lst != NULL) {
		curr = lst;
		lst = lst->next;
		curr->next = prev;
		prev = curr;
	}
	return prev;
}

struct cnfactlst*
cnfactlstReverse(struct cnfactlst *actlst)
{
	struct cnfactlst *curr, *prev;

	prev = actlst;
	actlst = actlst->next;
	prev->next = NULL;
	while(actlst != NULL) {
		curr = actlst;
		actlst = actlst->next;
		curr->syslines = cnfcfsyslinelstReverse(curr->syslines);
		curr->next = prev;
		prev = curr;
	}
	return prev;
}

void
cnfactlstPrint(struct cnfactlst *actlst)
{
	struct cnfcfsyslinelst *cflst;

	printf("---------- cnfactlst %p:\n", actlst);
	while(actlst != NULL) {
		if(actlst->actType == CNFACT_V2) {
			printf("V2 action type: ");
			nvlstPrint(actlst->data.lst);
		} else {
			printf("legacy action line: '%s'\n",
				actlst->data.legActLine);
		}
		for(  cflst = actlst->syslines
		    ; cflst != NULL ; cflst = cflst->next) {
			printf("cfsysline: '%s'\n", cflst->line);
		}
		actlst = actlst->next;
	}
	printf("----------\n");
}

struct cnfexpr*
cnfexprNew(int nodetype, struct cnfexpr *l, struct cnfexpr *r)
{
	struct cnfexpr *expr;
	if((expr = malloc(sizeof(struct cnfexpr))) != NULL) {
		expr->nodetype = nodetype;
		expr->l = l;
		expr->r = r;
	}
	return expr;
}


inline static void
doIndent(indent)
{
	int i;
	for(i = 0 ; i < indent ; ++i)
		printf("  ");
}
void
cnfexprPrint(struct cnfexpr *expr, int indent)
{
	//printf("expr %p, indent %d, type '%c'\n", expr, indent, expr->nodetype);
	switch(expr->nodetype) {
	case 'N':
		doIndent(indent);
		printf("%lld\n", ((struct cnfnumval*)expr)->val);
		break;
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case 'M':
		if(expr->l != NULL)
			cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf("%c\n", (char) expr->nodetype);
		cnfexprPrint(expr->r, indent+1);
		break;
	default:
		printf("error: unknown nodetype\n");
		break;
	}
}

struct cnfnumval*
cnfnumvalNew(long long val)
{
	struct cnfnumval *numval;
	if((numval = malloc(sizeof(struct cnfnumval))) != NULL) {
		numval->nodetype = 'N';
		numval->val = val;
	}
	return numval;
}

struct cnfstringval*
cnfstringvalNew(es_str_t *estr)
{
	struct cnfstringval *strval;
	if((strval = malloc(sizeof(struct cnfstringval))) != NULL) {
		strval->nodetype = 'S';
		strval->estr = estr;
	}
	return strval;
}

/* debug helper */
void
cstrPrint(char *text, es_str_t *estr)
{
	char *str;
	str = es_str2cstr(estr, NULL);
	printf("%s%s", text, str);
	free(str);
}

