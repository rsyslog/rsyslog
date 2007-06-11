/*
    ksym.c - functions for kernel address->symbol translation
    Copyright (c) 1995, 1996  Dr. G.W. Wettstein <greg@wind.rmcc.com>
    Copyright (c) 1996 Enjellic Systems Development

    This file is part of the sysklogd package, a kernel and system log daemon.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * This file contains functions which handle the translation of kernel
 * numeric addresses into symbols for the klogd utility.
 *
 * Sat Oct 28 09:00:14 CDT 1995:  Dr. Wettstein
 *	Initial Version.
 *
 * Fri Nov 24 12:50:52 CST 1995:  Dr. Wettstein
 *	Added VERBOSE_DEBUGGING define to make debugging output more
 *	manageable.
 *
 *	Added support for verification of the loaded kernel symbols.  If
 *	no version information can be be found in the mapfile a warning
 *	message is issued but translation will still take place.  This
 *	will be the default case if kernel versions < 1.3.43 are used.
 *
 *	If the symbols in the mapfile are of the same version as the kernel
 *	that is running an informative message is issued.  If the symbols
 *	in the mapfile do not match the current kernel version a warning
 *	message is issued and translation is disabled.
 *
 * Wed Dec  6 16:14:11 CST 1995:  Dr. Wettstein
 *	Added /boot/System.map to the list of symbol maps to search for.
 *	Also made this map the first item in the search list.  I am open
 *	to CONSTRUCTIVE suggestions for any additions or corrections to
 *	the list of symbol maps to search for.  Be forewarned that the
 *	list in use is the consensus agreement between myself, Linus and
 *	some package distributers.  It is a given that no list will suit
 *	everyone's taste.  If you have rabid concerns about the list
 *	please feel free to edit the system_maps array and compile your
 *	own binaries.
 *
 *	Added support for searching of the list of symbol maps.  This
 *	allows support for access to multiple symbol maps.  The theory
 *	behind this is that a production kernel may have a system map in
 *	/boot/System.map.  If a test kernel is booted this system map
 *	would be skipped in favor of one found in /usr/src/linux.
 *
 * Thu Jan 18 11:18:31 CST 1996:  Dr. Wettstein
 *	Added patch from beta-testers to allow for reading of both
 *	ELF and a.out map files.
 *
 * Wed Aug 21 09:15:49 CDT 1996:  Dr. Wettstein
 *	Reloading of kernel module symbols is now turned on by the
 *	SetParanoiaLevel function.  The default behavior is to NOT reload
 *	the kernel module symbols when a protection fault is detected.
 *
 *	Added support for freeing of the current kernel module symbols.
 *	This was necessary to support reloading of the kernel module symbols.
 *
 *	When a matching static symbol table is loaded the kernel version
 *	number is printed.
 *
 * Mon Jun  9 17:12:42 CST 1997:  Martin Schulze
 *	Added #1 and #2 to some error messages in order to being able
 *	to divide them (ulmo@Q.Net)
 *
 * Fri Jun 13 10:50:23 CST 1997:  Martin Schulze
 *	Changed definition of LookupSymbol to non-static because it is
 *	used in klogd.c, too.
 *
 * Fri Jan  9 23:00:08 CET 1998: Martin Schulze <joey@infodrom.north.de>
 *	Fixed bug that caused klogd to die if there is no System.map available.
 *
 * Sun 29 Mar 18:14:07 BST 1998: Mark Simon Phillips <M.S.Phillips@nortel.co.uk>
 *	Switched to fgets() as gets() is not buffer overrun secure.
 *
 * Mon Apr 13 18:18:45 CEST 1998: Martin Schulze <joey@infodrom.north.de>
 *	Modified loop for detecting the correct system map.  Now it won't
 *	stop if a file has been found but doesn't contain the correct map.
 *	Special thanks go go Mark Simon Phillips for the hint.
 *
 * Mon Oct 12 00:42:30 CEST 1998: Martin Schulze <joey@infodrom.north.de>
 *	Modified CheckVersion()
 *	. Use shift to decode the kernel version
 *	. Compare integers of kernel version
 *	. extract major.minor.patch from utsname.release via sscanf()
 *	The reason lays in possible use of kernel flavours which
 *	modify utsname.release but no the Version_ symbol.
 *
 * Sun Feb 21 22:27:49 EST 1999: Keith Owens <kaos@ocs.com.au>
 *	Fixed bug that caused klogd to die if there is no sym_array available.
 *
 * Tue Sep 12 23:48:12 CEST 2000: Martin Schulze <joey@infodrom.ffis.de>
 *	Close symbol file in InitKsyms() when an error occurred.
 */


/* Includes. */
#include <stdlib.h>
#include <malloc.h>
#include <sys/utsname.h>
#include <ctype.h>
#include "klogd.h"
#include "ksyms.h"

#define VERBOSE_DEBUGGING 0


/* Variables static to this module. */
struct sym_table
{
	unsigned long value;
	char *name;
};

static int num_syms = 0;
static int i_am_paranoid = 0;
static char vstring[12];
static struct sym_table *sym_array = (struct sym_table *) 0;

static char *system_maps[] =
{
	"/boot/System.map",
	"/System.map",
#if defined(TEST)
	"./System.map",
#endif
	(char *) 0
};


#if defined(TEST)
int debugging;
#else
extern int debugging;
#endif


/* Function prototypes. */
static char * FindSymbolFile(void);
static int AddSymbol(unsigned long, char*);
static void FreeSymbols(void);
static int CheckVersion(char *);
static int CheckMapVersion(char *);


/**************************************************************************
 * Function:	InitKsyms
 *
 * Purpose:	This function is responsible for initializing and loading
 *		the data tables used by the kernel address translations.
 *
 * Arguements:	(char *) mapfile
 *
 *			mapfile:->	A pointer to a complete path
 *					specification of the file containing
 *					the kernel map to use.
 *
 * Return:	int
 *
 *		A boolean style context is returned.  The return value will
 *		be true if initialization was successful.  False if not.
 **************************************************************************/

extern int InitKsyms(mapfile)

	char *mapfile;

{
	auto char	type,
			sym[512];

	auto int version = 0;

	auto unsigned long int address;

	auto FILE *sym_file;


	/* Check and make sure that we are starting with a clean slate. */
	if ( num_syms > 0 )
		FreeSymbols();


	/*
	 * Search for and open the file containing the kernel symbols.
	 */
	if ( mapfile != (char *) 0 )
	{
		if ( (sym_file = fopen(mapfile, "r")) == (FILE *) 0 )
		{
			Syslog(LOG_WARNING, "Cannot open map file: %s.", \
			       mapfile);
			return(0);
		}
	}
	else
	{
		if ( (mapfile = FindSymbolFile()) == (char *) 0 ) 
		{
			Syslog(LOG_WARNING, "Cannot find map file.");
			if ( debugging )
				fputs("Cannot find map file.\n", stderr);
			return(0);
		}
		
		if ( (sym_file = fopen(mapfile, "r")) == (FILE *) 0 )
		{
			Syslog(LOG_WARNING, "Cannot open map file.");
			if ( debugging )
				fputs("Cannot open map file.\n", stderr);
			return(0);
		}
	}
	

	/*
	 * Read the kernel symbol table file and add entries for each
	 * line.  I suspect that the use of fscanf is not really in vogue
	 * but it was quick and dirty and IMHO suitable for fixed format
	 * data such as this.  If anybody doesn't agree with this please
	 * e-mail me a diff containing a parser with suitable political
	 * correctness -- GW.
	 */
	while ( !feof(sym_file) )
	{
		if ( fscanf(sym_file, "%lx %c %s\n", &address, &type, sym)
		    != 3 )
		{
			Syslog(LOG_ERR, "Error in symbol table input (#1).");
			fclose(sym_file);
			return(0);
		}
		if ( VERBOSE_DEBUGGING && debugging )
			fprintf(stderr, "Address: %lx, Type: %c, Symbol: %s\n",
				address, type, sym);

		if ( AddSymbol(address, sym) == 0 )
		{
			Syslog(LOG_ERR, "Error adding symbol - %s.", sym);
			fclose(sym_file);
			return(0);
		}

		if ( version == 0 )
			version = CheckVersion(sym);
	}
	

	Syslog(LOG_INFO, "Loaded %d symbols from %s.", num_syms, mapfile);
	switch ( version )
	{
	    case -1:
		Syslog(LOG_WARNING, "Symbols do not match kernel version.");
		num_syms = 0;
		break;

	    case 0:
		Syslog(LOG_WARNING, "Cannot verify that symbols match " \
		       "kernel version.");
		break;
		
	    case 1:
		Syslog(LOG_INFO, "Symbols match kernel version %s.", vstring);
		break;
	}
		
	fclose(sym_file);
	return(1);
}


/**************************************************************************
 * Function:	FindSymbolFile
 *
 * Purpose:	This function is responsible for encapsulating the search
 *		for a valid symbol file.  Encapsulating the search for
 *		the map file in this function allows an intelligent search
 *		process to be implemented.
 *
 *		The list of symbol files will be searched until either a
 *		symbol file is found whose version matches the currently
 *		executing kernel or the end of the list is encountered.  If
 *		the end of the list is encountered the first available
 *		symbol file is returned to the caller.
 *
 *		This strategy allows klogd to locate valid symbol files
 *		for both a production and an experimental kernel.  For
 *		example a map for a production kernel could be installed
 *		in /boot.  If an experimental kernel is loaded the map
 *		in /boot will be skipped and the map in /usr/src/linux would
 *		be used if its version number matches the executing kernel.
 *
 * Arguements:	None specified.
 *
 * Return:	char *
 *
 *		If a valid system map cannot be located a null pointer
 *		is returned to the caller.
 *
 *		If the search is succesful a pointer is returned to the
 *		caller which points to the name of the file containing
 *		the symbol table to be used.
 **************************************************************************/

static char * FindSymbolFile()

{
	auto char	*file = (char *) 0,
			**mf = system_maps;

	auto struct utsname utsname;
	static char symfile[100];

	auto FILE *sym_file = (FILE *) 0;

        if ( uname(&utsname) < 0 )
        {
                Syslog(LOG_ERR, "Cannot get kernel version information.");
                return(0);
        }

	if ( debugging )
		fputs("Searching for symbol map.\n", stderr);
	
	for (mf = system_maps; *mf != (char *) 0 && file == (char *) 0; ++mf)
	{
 
		sprintf (symfile, "%s-%s", *mf, utsname.release);
		if ( debugging )
			fprintf(stderr, "Trying %s.\n", symfile);
		if ( (sym_file = fopen(symfile, "r")) != (FILE *) 0 ) {
			if (CheckMapVersion(symfile) == 1)
				file = symfile;
			fclose(sym_file);
		}
		if (sym_file == (FILE *) 0 || file == (char *) 0) {
			sprintf (symfile, "%s", *mf);
			if ( debugging )
				fprintf(stderr, "Trying %s.\n", symfile);
			if ( (sym_file = fopen(symfile, "r")) != (FILE *) 0 ) {
				if (CheckMapVersion(symfile) == 1)
					file = symfile;
				fclose(sym_file);
			}
		}

	}

	/*
	 * At this stage of the game we are at the end of the symbol
	 * tables.
	 */
	if ( debugging )
		fprintf(stderr, "End of search list encountered.\n");
	return(file);
}


/**************************************************************************
 * Function:	CheckVersion
 *
 * Purpose:	This function is responsible for determining whether or
 *		the system map being loaded matches the version of the
 *		currently running kernel.
 *
 *		The kernel version is checked by examing a variable which
 *		is of the form:	_Version_66347 (a.out) or Version_66437 (ELF).
 *
 *		The suffix of this variable is the current kernel version
 *		of the kernel encoded in base 256.  For example the
 *		above variable would be decoded as:
 *
 *			(66347 = 1*65536 + 3*256 + 43 = 1.3.43)
 *
 *		(Insert appropriate deities here) help us if Linus ever
 *		needs more than 255 patch levels to get a kernel out the
 *		door... :-)
 *
 * Arguements:	(char *) version
 *
 *			version:->	A pointer to the string which
 *					is to be decoded as a kernel
 *					version variable.
 *
 * Return:	int
 *
 *		       -1:->	The currently running kernel version does
 *				not match this version string.
 *
 *			0:->	The string is not a kernel version variable.
 *
 *			1:->	The executing kernel is of the same version
 *				as the version string.
 **************************************************************************/

static int CheckVersion(version)

	char *version;
	

{
	auto int	vnum,
			major,
			minor,
			patch;

#ifndef TESTING
	int kvnum;
	auto struct utsname utsname;
#endif

	static char *prefix = { "Version_" };


	/* Early return if there is no hope. */
	if ( strncmp(version, prefix, strlen(prefix)) == 0  /* ELF */ ||
	   (*version == '_' &&
		strncmp(++version, prefix, strlen(prefix)) == 0 ) /* a.out */ )
		;
	else
		return(0);


	/*
	 * Since the symbol looks like a kernel version we can start
	 * things out by decoding the version string into its component
	 * parts.
	 */
	vnum = atoi(version + strlen(prefix));
	patch = vnum & 0x000000FF;
	minor = (vnum >> 8) & 0x000000FF;
	major = (vnum >> 16) & 0x000000FF;
	if ( debugging )
		fprintf(stderr, "Version string = %s, Major = %d, " \
		       "Minor = %d, Patch = %d.\n", version +
		       strlen(prefix), major, minor, \
		       patch);
	sprintf(vstring, "%d.%d.%d", major, minor, patch);

#ifndef TESTING
	/*
	 * We should now have the version string in the vstring variable in
	 * the same format that it is stored in by the kernel.  We now
	 * ask the kernel for its version information and compare the two
	 * values to determine if our system map matches the kernel
	 * version level.
	 */
	if ( uname(&utsname) < 0 )
	{
		Syslog(LOG_ERR, "Cannot get kernel version information.");
		return(0);
	}
	if ( debugging )
		fprintf(stderr, "Comparing kernel %s with symbol table %s.\n",\
		       utsname.release, vstring);

	if ( sscanf (utsname.release, "%d.%d.%d", &major, &minor, &patch) < 3 )
	{
		Syslog(LOG_ERR, "Kernel send bogus release string `%s'.",
		       utsname.release);
		return(0);
	}

	/* Compute the version code from data sent by the kernel */
	kvnum = (major << 16) | (minor << 8) | patch;

	/* Failure. */
	if ( vnum != kvnum )
		return(-1);

	/* Success. */
#endif
	return(1);
}


/**************************************************************************
 * Function:	CheckMapVersion
 *
 * Purpose:	This function is responsible for determining whether or
 *		the system map being loaded matches the version of the
 *		currently running kernel.  It uses CheckVersion as
 *		backend.
 *
 * Arguements:	(char *) fname
 *
 *			fname:->	A pointer to the string which
 *					references the system map file to
 *					be used.
 *
 * Return:	int
 *
 *		       -1:->	The currently running kernel version does
 *				not match the version in the given file.
 *
 *			0:->	No system map file or no version information.
 *
 *			1:->	The executing kernel is of the same version
 *				as the version of the map file.
 **************************************************************************/

static int CheckMapVersion(fname)

	char *fname;
	

{
	int	version;
	FILE	*sym_file;
	auto unsigned long int address;
	auto char	type,
			sym[512];

	if ( (sym_file = fopen(fname, "r")) != (FILE *) 0 ) {
		/*
		 * At this point a map file was successfully opened.  We
		 * now need to search this file and look for version
		 * information.
		 */
		Syslog(LOG_INFO, "Inspecting %s", fname);

		version = 0;
		while ( !feof(sym_file) && (version == 0) )
		{
			if ( fscanf(sym_file, "%lx %c %s\n", &address, \
				    &type, sym) != 3 )
			{
				Syslog(LOG_ERR, "Error in symbol table input (#2).");
				fclose(sym_file);
				return(0);
			}
			if ( VERBOSE_DEBUGGING && debugging )
				fprintf(stderr, "Address: %lx, Type: %c, " \
				    "Symbol: %s\n", address, type, sym);

			version = CheckVersion(sym);
		}
		fclose(sym_file);

		switch ( version )
		{
		    case -1:
			Syslog(LOG_ERR, "Symbol table has incorrect " \
				"version number.\n");
			break;
			
		    case 0:
			if ( debugging )
				fprintf(stderr, "No version information " \
					"found.\n");
			break;
		    case 1:
			if ( debugging )
				fprintf(stderr, "Found table with " \
					"matching version number.\n");
			break;
		}

		return(version);
	}

	return(0);
}

	
/**************************************************************************
 * Function:	AddSymbol
 *
 * Purpose:	This function is responsible for adding a symbol name
 *		and its address to the symbol table.
 *
 * Arguements:	(unsigned long) address, (char *) symbol
 *
 * Return:	int
 *
 *		A boolean value is assumed.  True if the addition is
 *		successful.  False if not.
 **************************************************************************/

static int AddSymbol(address, symbol)

	unsigned long address;
	
	char *symbol;
	
{
	/* Allocate the the symbol table entry. */
	sym_array = (struct sym_table *) realloc(sym_array, (num_syms+1) * \
						 sizeof(struct sym_table));
	if ( sym_array == (struct sym_table *) 0 )
		return(0);

	/* Then the space for the symbol. */
	sym_array[num_syms].name = (char *) malloc(strlen(symbol)*sizeof(char)\
						   + 1);
	if ( sym_array[num_syms].name == (char *) 0 )
		return(0);
	
	sym_array[num_syms].value = address;
	strcpy(sym_array[num_syms].name, symbol);
	++num_syms;
	return(1);
}


/**************************************************************************
 * Function:	LookupSymbol
 *
 * Purpose:	Find the symbol which is related to the given kernel
 *		address.
 *
 * Arguements:	(long int) value, (struct symbol *) sym
 *
 *		value:->	The address to be located.
 * 
 *		sym:->		A pointer to a structure which will be
 *				loaded with the symbol's parameters.
 *
 * Return:	(char *)
 *
 *		If a match cannot be found a diagnostic string is printed.
 *		If a match is found the pointer to the symbolic name most
 *		closely matching the address is returned.
 **************************************************************************/

char * LookupSymbol(value, sym)

	unsigned long value;

	struct symbol *sym;
	
{
	auto int lp;
	
	auto char *last;

	if (!sym_array)
		return((char *) 0);

	last = sym_array[0].name;
	sym->offset = 0;
	sym->size = 0;
	if ( value < sym_array[0].value )
		return((char *) 0);
	
	for(lp= 0; lp <= num_syms; ++lp)
	{
		if ( sym_array[lp].value > value )
		{		
			sym->offset = value - sym_array[lp-1].value;
			sym->size = sym_array[lp].value - \
				sym_array[lp-1].value;
			return(last);
		}
		last = sym_array[lp].name;
	}

	if ( (last = LookupModuleSymbol(value, sym)) != (char *) 0 )
		return(last);

	return((char *) 0);
}


/**************************************************************************
 * Function:	FreeSymbols
 *
 * Purpose:	This function is responsible for freeing all memory which
 *		has been allocated to hold the static symbol table.  It
 *		also initializes the symbol count and in general prepares
 *		for a re-read of a static symbol table.
 *
 * Arguements:  void
 *
 * Return:	void
 **************************************************************************/

static void FreeSymbols()

{
	auto int lp;

	/* Free each piece of memory allocated for symbol names. */
	for(lp= 0; lp < num_syms; ++lp)
		free(sym_array[lp].name);

	/* Whack the entire array and initialize everything. */
	free(sym_array);
	sym_array = (struct sym_table *) 0;
	num_syms = 0;

	return;
}


/**************************************************************************
 * Function:	LogExpanded
 *
 * Purpose:	This function is responsible for logging a kernel message
 *		line after all potential numeric kernel addresses have
 *		been resolved symolically.
 *
 * Arguements:	(char *) line, (char *) el
 *
 *		line:->	A pointer to the buffer containing the kernel
 *			message to be expanded and logged.
 *
 *		el:->	A pointer to the buffer into which the expanded
 *			kernel line will be written.
 *
 * Return:	void
 **************************************************************************/

extern char * ExpandKadds(line, el)

	char *line;

	char *el;
	
{
	auto char	dlm,
			*kp,
			*sl = line,
			*elp = el,
			*symbol;

	char num[15];
	auto unsigned long int value;

	auto struct symbol sym;


	/*
	 * This is as handy a place to put this as anyplace.
	 *
	 * Since the insertion of kernel modules can occur in a somewhat
	 * dynamic fashion we need some mechanism to insure that the
	 * kernel symbol tables get read just prior to when they are
	 * needed.
	 *
	 * To accomplish this we look for the Oops string and use its
	 * presence as a signal to load the module symbols.
	 *
	 * This is not the best solution of course, especially if the
	 * kernel is rapidly going out to lunch.  What really needs to
	 * be done is to somehow generate a callback from the
	 * kernel whenever a module is loaded or unloaded.  I am
	 * open for patches.
	 */
	if ( i_am_paranoid &&
	     (strstr(line, "Oops:") != (char *) 0) && !InitMsyms() )
		Syslog(LOG_WARNING, "Cannot load kernel module symbols.\n");
	

	/*
	 * Early return if there do not appear to be any kernel
	 * messages in this line.
	 */
	if ( (num_syms == 0) ||
	     (kp = strstr(line, "[<")) == (char *) 0 )
	{
#ifdef __sparc__
		if (num_syms) {
			/*
			 * On SPARC, register dumps do not have the [< >] characters in it.
			 */
			static struct sparc_tests {
				char *str;
				int len;
			} tests[] = { { "PC: ", 4 },
				      { " o7: ", 5 },
				      { " ret_pc: ", 9 },
				      { " i7: ", 5 },
				      { "Caller[", 7 }
				    };
			int i, j, ndigits;
			char *kp2;
			for (i = 0; i < 5; i++) {
				kp = strstr(line, tests[i].str);
				if (!kp) continue;
				kp2 = kp + tests[i].len;
				if (!isxdigit(*kp2)) continue;
				for (ndigits = 1; isxdigit(kp2[ndigits]); ndigits++);
				if (ndigits != 8 && ndigits != 16) continue;
				/* On sparc64, all kernel addresses are in first 4GB */
				if (ndigits == 16) {
					if (strncmp (kp2, "00000000", 8)) continue;
					kp2 += 8;
				}
				if (!i) {
					char *kp3;
					if (ndigits == 16 && kp > line && kp[-1L] != 'T') continue;
					kp3 = kp2 + 8;
					if (ndigits == 16) {
						if (strncmp (kp3, " TNPC: 00000000", 15) || !isxdigit(kp3[15]))
							continue;
						kp3 += 15;
					} else {
						if (strncmp (kp3, " NPC: ", 6) || !isxdigit(kp3[6]))
							continue;
						kp3 += 6;
					}
					for (j = 0; isxdigit(kp3[j]); j++);
					if (j != 8) continue;
					strncpy(elp, line, kp2 + 8 - line);
					elp += kp2 + 8 - line;
					value = strtol(kp2, (char **) 0, 16);
					if ( (symbol = LookupSymbol(value, &sym)) ) {
						if (sym.size)
							elp += sprintf(elp, " (%s+%d/%d)", symbol, sym.offset, sym.size);
						else
							elp += sprintf(elp, " (%s)", symbol);
					}
					strncpy(elp, kp2 + 8, kp3 - kp2);
					elp += kp3 - kp2;
					value = strtol(kp3, (char **) 0, 16);
					if ( (symbol = LookupSymbol(value, &sym)) ) {
						if (sym.size)
							elp += sprintf(elp, " (%s+%d/%d)", symbol, sym.offset, sym.size);
						else
							elp += sprintf(elp, " (%s)", symbol);
					}
					strcpy(elp, kp3 + 8);
				} else {
					strncpy(elp, line, kp2 + 8 - line);
					elp += kp2 + 8 - line;
					value = strtol(kp2, (char **) 0, 16);
					if ( (symbol = LookupSymbol(value, &sym)) ) {
						if (sym.size)
							elp += sprintf(elp, " (%s+%d/%d)", symbol, sym.offset, sym.size);
						else
							elp += sprintf(elp, " (%s)", symbol);
					}
					strcpy(elp, kp2 + 8);
				}
				return el;
			}
		}
#endif	
		strcpy(el, line);
		return(el);
	}

	/* Loop through and expand all kernel messages. */
	do
	{
		while ( sl < kp+1 )
			*elp++ = *sl++;

		/* Now poised at a kernel delimiter. */
	        if ( (kp = strstr(sl, ">]")) == (char *) 0 )
		{
			strcpy(el, sl);
			return(el);
		}
		dlm = *kp;
		strncpy(num,sl+1,kp-sl-1);
		num[kp-sl-1] = '\0';
		value = strtoul(num, (char **) 0, 16);
		if ( (symbol = LookupSymbol(value, &sym)) == (char *) 0 )
			symbol = sl;
			
		strcat(elp, symbol);
		elp += strlen(symbol);
		if ( debugging )
			fprintf(stderr, "Symbol: %s = %lx = %s, %x/%d\n", \
				sl+1, value, \
				(sym.size==0) ? symbol+1 : symbol, \
				sym.offset, sym.size);

		value = 2;
		if ( sym.size != 0 )
		{
			--value;
			++kp;
			elp += sprintf(elp, "+%x/%d", sym.offset, sym.size);
		}
		strncat(elp, kp, value);
		elp += value;
		sl = kp + value;
		if ( (kp = strstr(sl, "[<")) == (char *) 0 )
			strcat(elp, sl);
	}
	while ( kp != (char *) 0);
		
	if ( debugging )
		fprintf(stderr, "Expanded line: %s\n", el);
	return(el);
}


/**************************************************************************
 * Function:	SetParanoiaLevel
 *
 * Purpose:	This function is an interface function for setting the
 *		mode of loadable module symbol lookups.  Probably overkill
 *		but it does slay another global variable.
 *
 * Arguements:	(int) level
 *
 *		level:->	The amount of paranoia which is to be
 *				present when resolving kernel exceptions.
 * Return:	void
 **************************************************************************/

extern void SetParanoiaLevel(level)

	int level;

{
	i_am_paranoid = level;
	return;
}


/*
 * Setting the -DTEST define enables the following code fragment to
 * be compiled.  This produces a small standalone program which will
 * echo the standard input of the process to stdout while translating
 * all numeric kernel addresses into their symbolic equivalent.
 */
#if defined(TEST)

#include <stdarg.h>

extern int main(int, char **);


extern int main(int argc, char *argv[])
{
	auto char line[1024], eline[2048];

	debugging = 1;
	
	
	if ( !InitKsyms((char *) 0) )
	{
		fputs("ksym: Error loading system map.\n", stderr);
		return(1);
	}

	while ( !feof(stdin) )
	{
		fgets(line, sizeof(line), stdin);
		if (line[strlen(line)-1] == '\n') line[strlen(line)-1] = '\0'; /* Trash NL char */
		memset(eline, '\0', sizeof(eline));
		ExpandKadds(line, eline);
		fprintf(stdout, "%s\n", eline);
	}
	

	return(0);
}

extern void Syslog(int priority, char *fmt, ...)

{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stdout, "Pr: %d, ", priority);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fputc('\n', stdout);

	return;
}
#endif
