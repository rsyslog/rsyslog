#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libestr.h>
#include "utils.h"
#include "parserif.h"
#include "rscript.tab.h"

void
readConfFile(FILE *fp, es_str_t **str)
{
	char ln[10240];
	char buf[512];
	int lenBuf;
	int bWriteLineno = 0;
	int len, i;
	int start;	/* start index of to be submitted text */
	int bContLine = 0;
	int lineno = 0;

	*str = es_newStr(4096);

	while(fgets(ln, sizeof(ln), fp) != NULL) {
		++lineno;
		if(bWriteLineno) {
			bWriteLineno = 0;
			lenBuf = sprintf(buf, "PreprocFileLineNumber(%d)\n", lineno);
			es_addBuf(str, buf, lenBuf);
		}
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
				if(bContLine) /* write line number if we had cont line */
					bWriteLineno = 1;
				bContLine = 0;
			}
			/* add relevant data to buffer */
			es_addBuf(str, ln+start, i+1 - start);
		}
		if(!bContLine)
			es_addChar(str, '\n');
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
	prev = NULL;
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

	prev = NULL;
	while(actlst != NULL) {
		//printf("reversing: %s\n", actlst->data.legActLine);
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

	while(actlst != NULL) {
		printf("aclst %p: ", actlst);
		if(actlst->actType == CNFACT_V2) {
			printf("V2 action type: ");
			nvlstPrint(actlst->data.lst);
		} else {
			printf("legacy action line: '%s'\n",
				actlst->data.legActLine);
		}
		for(  cflst = actlst->syslines
		    ; cflst != NULL ; cflst = cflst->next) {
			printf("action:cfsysline: '%s'\n", cflst->line);
		}
		actlst = actlst->next;
	}
}

struct cnfexpr*
cnfexprNew(unsigned nodetype, struct cnfexpr *l, struct cnfexpr *r)
{
	struct cnfexpr *expr;

	/* optimize some constructs during parsing */
	if(nodetype == 'M' && r->nodetype == 'N') {
		((struct cnfnumval*)r)->val *= -1;
		expr = r;
		goto done;
	}

	if((expr = malloc(sizeof(struct cnfexpr))) != NULL) {
		expr->nodetype = nodetype;
		expr->l = l;
		expr->r = r;
	}
done:
	return expr;
}

/* ensure that retval is a number; if string is no number,
 * emit error message and set number to 0.
 */
static inline long long
exprret2Number(struct exprret *r)
{
	if(r->datatype == 'S') {
		printf("toNumber CONVERSION MISSING\n"); abort();
	}
	return r->d.n;
}

/* ensure that retval is a string; if string is no number,
 * emit error message and set number to 0.
 */
static inline es_str_t *
exprret2String(struct exprret *r)
{
	if(r->datatype == 'N') {
		printf("toString CONVERSION MISSING\n"); abort();
	}
	return r->d.estr;
}

#define COMP_NUM_BINOP(x) \
	cnfexprEval(expr->l, &l); \
	cnfexprEval(expr->r, &r); \
	ret->datatype = 'N'; \
	ret->d.n = exprret2Number(&l) x exprret2Number(&r)

/* evaluate an expression.
 * Note that we try to avoid malloc whenever possible (because on
 * the large overhead it has, especially on highly threaded programs).
 * As such, the each caller level must provide buffer space for the
 * result on its stack during recursion. This permits the callee to store
 * the return value without malloc. As the value is a somewhat larger
 * struct, we could otherwise not return it without malloc.
 * Note that we implement boolean shortcut operations. For our needs, there
 * simply is no case where full evaluation would make any sense at all.
 */
void
cnfexprEval(struct cnfexpr *expr, struct exprret *ret)
{
	struct exprret r, l; /* memory for subexpression results */

	//printf("eval expr %p, type '%c'(%u)\n", expr, expr->nodetype, expr->nodetype);
	switch(expr->nodetype) {
	case CMP_EQ:
		COMP_NUM_BINOP(==);
		break;
	case CMP_NE:
		COMP_NUM_BINOP(!=);
		break;
	case CMP_LE:
		COMP_NUM_BINOP(<=);
		break;
	case CMP_GE:
		COMP_NUM_BINOP(>=);
		break;
	case CMP_LT:
		COMP_NUM_BINOP(<);
		break;
	case CMP_GT:
		COMP_NUM_BINOP(>);
		break;
	case OR:
		cnfexprEval(expr->l, &l);
		ret->datatype = 'N';
		if(exprret2Number(&l)) {
			ret->d.n = 1ll;
		} else {
			cnfexprEval(expr->r, &r);
			if(exprret2Number(&r))
				ret->d.n = 1ll;
			else 
				ret->d.n = 0ll;
		}
		break;
	case AND:
		cnfexprEval(expr->l, &l);
		ret->datatype = 'N';
		if(exprret2Number(&l)) {
			cnfexprEval(expr->r, &r);
			if(exprret2Number(&r))
				ret->d.n = 1ll;
			else 
				ret->d.n = 0ll;
		} else {
			ret->d.n = 0ll;
		}
		break;
	case NOT:
		cnfexprEval(expr->r, &r);
		ret->datatype = 'N';
		ret->d.n = !exprret2Number(&r);
		break;
	case 'N':
		ret->datatype = 'N';
		ret->d.n = ((struct cnfnumval*)expr)->val;
		break;
	case '+':
		COMP_NUM_BINOP(+);
		break;
	case '-':
		COMP_NUM_BINOP(-);
		break;
	case '*':
		COMP_NUM_BINOP(*);
		break;
	case '/':
		COMP_NUM_BINOP(/);
		break;
	case '%':
		COMP_NUM_BINOP(%);
		break;
	case 'M':
		cnfexprEval(expr->r, &r);
		ret->datatype = 'N';
		ret->d.n = -exprret2Number(&r);
		break;
	default:
		ret->datatype = 'N';
		ret->d.n = 0ll;
		printf("eval error: unknown nodetype %u\n",
			(unsigned) expr->nodetype);
		break;
	}
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
	case CMP_EQ:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf("==\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_NE:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf("!=\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_LE:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf("<=\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_GE:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf(">=\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_LT:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf("<\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_GT:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf(">\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_CONTAINS:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf("CONTAINS\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_CONTAINSI:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf("CONTAINS_I\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_STARTSWITH:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf("STARTSWITH\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case CMP_STARTSWITHI:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf("STARTSWITH_I\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case OR:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf("OR\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case AND:
		cnfexprPrint(expr->l, indent+1);
		doIndent(indent);
		printf("AND\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case NOT:
		doIndent(indent);
		printf("NOT\n");
		cnfexprPrint(expr->r, indent+1);
		break;
	case 'N':
		doIndent(indent);
		printf("%lld\n", ((struct cnfnumval*)expr)->val);
		break;
	case 'V':
		doIndent(indent);
		printf("var '%s'\n", ((struct cnfvar*)expr)->name);
		break;
	case 'S':
		doIndent(indent);
		cstrPrint("string '", ((struct cnfstringval*)expr)->estr);
		printf("'\n");
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
		printf("error: unknown nodetype %u\n",
			(unsigned) expr->nodetype);
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

struct cnfvar*
cnfvarNew(char *name)
{
	struct cnfvar *var;
	if((var = malloc(sizeof(struct cnfvar))) != NULL) {
		var->nodetype = 'V';
		var->name = name;
	}
	return var;
}

struct cnfrule *
cnfruleNew(enum cnfFiltType filttype, struct cnfactlst *actlst)
{
	struct cnfrule* cnfrule;
	if((cnfrule = malloc(sizeof(struct cnfrule))) != NULL) {
		cnfrule->nodetype = 'R';
		cnfrule->filttype = filttype;
		cnfrule->actlst = cnfactlstReverse(actlst);
	}
	return cnfrule;
}

void
cnfrulePrint(struct cnfrule *rule)
{
	printf("------ start rule %p:\n", rule);
	printf("%s: ", cnfFiltType2str(rule->filttype));
	switch(rule->filttype) {
	case CNFFILT_NONE:
		break;
	case CNFFILT_PRI:
	case CNFFILT_PROP:
		printf("%s\n", rule->filt.s);
		break;
	case CNFFILT_SCRIPT:
		printf("\n");
		cnfexprPrint(rule->filt.expr, 0);
		break;
	}
	cnfactlstPrint(rule->actlst);
	printf("------ end rule %p\n", rule);
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


int
main(int argc, char *argv[])
{
	int r;

	cnfSetLexFile(argc == 1 ? NULL : argv[1]);
	yydebug = 0;
	r = yyparse();
	printf("yyparse() returned %d\n", r);
	return r;
}

