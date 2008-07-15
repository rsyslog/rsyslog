/* Definitions for config file handling (not yet an object).
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_CONF_H
#define INCLUDED_CONF_H

/* definitions used for doNameLine to differentiate between different command types
 * (with otherwise identical code). This is a left-over from the previous config
 * system. It stays, because it is still useful. So do not wonder why it looks
 * somewhat strange (at least its name). -- rgerhards, 2007-08-01
 */
enum eDirective { DIR_TEMPLATE = 0, DIR_OUTCHANNEL = 1, DIR_ALLOWEDSENDER = 2};

/* interfaces */
BEGINinterface(conf) /* name must also be changed in ENDinterface macro! */
	rsRetVal (*doNameLine)(uchar **pp, void* pVal);
	rsRetVal (*cfsysline)(uchar *p);
	rsRetVal (*doModLoad)(uchar **pp, __attribute__((unused)) void* pVal);
	rsRetVal (*doIncludeLine)(uchar **pp, __attribute__((unused)) void* pVal);
	rsRetVal (*cfline)(uchar *line, selector_t **pfCurr);
	rsRetVal (*processConfFile)(uchar *pConfFile);
ENDinterface(conf)
#define confCURR_IF_VERSION 1 /* increment whenever you change the interface structure! */


/* prototypes */
PROTOTYPEObj(conf);


/* TODO: remove them below (means move the config init code) -- rgerhards, 2008-02-19 */
extern EHostnameCmpMode eDfltHostnameCmpMode;
extern cstr_t *pDfltHostnameCmp;
extern cstr_t *pDfltProgNameCmp;

#endif /* #ifndef INCLUDED_CONF_H */
