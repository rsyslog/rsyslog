/* The datetime object. It contains date and time related functions.
 *
 * Module begun 2008-03-05 by Rainer Gerhards, based on some code
 * from syslogd.c. The main intension was to move code out of syslogd.c
 * in a useful manner. It is still undecided if all functions will continue
 * to stay here or some will be moved into parser modules (once we have them).
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#ifdef HAVE_SYS_TIME_H
#	include <sys/time.h>
#endif

#include "rsyslog.h"
#include "obj.h"
#include "modules.h"
#include "datetime.h"
#include "sysvar.h"
#include "srUtils.h"
#include "stringbuf.h"
#include "errmsg.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)


/* ------------------------------ methods ------------------------------ */


/**
 * Get the current date/time in the best resolution the operating
 * system has to offer (well, actually at most down to the milli-
 * second level.
 *
 * The date and time is returned in separate fields as this is
 * most portable and removes the need for additional structures
 * (but I have to admit it is somewhat "bulky";)).
 *
 * Obviously, *t must not be NULL...
 *
 * rgerhards, 2008-10-07: added ttSeconds to provide a way to 
 * obtain the second-resolution UNIX timestamp. This is needed
 * in some situations to minimize time() calls (namely when doing
 * output processing). This can be left NULL if not needed.
 */
static void getCurrTime(struct syslogTime *t, time_t *ttSeconds)
{
	struct timeval tp;
	struct tm *tm;
	struct tm tmBuf;
	long lBias;
#	if defined(__hpux)
	struct timezone tz;
#	endif

	assert(t != NULL);
#	if defined(__hpux)
		/* TODO: check this: under HP UX, the tz information is actually valid
		 * data. So we need to obtain and process it there.
		 */
		gettimeofday(&tp, &tz);
#	else
		gettimeofday(&tp, NULL);
#	endif
	if(ttSeconds != NULL)
		*ttSeconds = tp.tv_sec;

	tm = localtime_r((time_t*) &(tp.tv_sec), &tmBuf);

	t->year = tm->tm_year + 1900;
	t->month = tm->tm_mon + 1;
	t->day = tm->tm_mday;
	t->hour = tm->tm_hour;
	t->minute = tm->tm_min;
	t->second = tm->tm_sec;
	t->secfrac = tp.tv_usec;
	t->secfracPrecision = 6;

#	if __sun
		/* Solaris uses a different method of exporting the time zone.
		 * It is UTC - localtime, which is the opposite sign of mins east of GMT.
		 */
		lBias = -(daylight ? altzone : timezone);
#	elif defined(__hpux)
		lBias = tz.tz_dsttime ? - tz.tz_minuteswest : 0;
#	else
		lBias = tm->tm_gmtoff;
#	endif
	if(lBias < 0)
	{
		t->OffsetMode = '-';
		lBias *= -1;
	}
	else
		t->OffsetMode = '+';
	t->OffsetHour = lBias / 3600;
	t->OffsetMinute = lBias % 3600;
}




/*******************************************************************
 * BEGIN CODE-LIBLOGGING                                           *
 *******************************************************************
 * Code in this section is borrowed from liblogging. This is an
 * interim solution. Once liblogging is fully integrated, this is
 * to be removed (see http://www.monitorware.com/liblogging for
 * more details. 2004-11-16 rgerhards
 *
 * Please note that the orginal liblogging code is modified so that
 * it fits into the context of the current version of syslogd.c.
 *
 * DO NOT PUT ANY OTHER CODE IN THIS BEGIN ... END BLOCK!!!!
 */

/**
 * Parse a 32 bit integer number from a string.
 *
 * \param ppsz Pointer to the Pointer to the string being parsed. It
 *             must be positioned at the first digit. Will be updated 
 *             so that on return it points to the first character AFTER
 *             the integer parsed.
 * \retval The number parsed.
 */

static int srSLMGParseInt32(char** ppsz)
{
	int i;

	i = 0;
	while(isdigit((int) **ppsz))
	{
		i = i * 10 + **ppsz - '0';
		++(*ppsz);
	}

	return i;
}


/**
 * Parse a TIMESTAMP-3339.
 * updates the parse pointer position. The pTime parameter
 * is guranteed to be updated only if a new valid timestamp
 * could be obtained (restriction added 2008-09-16 by rgerhards).
 */
static rsRetVal
ParseTIMESTAMP3339(struct syslogTime *pTime, char** ppszTS)
{
	char *pszTS = *ppszTS;
	/* variables to temporarily hold time information while we parse */
	int year;
	int month;
	int day;
	int hour; /* 24 hour clock */
	int minute;
	int second;
	int secfrac;	/* fractional seconds (must be 32 bit!) */
	int secfracPrecision;
	char OffsetMode;	/* UTC offset + or - */
	char OffsetHour;	/* UTC offset in hours */
	int OffsetMinute;	/* UTC offset in minutes */
	/* end variables to temporarily hold time information while we parse */
	DEFiRet;

	assert(pTime != NULL);
	assert(ppszTS != NULL);
	assert(pszTS != NULL);

	year = srSLMGParseInt32(&pszTS);

	/* We take the liberty to accept slightly malformed timestamps e.g. in 
	 * the format of 2003-9-1T1:0:0. This doesn't hurt on receiving. Of course,
	 * with the current state of affairs, we would never run into this code
	 * here because at postion 11, there is no "T" in such cases ;)
	 */
	if(*pszTS++ != '-')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	month = srSLMGParseInt32(&pszTS);
	if(month < 1 || month > 12)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(*pszTS++ != '-')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	day = srSLMGParseInt32(&pszTS);
	if(day < 1 || day > 31)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(*pszTS++ != 'T')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	hour = srSLMGParseInt32(&pszTS);
	if(hour < 0 || hour > 23)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(*pszTS++ != ':')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	minute = srSLMGParseInt32(&pszTS);
	if(minute < 0 || minute > 59)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(*pszTS++ != ':')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	second = srSLMGParseInt32(&pszTS);
	if(second < 0 || second > 60)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	/* Now let's see if we have secfrac */
	if(*pszTS == '.') {
		char *pszStart = ++pszTS;
		secfrac = srSLMGParseInt32(&pszTS);
		secfracPrecision = (int) (pszTS - pszStart);
	} else {
		secfracPrecision = 0;
		secfrac = 0;
	}

	/* check the timezone */
	if(*pszTS == 'Z')
	{
		pszTS++; /* eat Z */
		OffsetMode = 'Z';
		OffsetHour = 0;
		OffsetMinute = 0;
	} else if((*pszTS == '+') || (*pszTS == '-')) {
		OffsetMode = *pszTS;
		pszTS++;

		OffsetHour = srSLMGParseInt32(&pszTS);
		if(OffsetHour < 0 || OffsetHour > 23)
			ABORT_FINALIZE(RS_RET_INVLD_TIME);

		if(*pszTS++ != ':')
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		OffsetMinute = srSLMGParseInt32(&pszTS);
		if(OffsetMinute < 0 || OffsetMinute > 59)
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
	} else {
		/* there MUST be TZ information */
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	}

	/* OK, we actually have a 3339 timestamp, so let's indicated this */
	if(*pszTS == ' ')
		++pszTS;
	else
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	/* we had success, so update parse pointer and caller-provided timestamp */
	*ppszTS = pszTS;
	pTime->timeType = 2;
	pTime->year = year;
	pTime->month = month;
	pTime->day = day;
	pTime->hour = hour;
	pTime->minute = minute;
	pTime->second = second;
	pTime->secfrac = secfrac;
	pTime->secfracPrecision = secfracPrecision;
	pTime->OffsetMode = OffsetMode;
	pTime->OffsetHour = OffsetHour;
	pTime->OffsetMinute = OffsetMinute;

finalize_it:
	RETiRet;
}


/**
 * Parse a TIMESTAMP-3164. The pTime parameter
 * is guranteed to be updated only if a new valid timestamp
 * could be obtained (restriction added 2008-09-16 by rgerhards). This
 * also means the caller *must* provide a valid (probably current) 
 * timstamp in pTime when calling this function. a 3164 timestamp contains
 * only partial information and only that partial information is updated.
 * So the "output timestamp" is a valid timestamp only if the "input
 * timestamp" was valid, too. The is actually an optimization, as it
 * permits us to use a pre-aquired timestamp and thus avoids to do
 * a (costly) time() call. Thanks to David Lang for insisting on
 * time() call reduction ;).
 */
static rsRetVal
ParseTIMESTAMP3164(struct syslogTime *pTime, char** ppszTS)
{
	/* variables to temporarily hold time information while we parse */
	int month;
	int day;
	int year = 0; /* 0 means no year provided */
	int hour; /* 24 hour clock */
	int minute;
	int second;
	/* end variables to temporarily hold time information while we parse */
	char *pszTS;
	DEFiRet;

	assert(ppszTS != NULL);
	pszTS = *ppszTS;
	assert(pszTS != NULL);
	assert(pTime != NULL);

	/* If we look at the month (Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec),
	 * we may see the following character sequences occur:
	 *
	 * J(an/u(n/l)), Feb, Ma(r/y), A(pr/ug), Sep, Oct, Nov, Dec
	 *
	 * We will use this for parsing, as it probably is the
	 * fastest way to parse it.
	 *
	 * 2005-07-18, well sometimes it pays to be a bit more verbose, even in C...
	 * Fixed a bug that lead to invalid detection of the data. The issue was that
	 * we had an if(++pszTS == 'x') inside of some of the consturcts below. However,
	 * there were also some elseifs (doing the same ++), which than obviously did not
	 * check the orginal character but the next one. Now removed the ++ and put it
	 * into the statements below. Was a really nasty bug... I didn't detect it before
	 * june, when it first manifested. This also lead to invalid parsing of the rest
	 * of the message, as the time stamp was not detected to be correct. - rgerhards
	 */
	switch(*pszTS++)
	{
	case 'J':
		if(*pszTS == 'a') {
			++pszTS;
			if(*pszTS == 'n') {
				++pszTS;
				month = 1;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else if(*pszTS == 'u') {
			++pszTS;
			if(*pszTS == 'n') {
				++pszTS;
				month = 6;
			} else if(*pszTS == 'l') {
				++pszTS;
				month = 7;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'F':
		if(*pszTS == 'e') {
			++pszTS;
			if(*pszTS == 'b') {
				++pszTS;
				month = 2;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'M':
		if(*pszTS == 'a') {
			++pszTS;
			if(*pszTS == 'r') {
				++pszTS;
				month = 3;
			} else if(*pszTS == 'y') {
				++pszTS;
				month = 5;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'A':
		if(*pszTS == 'p') {
			++pszTS;
			if(*pszTS == 'r') {
				++pszTS;
				month = 4;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else if(*pszTS == 'u') {
			++pszTS;
			if(*pszTS == 'g') {
				++pszTS;
				month = 8;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'S':
		if(*pszTS == 'e') {
			++pszTS;
			if(*pszTS == 'p') {
				++pszTS;
				month = 9;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'O':
		if(*pszTS == 'c') {
			++pszTS;
			if(*pszTS == 't') {
				++pszTS;
				month = 10;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'N':
		if(*pszTS == 'o') {
			++pszTS;
			if(*pszTS == 'v') {
				++pszTS;
				month = 11;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'D':
		if(*pszTS == 'e') {
			++pszTS;
			if(*pszTS == 'c') {
				++pszTS;
				month = 12;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	default:
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	}

	/* done month */

	if(*pszTS++ != ' ')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	/* we accept a slightly malformed timestamp when receiving. This is
	 * we accept one-digit days
	 */
	if(*pszTS == ' ')
		++pszTS;

	day = srSLMGParseInt32(&pszTS);
	if(day < 1 || day > 31)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(*pszTS++ != ' ')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	/* time part */
	hour = srSLMGParseInt32(&pszTS);
	if(hour > 1970 && hour < 2100) {
		/* if so, we assume this actually is a year. This is a format found
		 * e.g. in Cisco devices.
		 * (if you read this 2100+ trying to fix a bug, congratulate myself
		 * to how long the code survived - me no longer ;)) -- rgerhards, 2008-11-18
		 */
		year = hour;

		/* re-query the hour, this time it must be valid */
		if(*pszTS++ != ' ')
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		hour = srSLMGParseInt32(&pszTS);
	}

	if(hour < 0 || hour > 23)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(*pszTS++ != ':')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	minute = srSLMGParseInt32(&pszTS);
	if(minute < 0 || minute > 59)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(*pszTS++ != ':')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	second = srSLMGParseInt32(&pszTS);
	if(second < 0 || second > 60)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	/* we provide support for an extra ":" after the date. While this is an
	 * invalid format, it occurs frequently enough (e.g. with Cisco devices)
	 * to permit it as a valid case. -- rgerhards, 2008-09-12
	 */
	if(*pszTS++ == ':')
		++pszTS; /* just skip past it */

	/* we had success, so update parse pointer and caller-provided timestamp
	 * fields we do not have are not updated in the caller's timestamp. This
	 * is the reason why the caller must pass in a correct timestamp.
	 */
	*ppszTS = pszTS; /* provide updated parse position back to caller */
	pTime->timeType = 1;
	pTime->month = month;
	if(year > 0)
		pTime->year = year; /* persist year if detected */
	pTime->day = day;
	pTime->hour = hour;
	pTime->minute = minute;
	pTime->second = second;
 	pTime->secfracPrecision = 0;
	pTime->secfrac = 0;

finalize_it:
	RETiRet;
}

/*******************************************************************
 * END CODE-LIBLOGGING                                             *
 *******************************************************************/

/**
 * Format a syslogTimestamp into format required by MySQL.
 * We are using the 14 digits format. For example 20041111122600 
 * is interpreted as '2004-11-11 12:26:00'. 
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string terminator). If 0 is returend, an error occured.
 */
int formatTimestampToMySQL(struct syslogTime *ts, char* pDst, size_t iLenDst)
{
	/* currently we do not consider localtime/utc. This may later be
	 * added. If so, I recommend using a property replacer option
	 * and/or a global configuration option. However, we should wait
	 * on user requests for this feature before doing anything.
	 * rgerhards, 2007-06-26
	 */
	assert(ts != NULL);
	assert(pDst != NULL);

	if (iLenDst < 15) /* we need at least 14 bytes
			     14 digits for timestamp + '\n' */
		return(0); 

	return(snprintf(pDst, iLenDst, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d", 
		ts->year, ts->month, ts->day, ts->hour, ts->minute, ts->second));

}

int formatTimestampToPgSQL(struct syslogTime *ts, char *pDst, size_t iLenDst)
{
       /* see note in formatTimestampToMySQL, applies here as well */
       assert(ts != NULL);
       assert(pDst != NULL);

       if (iLenDst < 21) /* we need 20 bytes + '\n' */
               return(0);

       return(snprintf(pDst, iLenDst, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
                               ts->year, ts->month, ts->day, ts->hour, ts->minute, ts->second));
}


/**
 * Format a syslogTimestamp to just the fractional seconds.
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string terminator). If 0 is returend, an error occured.
 * The buffer must be at least 10 bytes large.
 * rgerhards, 2008-06-06
 */
int formatTimestampSecFrac(struct syslogTime *ts, char* pBuf, size_t iLenBuf)
{
	int lenRet;
	char szFmtStr[64];

	assert(ts != NULL);
	assert(pBuf != NULL);
	assert(iLenBuf >= 10);

	if(ts->secfracPrecision > 0)
	{	/* We must look at
		 * the precision specified. For example, if we have millisec precision (3 digits), a
		 * secFrac value of 12 is not equivalent to ".12" but ".012". Obviously, this
		 * is a huge difference ;). To avoid this, we first create a format string with
		 * the specific precision and *then* use that format string to do the actual formating.
		 */
		/* be careful: there is ONE actual %d in the format string below ;) */
		snprintf(szFmtStr, sizeof(szFmtStr), "%%0%dd", ts->secfracPrecision);
		lenRet = snprintf(pBuf, iLenBuf, szFmtStr, ts->secfrac);
	} else {
		pBuf[0] = '0';
		pBuf[1] = '\0';
		lenRet = 1;
	}

	return(lenRet);
}


/**
 * Format a syslogTimestamp to a RFC3339 timestamp string (as
 * specified in syslog-protocol).
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string terminator). If 0 is returend, an error occured.
 */
int formatTimestamp3339(struct syslogTime *ts, char* pBuf, size_t iLenBuf)
{
	int iRet;
	char szTZ[7]; /* buffer for TZ information */

	assert(ts != NULL);
	assert(pBuf != NULL);
	
	if(iLenBuf < 20)
		return(0); /* we NEED at least 20 bytes */

	/* do TZ information first, this is easier to take care of "Z" zone in rfc3339 */
	if(ts->OffsetMode == 'Z') {
		szTZ[0] = 'Z';
		szTZ[1] = '\0';
	} else {
		snprintf(szTZ, sizeof(szTZ) / sizeof(char), "%c%2.2d:%2.2d",
			ts->OffsetMode, ts->OffsetHour, ts->OffsetMinute);
	}

	if(ts->secfracPrecision > 0)
	{	/* we now need to include fractional seconds. While doing so, we must look at
		 * the precision specified. For example, if we have millisec precision (3 digits), a
		 * secFrac value of 12 is not equivalent to ".12" but ".012". Obviously, this
		 * is a huge difference ;). To avoid this, we first create a format string with
		 * the specific precision and *then* use that format string to do the actual
		 * formating (mmmmhhh... kind of self-modifying code... ;)).
		 */
		char szFmtStr[64];
		/* be careful: there is ONE actual %d in the format string below ;) */
		snprintf(szFmtStr, sizeof(szFmtStr),
		         "%%04d-%%02d-%%02dT%%02d:%%02d:%%02d.%%0%dd%%s",
			ts->secfracPrecision);
		iRet = snprintf(pBuf, iLenBuf, szFmtStr, ts->year, ts->month, ts->day,
			        ts->hour, ts->minute, ts->second, ts->secfrac, szTZ);
	}
	else
		iRet = snprintf(pBuf, iLenBuf,
		 		"%4.4d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2d%s",
				ts->year, ts->month, ts->day,
			        ts->hour, ts->minute, ts->second, szTZ);
	return(iRet);
}

/**
 * Format a syslogTimestamp to a RFC3164 timestamp sring.
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string termnator). If 0 is returend, an error occured.
 */
int formatTimestamp3164(struct syslogTime *ts, char* pBuf, size_t iLenBuf)
{
	static char* monthNames[13] = {"ERR", "Jan", "Feb", "Mar",
	                               "Apr", "May", "Jun", "Jul",
				       "Aug", "Sep", "Oct", "Nov", "Dec"};
	assert(ts != NULL);
	assert(pBuf != NULL);
	
	if(iLenBuf < 16)
		return(0); /* we NEED 16 bytes */
	return(snprintf(pBuf, iLenBuf, "%s %2d %2.2d:%2.2d:%2.2d",
		monthNames[ts->month], ts->day, ts->hour,
		ts->minute, ts->second
		));
}

/**
 * Format a syslogTimestamp to a text format.
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string termnator). If 0 is returend, an error occured.
 */
#if 0 /* This method is currently not called, be we like to preserve it */
static int formatTimestamp(struct syslogTime *ts, char* pBuf, size_t iLenBuf)
{
	assert(ts != NULL);
	assert(pBuf != NULL);
	
	if(ts->timeType == 1) {
		return(formatTimestamp3164(ts, pBuf, iLenBuf));
	}

	if(ts->timeType == 2) {
		return(formatTimestamp3339(ts, pBuf, iLenBuf));
	}

	return(0);
}
#endif
/* queryInterface function
 * rgerhards, 2008-03-05
 */
BEGINobjQueryInterface(datetime)
CODESTARTobjQueryInterface(datetime)
	if(pIf->ifVersion != datetimeCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->getCurrTime = getCurrTime;
	pIf->ParseTIMESTAMP3339 = ParseTIMESTAMP3339;
	pIf->ParseTIMESTAMP3164 = ParseTIMESTAMP3164;
	pIf->formatTimestampToMySQL = formatTimestampToMySQL;
	pIf->formatTimestampToPgSQL = formatTimestampToPgSQL;
	pIf->formatTimestampSecFrac = formatTimestampSecFrac;
	pIf->formatTimestamp3339 = formatTimestamp3339;
	pIf->formatTimestamp3164 = formatTimestamp3164;
finalize_it:
ENDobjQueryInterface(datetime)


/* Initialize the datetime class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINAbstractObjClassInit(datetime, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));

ENDObjClassInit(datetime)

/* vi:set ai:
 */
