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

/* debug helper */
void cstrPrint(char *text, es_str_t *estr);
#endif
