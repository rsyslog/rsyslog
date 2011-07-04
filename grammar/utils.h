#ifndef INC_UTILS_H
#define INC_UTILS_H
#include <libestr.h>

enum cnfobjType {
	CNFOBJ_ACTION,
	CNFOBJ_GLOBAL,
	CNFOBJ_INPUT,
	CNFOBJ_MODULE,
	CNFOBJ_INVALID = 0
};

static inline char*
cnfobjType2str(enum cnfobjType ot)
{
	switch(ot) {
	case CNFOBJ_ACTION:
		return "action";
		break;
	case CNFOBJ_GLOBAL:
		return "global";
		break;
	case CNFOBJ_INPUT:
		return "input";
		break;
	case CNFOBJ_MODULE:
		return "module";
		break;
	default:return "error: invalid cnfobjType";
	}
}

enum cnfactType { CNFACT_V2, CNFACT_LEGACY };

struct cnfobj {
	enum cnfobjType objType;
	struct nvlst *nvlst;
};

struct nvlst {
  struct nvlst *next;
  es_str_t *name;
  es_str_t *value;
};

struct cnfcfsyslinelst {
	struct cnfcfsyslinelst *next;
	char *line;
};

struct cnfactlst {
	struct cnfactlst *next;
	struct cnfcfsyslinelst *syslines;
	enum cnfactType actType;
	union {
		struct nvlst *lst;
		char *legActLine;
	} data;
};

/* the following structures support expressions, and may (very much later
 * be the sole foundation for the AST.
 *
 * nodetypes (list not yet complete)
 * S - string
 * N - number
 * V - var
 * R - rule
 */
enum cnfFiltType { CNFFILT_NONE, CNFFILT_PRI, CNFFILT_PROP, CNFFILT_SCRIPT };
static inline char*
cnfFiltType2str(enum cnfFiltType filttype)
{
	switch(filttype) {
	case CNFFILT_NONE:
		return("filter:none");
	case CNFFILT_PRI:
		return("filter:pri");
	case CNFFILT_PROP:
		return("filter:prop");
	case CNFFILT_SCRIPT:
		return("filter:script");
	}
	return("error:invalid_filter_type");	/* should never be reached */
}


struct cnfrule {
	unsigned nodetype;
	enum cnfFiltType filttype;
	union {
		char *s;
		struct cnfexpr *expr;
	} filt;
	struct cnfactlst *actlst;
};

struct cnfexpr {
	unsigned nodetype;
	struct cnfexpr *l;
	struct cnfexpr *r;
};

struct cnfnumval {
	unsigned nodetype;
	long long val;
};

struct cnfstringval {
	unsigned nodetype;
	es_str_t *estr;
};

struct cnfvar {
	unsigned nodetype;
	char *name;
};

/* future extensions
struct x {
	int nodetype;
};
*/

/* the return value of an expresion evaluation */
struct exprret {
	union {
		es_str_t *estr;
		long long n;
	} d;
	char datatype; /* 'N' - number, 'S' - string */
};


void readConfFile(FILE *fp, es_str_t **str);
struct nvlst* nvlstNew(es_str_t *name, es_str_t *value);
void nvlstDestruct(struct nvlst *lst);
void nvlstPrint(struct nvlst *lst);
struct cnfobj* cnfobjNew(enum cnfobjType objType, struct nvlst *lst);
void cnfobjDestruct(struct cnfobj *o);
void cnfobjPrint(struct cnfobj *o);
struct cnfactlst* cnfactlstNew(enum cnfactType actType, struct nvlst *lst, char *actLine);
void cnfactlstDestruct(struct cnfactlst *actlst);
void cnfactlstPrint(struct cnfactlst *actlst);
struct cnfactlst* cnfactlstAddSysline(struct cnfactlst* actlst, char *line);
struct cnfactlst* cnfactlstReverse(struct cnfactlst *actlst);
struct cnfexpr* cnfexprNew(unsigned nodetype, struct cnfexpr *l, struct cnfexpr *r);
void cnfexprPrint(struct cnfexpr *expr, int indent);
void cnfexprEval(struct cnfexpr *expr, struct exprret *ret);
struct cnfnumval* cnfnumvalNew(long long val);
struct cnfstringval* cnfstringvalNew(es_str_t *estr);
struct cnfrule * cnfruleNew(enum cnfFiltType filttype, struct cnfactlst *actlst);
void cnfrulePrint(struct cnfrule *rule);
struct cnfvar* cnfvarNew(char *name);

/* debug helper */
void cstrPrint(char *text, es_str_t *estr);
#endif
