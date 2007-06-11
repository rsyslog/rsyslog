
/* Module definitions for klogd's module support */
struct kernel_sym
{
	        unsigned long value;
	        char name[60];
};

struct module_symbol
{
	unsigned long value;
	const char *name;
};

struct module_ref
{
	struct module *dep;     /* "parent" pointer */
	struct module *ref;     /* "child" pointer */
	struct module_ref *next_ref;
};

struct module_info
{
	unsigned long addr;
	unsigned long size;
	unsigned long flags;
	long usecount;
};


typedef struct { volatile int counter; } atomic_t;

struct module
{
	unsigned long size_of_struct;   /* == sizeof(module) */
	struct module *next;
	const char *name;
	unsigned long size;
	
	union
	{
		atomic_t usecount;
		long pad;
        } uc;                           /* Needs to keep its size - so says rth */
	
	unsigned long flags;            /* AUTOCLEAN et al */
	
	unsigned nsyms;
	unsigned ndeps;
	
	struct module_symbol *syms;
	struct module_ref *deps;
	struct module_ref *refs;
	int (*init)(void);
	void (*cleanup)(void);
	const struct exception_table_entry *ex_table_start;
	const struct exception_table_entry *ex_table_end;
#ifdef __alpha__
	unsigned long gp;
#endif
};
	
