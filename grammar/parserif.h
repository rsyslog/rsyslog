#ifndef PARSERIF_H_DEFINED
#define PARSERIF_H_DEFINED
int cnfSetLexFile(char*);
int yyparse();
int yydebug;
void dbgprintf(char *fmt, ...) __attribute__((format(printf, 1, 2)));
void parser_errmsg(char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* entry points to be called after the parser has processed the
 * element in question. Actual processing must than be done inside
 * these functions.
 */
void cnfDoObj(struct cnfobj *o);
void cnfDoRule(struct cnfrule *rule);
void cnfDoCfsysline(char *ln);
void cnfDoBSDTag(char *ln);
void cnfDoBSDHost(char *ln);
#endif
