/* This is the header for the output channel code of rsyslog.
 * Please see syslogd.c for license information.
 * begun 2005-06-21 rgerhards
 *
 * Copyright(C) 2005 Rainer Gerhards and Adiscon GmbH
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
struct outchannel {
	struct outchannel *pNext;
	char *pszName;
	int iLenName;
	uchar *pszFileTemplate;
	off_t	uSizeLimit;
	uchar *cmdOnSizeLimit;
};

struct outchannel* ochConstruct(void);
struct outchannel *ochAddLine(char* pName, unsigned char** pRestOfConfLine);
struct outchannel *ochFind(char *pName, int iLenName);
void ochDeleteAll(void);
void ochPrintList(void);

/*
 * vi:set ai:
 */
