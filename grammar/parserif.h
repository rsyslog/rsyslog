#ifndef PARSERIF_H_DEFINED
#define PARSERIF_H_DEFINED
#include "rainerscript.h"
int cnfSetLexFile(char*);
int yyparse();
char *cnfcurrfn;
void dbgprintf(char *fmt, ...) __attribute__((format(printf, 1, 2)));
void parser_errmsg(char *fmt, ...) __attribute__((format(printf, 1, 2)));
void tellLexEndParsing(void);
extern int yydebug;
extern int yylineno;

/* entry points to be called after the parser has processed the
 * element in question. Actual processing must than be done inside
 * these functions.
 */
void cnfDoObj(struct cnfobj *o);
void cnfDoScript(struct cnfstmt *script);
void cnfDoCfsysline(char *ln);
void cnfDoBSDTag(char *ln);
void cnfDoBSDHost(char *ln);
es_str_t *cnfGetVar(char *name, void *usrptr);
#endif
