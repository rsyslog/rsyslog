/* header for parser.c
 *
 * Copyright 2008,2009 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#ifndef INCLUDED_PARSER_H
#define INCLUDED_PARSER_H


/* we create a small helper object, a list of parsers, that we can use to
 * build a chain of them whereever this is needed (initially thought to be
 * used in ruleset.c as well as ourselvs).
 */
struct parserList_s {
	parser_t *pParser;
	parserList_t *pNext;
};


/* the parser object, a dummy because we have only static methods */
struct parser_s {
	BEGINobjInstance;	/* Data to implement generic object - MUST be the first data element! */
	uchar *pName;		/* name of this parser */
	modInfo_t *pModule;	/* pointer to parser's module */
	sbool bDoSanitazion;	/* do standard message sanitazion before calling parser? */
	sbool bDoPRIParsing;	/* do standard PRI parsing before calling parser? */
};

/* interfaces */
BEGINinterface(parser) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(var);
	rsRetVal (*Construct)(parser_t **ppThis);
	rsRetVal (*ConstructFinalize)(parser_t *pThis);
	rsRetVal (*Destruct)(parser_t **ppThis);
	rsRetVal (*SetName)(parser_t *pThis, uchar *name);
	rsRetVal (*SetModPtr)(parser_t *pThis, modInfo_t *pMod);
	rsRetVal (*SetDoSanitazion)(parser_t *pThis, int);
	rsRetVal (*SetDoPRIParsing)(parser_t *pThis, int);
	rsRetVal (*FindParser)(parser_t **ppThis, uchar*name);
	rsRetVal (*InitParserList)(parserList_t **pListRoot);
	rsRetVal (*DestructParserList)(parserList_t **pListRoot);
	rsRetVal (*AddParserToList)(parserList_t **pListRoot, parser_t *pParser);
	/* static functions */
	rsRetVal (*ParseMsg)(msg_t *pMsg);
	rsRetVal (*SanitizeMsg)(msg_t *pMsg);
	rsRetVal (*AddDfltParser)(uchar *);
ENDinterface(parser)
#define parserCURR_IF_VERSION 1 /* increment whenever you change the interface above! */


/* prototypes */
PROTOTYPEObj(parser);


#endif /* #ifndef INCLUDED_PARSER_H */
