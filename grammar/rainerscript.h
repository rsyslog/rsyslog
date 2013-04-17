#ifndef INC_UTILS_H
#define INC_UTILS_H
#include <stdio.h>
#include <libestr.h>
#include <typedefs.h>
#include <sys/types.h>
#include <regex.h>


#define	LOG_NFACILITIES	24	/* current number of syslog facilities */
#define CNFFUNC_MAX_ARGS 32
	/**< maximum number of arguments that any function can have (among
	 *   others, this is used to size data structures).
	 */

extern int Debug; /* 1 if in debug mode, 0 otherwise -- to be enhanced */

enum cnfobjType {
	CNFOBJ_ACTION,
	CNFOBJ_RULESET,
	CNFOBJ_GLOBAL,
	CNFOBJ_INPUT,
	CNFOBJ_MODULE,
	CNFOBJ_TPL,
	CNFOBJ_PROPERTY,
	CNFOBJ_CONSTANT,
	CNFOBJ_INVALID = 0
};

static inline char*
cnfobjType2str(enum cnfobjType ot)
{
	switch(ot) {
	case CNFOBJ_ACTION:
		return "action";
		break;
	case CNFOBJ_RULESET:
		return "ruleset";
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
	case CNFOBJ_TPL:
		return "template";
		break;
	case CNFOBJ_PROPERTY:
		return "property";
		break;
	case CNFOBJ_CONSTANT:
		return "constant";
		break;
	default:return "error: invalid cnfobjType";
	}
}

enum cnfactType { CNFACT_V2, CNFACT_LEGACY };

/* a variant type, for example used for expression evaluation
 * 2011-07-15/rger: note that there exists a "legacy" object var_t,
 * which implements the same idea, but in a suboptimal manner. I have
 * stipped this down as much as possible, but will keep it for a while
 * to avoid unnecessary complexity during development. TODO: in the long
 * term, var_t shall be replaced by struct var.
 */
struct var {
	union {
		es_str_t *estr;
		struct cnfarray *ar;
		long long n;
		struct json_object *json;
	} d;
	char datatype; /* 'N' number, 'S' string, 'J' JSON, 'A' array
			* Note: 'A' is only supported during config phase
			*/
};

struct cnfobj {
	enum cnfobjType objType;
	struct nvlst *nvlst;
	struct objlst *subobjs;
	struct cnfstmt *script;
};

struct objlst {
	struct objlst *next;
	struct cnfobj *obj;
};

struct nvlst {
  struct nvlst *next;
  es_str_t *name;
  struct var val;
  unsigned char bUsed;
  	/**< was this node used during config processing? If not, this
	 *   indicates an error. After all, the user specified a setting
	 *   that the software does not know.
	 */
};

/* the following structures support expressions, and may (very much later
 * be the sole foundation for the AST.
 *
 * nodetypes (list not yet complete)
 * F - function
 * N - number
 * P - fparamlst
 * R - rule
 * S - string
 * V - var
 * A - (string) array
 * ... plus the S_* #define's below:
 */
#define S_STOP 4000
#define S_PRIFILT 4001
#define S_PROPFILT 4002
#define S_IF 4003
#define S_ACT 4004
#define S_NOP 4005	/* usually used to disable some statement */
#define S_SET 4006
#define S_UNSET 4007
#define S_CALL 4008

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


struct cnfstmt {
	unsigned nodetype;
	struct cnfstmt *next;
	uchar *printable; /* printable text for debugging */
	union {
		struct {
			struct cnfexpr *expr;
			struct cnfstmt *t_then;
			struct cnfstmt *t_else;
		} s_if;
		struct {
			uchar *varname;
			struct cnfexpr *expr;
		} s_set;
		struct {
			uchar *varname;
		} s_unset;
		struct {
			es_str_t *name;
			struct cnfstmt *stmt;
		} s_call;
		struct {
			uchar pmask[LOG_NFACILITIES+1];	/* priority mask */
			struct cnfstmt *t_then;
			struct cnfstmt *t_else;
		} s_prifilt;
		struct {
			fiop_t operation;
			regex_t *regex_cache;/* cache for compiled REs, if used */
			struct cstr_s *pCSCompValue;/* value to "compare" against */
			sbool isNegated;
			uintTiny propID;/* ID of the requested property */
			es_str_t *propName;/* name of property for CEE-based filters */
			struct cnfstmt *t_then;
			struct cnfstmt *t_else;
		} s_propfilt;
		struct action_s *act;
	} d;
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

struct cnfarray {
	unsigned nodetype;
	int nmemb;
	es_str_t **arr;
};

struct cnffparamlst {
	unsigned nodetype; /* P */
	struct cnffparamlst *next;
	struct cnfexpr *expr;
};

enum cnffuncid {
	CNFFUNC_INVALID = 0, /**< defunct entry, do not use (should normally not be present) */
	CNFFUNC_NAME = 1,   /**< use name to call function (for future use) */
	CNFFUNC_STRLEN,
	CNFFUNC_GETENV,
	CNFFUNC_TOLOWER,
	CNFFUNC_CSTR,
	CNFFUNC_CNUM,
	CNFFUNC_RE_MATCH,
	CNFFUNC_RE_EXTRACT,
	CNFFUNC_FIELD,
	CNFFUNC_PRIFILT
};

struct cnffunc {
	unsigned nodetype;
	es_str_t *fname;
	unsigned short nParams;
	enum cnffuncid fID; /* function ID for built-ins, 0 means use name */
	void *funcdata;	/* global data for function-specific use (e.g. compiled regex) */
	struct cnfexpr *expr[];
};

/* future extensions
struct x {
	int nodetype;
};
*/


/* the following defines describe the parameter block for puling
 * config parameters. Note that the focus is on ease and saveness of
 * use, not performance. For example, we address parameters by name
 * instead of index, because the former is less error-prone. The (severe)
 * performance hit does not matter, as it is a one-time hit during config
 * load but never during actual processing. So there is really no reason
 * to care.
 */
struct cnfparamdescr { /* first the param description */
	char *name;	/**< not a es_str_t to ease definition in code */
	ecslCmdHdrlType type;
	unsigned flags;
};
/* flags for cnfparamdescr: */
#define CNFPARAM_REQUIRED 0x0001

struct cnfparamblk { /* now the actual param block use in API calls */
	unsigned short version;
	unsigned short nParams;
	struct cnfparamdescr *descr;
};
#define CNFPARAMBLK_VERSION 1
	/**< caller must have same version as engine -- else things may
	 * be messed up. But note that we may support multiple versions
	 * inside the engine, if at some later stage we want to do
	 * that. -- rgerhards, 2011-07-15
	 */
struct cnfparamvals { /* the values we obtained for param descr. */
	struct var val;
	unsigned char bUsed;
};

struct funcData_prifilt {
	uchar pmask[LOG_NFACILITIES+1];	/* priority mask */
};


int cnfParseBuffer(char *buf, unsigned lenBuf);
void readConfFile(FILE *fp, es_str_t **str);
struct objlst* objlstNew(struct cnfobj *obj);
void objlstDestruct(struct objlst *lst);
void objlstPrint(struct objlst *lst);
struct nvlst* nvlstNewArray(struct cnfarray *ar);
struct nvlst* nvlstNewStr(es_str_t *value);
struct nvlst* nvlstSetName(struct nvlst *lst, es_str_t *name);
void nvlstDestruct(struct nvlst *lst);
void nvlstPrint(struct nvlst *lst);
void nvlstChkUnused(struct nvlst *lst);
struct nvlst* nvlstFindName(struct nvlst *lst, es_str_t *name);
struct cnfobj* cnfobjNew(enum cnfobjType objType, struct nvlst *lst);
void cnfobjDestruct(struct cnfobj *o);
void cnfobjPrint(struct cnfobj *o);
struct cnfexpr* cnfexprNew(unsigned nodetype, struct cnfexpr *l, struct cnfexpr *r);
void cnfexprPrint(struct cnfexpr *expr, int indent);
void cnfexprEval(struct cnfexpr *expr, struct var *ret, void *pusr);
int cnfexprEvalBool(struct cnfexpr *expr, void *usrptr);
void cnfexprDestruct(struct cnfexpr *expr);
struct cnfnumval* cnfnumvalNew(long long val);
struct cnfstringval* cnfstringvalNew(es_str_t *estr);
struct cnfvar* cnfvarNew(char *name);
struct cnffunc * cnffuncNew(es_str_t *fname, struct cnffparamlst* paramlst);
struct cnffparamlst * cnffparamlstNew(struct cnfexpr *expr, struct cnffparamlst *next);
int cnfDoInclude(char *name);
int cnfparamGetIdx(struct cnfparamblk *params, char *name);
struct cnfparamvals* nvlstGetParams(struct nvlst *lst, struct cnfparamblk *params,
	       struct cnfparamvals *vals);
void cnfparamsPrint(struct cnfparamblk *params, struct cnfparamvals *vals);
int cnfparamvalsIsSet(struct cnfparamblk *params, struct cnfparamvals *vals);
void varDelete(struct var *v);
void cnfparamvalsDestruct(struct cnfparamvals *paramvals, struct cnfparamblk *blk);
struct cnfstmt * cnfstmtNew(unsigned s_type);
void cnfstmtPrintOnly(struct cnfstmt *stmt, int indent, sbool subtree);
void cnfstmtPrint(struct cnfstmt *stmt, int indent);
struct cnfstmt* scriptAddStmt(struct cnfstmt *root, struct cnfstmt *s);
struct objlst* objlstAdd(struct objlst *root, struct cnfobj *o);
char *rmLeadingSpace(char *s);
struct cnfstmt * cnfstmtNewPRIFILT(char *prifilt, struct cnfstmt *t_then);
struct cnfstmt * cnfstmtNewPROPFILT(char *propfilt, struct cnfstmt *t_then);
struct cnfstmt * cnfstmtNewAct(struct nvlst *lst);
struct cnfstmt * cnfstmtNewLegaAct(char *actline);
struct cnfstmt * cnfstmtNewSet(char *var, struct cnfexpr *expr);
struct cnfstmt * cnfstmtNewUnset(char *var);
struct cnfstmt * cnfstmtNewCall(es_str_t *name);
struct cnfstmt * cnfstmtNewContinue(void);
void cnfstmtDestruct(struct cnfstmt *root);
void cnfstmtOptimize(struct cnfstmt *root);
struct cnfarray* cnfarrayNew(es_str_t *val);
struct cnfarray* cnfarrayDup(struct cnfarray *old);
struct cnfarray* cnfarrayAdd(struct cnfarray *ar, es_str_t *val);
void cnfarrayContentDestruct(struct cnfarray *ar);
char* getFIOPName(unsigned iFIOP);
rsRetVal initRainerscript(void);
void unescapeStr(uchar *s, int len);
char * tokenval2str(int tok);

/* debug helper */
void cstrPrint(char *text, es_str_t *estr);
#endif
