/* Module definitions for klogd's module support 
 *
 * Copyright 2007 by Rainer Gerhards and others
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
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
	
