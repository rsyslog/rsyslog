/* The datetime object. It contains date and time related functions.
 *
 * Module begun 2008-03-05 by Rainer Gerhards, based on some code
 * from syslogd.c. The main intension was to move code out of syslogd.c
 * in a useful manner. It is still undecided if all functions will continue
 * to stay here or some will be moved into parser modules (once we have them).
 *
 * Copyright 2008-2012 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
#include "srUtils.h"
#include "stringbuf.h"
#include "errmsg.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)

/* the following table of ten powers saves us some computation */
static const int tenPowers[6] = { 1, 10, 100, 1000, 10000, 100000 };

/* ------------------------------ methods ------------------------------ */


/** 
 * Convert struct timeval to syslog_time
 */
void
timeval2syslogTime(struct timeval *tp, struct syslogTime *t)
{
	struct tm *tm;
	struct tm tmBuf;
	long lBias;
	time_t secs;

	secs = tp->tv_sec;
	tm = localtime_r(&secs, &tmBuf);

	t->year = tm->tm_year + 1900;
	t->month = tm->tm_mon + 1;
	t->day = tm->tm_mday;
	t->hour = tm->tm_hour;
	t->minute = tm->tm_min;
	t->second = tm->tm_sec;
	t->secfrac = tp->tv_usec;
	t->secfracPrecision = 6;

#	if __sun
		/* Solaris uses a different method of exporting the time zone.
		 * It is UTC - localtime, which is the opposite sign of mins east of GMT.
		 */
		lBias = -(tm->tm_isdst ? altzone : timezone);
#	elif defined(__hpux)
		lBias = tz.tz_dsttime ? - tz.tz_minuteswest : 0;
#	else
		lBias = tm->tm_gmtoff;
#	endif
	if(lBias < 0) {
		t->OffsetMode = '-';
		lBias *= -1;
	} else
		t->OffsetMode = '+';
	t->OffsetHour = lBias / 3600;
	t->OffsetMinute = (lBias % 3600) / 60;
	t->timeType = TIME_TYPE_RFC5424; /* we have a high precision timestamp */
}

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

	timeval2syslogTime(&tp, t);
}


/* A fast alternative to getCurrTime() and time() that only obtains
 * a timestamp like time() does. I was told that gettimeofday(), at
 * least under Linux, is much faster than time() and I could confirm
 * this testing. So I created that function as a replacement.
 * rgerhards, 2009-11-12
 */
static time_t
getTime(time_t *ttSeconds)
{
	struct timeval tp;

	if(gettimeofday(&tp, NULL) == -1)
		return -1;

	if(ttSeconds != NULL)
		*ttSeconds = tp.tv_sec;
	return tp.tv_sec;
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
 * \param pLenStr pointer to string length, decremented on exit by
 *                characters processed
 * 		  Note that if an empty string (len < 1) is passed in,
 * 		  the method always returns zero.
 * \retval The number parsed.
 */
static inline int
srSLMGParseInt32(uchar** ppsz, int *pLenStr)
{
	register int i;

	i = 0;
	while(*pLenStr > 0 && **ppsz >= '0' && **ppsz <= '9') {
		i = i * 10 + **ppsz - '0';
		++(*ppsz);
		--(*pLenStr);
	}

	return i;
}


/**
 * Parse a TIMESTAMP-3339.
 * updates the parse pointer position. The pTime parameter
 * is guranteed to be updated only if a new valid timestamp
 * could be obtained (restriction added 2008-09-16 by rgerhards).
 * This method now also checks the maximum string length it is passed.
 * If a *valid* timestamp is found, the string length is decremented
 * by the number of characters processed. If it is not a valid timestamp,
 * the length is kept unmodified. -- rgerhards, 2009-09-23
 */
static rsRetVal
ParseTIMESTAMP3339(struct syslogTime *pTime, uchar** ppszTS, int *pLenStr)
{
	uchar *pszTS = *ppszTS;
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
	int lenStr;
	/* end variables to temporarily hold time information while we parse */
	DEFiRet;

	assert(pTime != NULL);
	assert(ppszTS != NULL);
	assert(pszTS != NULL);

	lenStr = *pLenStr;
	year = srSLMGParseInt32(&pszTS, &lenStr);

	/* We take the liberty to accept slightly malformed timestamps e.g. in 
	 * the format of 2003-9-1T1:0:0. This doesn't hurt on receiving. Of course,
	 * with the current state of affairs, we would never run into this code
	 * here because at postion 11, there is no "T" in such cases ;)
	 */
	if(lenStr == 0 || *pszTS++ != '-')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	--lenStr;
	month = srSLMGParseInt32(&pszTS, &lenStr);
	if(month < 1 || month > 12)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(lenStr == 0 || *pszTS++ != '-')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	--lenStr;
	day = srSLMGParseInt32(&pszTS, &lenStr);
	if(day < 1 || day > 31)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(lenStr == 0 || *pszTS++ != 'T')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	--lenStr;

	hour = srSLMGParseInt32(&pszTS, &lenStr);
	if(hour < 0 || hour > 23)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(lenStr == 0 || *pszTS++ != ':')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	--lenStr;
	minute = srSLMGParseInt32(&pszTS, &lenStr);
	if(minute < 0 || minute > 59)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(lenStr == 0 || *pszTS++ != ':')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	--lenStr;
	second = srSLMGParseInt32(&pszTS, &lenStr);
	if(second < 0 || second > 60)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	/* Now let's see if we have secfrac */
	if(lenStr > 0 && *pszTS == '.') {
		--lenStr;
		uchar *pszStart = ++pszTS;
		secfrac = srSLMGParseInt32(&pszTS, &lenStr);
		secfracPrecision = (int) (pszTS - pszStart);
	} else {
		secfracPrecision = 0;
		secfrac = 0;
	}

	/* check the timezone */
	if(lenStr == 0)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(*pszTS == 'Z') {
		--lenStr;
		pszTS++; /* eat Z */
		OffsetMode = 'Z';
		OffsetHour = 0;
		OffsetMinute = 0;
	} else if((*pszTS == '+') || (*pszTS == '-')) {
		OffsetMode = *pszTS;
		--lenStr;
		pszTS++;

		OffsetHour = srSLMGParseInt32(&pszTS, &lenStr);
		if(OffsetHour < 0 || OffsetHour > 23)
			ABORT_FINALIZE(RS_RET_INVLD_TIME);

		if(lenStr == 0 || *pszTS != ':')
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		--lenStr;
		pszTS++;
		OffsetMinute = srSLMGParseInt32(&pszTS, &lenStr);
		if(OffsetMinute < 0 || OffsetMinute > 59)
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
	} else {
		/* there MUST be TZ information */
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	}

	/* OK, we actually have a 3339 timestamp, so let's indicated this */
	if(lenStr > 0) {
		if(*pszTS != ' ') /* if it is not a space, it can not be a "good" time - 2010-02-22 rgerhards */
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		++pszTS; /* just skip past it */
		--lenStr;
	}

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
	*pLenStr = lenStr;

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
 * This method now also checks the maximum string length it is passed.
 * If a *valid* timestamp is found, the string length is decremented
 * by the number of characters processed. If it is not a valid timestamp,
 * the length is kept unmodified. -- rgerhards, 2009-09-23
 */
static rsRetVal
ParseTIMESTAMP3164(struct syslogTime *pTime, uchar** ppszTS, int *pLenStr)
{
	/* variables to temporarily hold time information while we parse */
	int month;
	int day;
	int year = 0; /* 0 means no year provided */
	int hour; /* 24 hour clock */
	int minute;
	int second;
	/* end variables to temporarily hold time information while we parse */
	int lenStr;
	uchar *pszTS;
	DEFiRet;

	assert(ppszTS != NULL);
	pszTS = *ppszTS;
	assert(pszTS != NULL);
	assert(pTime != NULL);
	assert(pLenStr != NULL);
	lenStr = *pLenStr;

	/* If we look at the month (Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec),
	 * we may see the following character sequences occur:
	 *
	 * J(an/u(n/l)), Feb, Ma(r/y), A(pr/ug), Sep, Oct, Nov, Dec
	 *
	 * We will use this for parsing, as it probably is the
	 * fastest way to parse it.
	 *
	 * 2009-08-17: we now do case-insensitive comparisons, as some devices obviously do not
	 * obey to the RFC-specified case. As we need to guess in any case, we can ignore case
	 * in the first place -- rgerhards
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
	if(lenStr < 3)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	switch(*pszTS++)
	{
	case 'j':
	case 'J':
		if(*pszTS == 'a' || *pszTS == 'A') {
			++pszTS;
			if(*pszTS == 'n' || *pszTS == 'N') {
				++pszTS;
				month = 1;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else if(*pszTS == 'u' || *pszTS == 'U') {
			++pszTS;
			if(*pszTS == 'n' || *pszTS == 'N') {
				++pszTS;
				month = 6;
			} else if(*pszTS == 'l' || *pszTS == 'L') {
				++pszTS;
				month = 7;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'f':
	case 'F':
		if(*pszTS == 'e' || *pszTS == 'E') {
			++pszTS;
			if(*pszTS == 'b' || *pszTS == 'B') {
				++pszTS;
				month = 2;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'm':
	case 'M':
		if(*pszTS == 'a' || *pszTS == 'A') {
			++pszTS;
			if(*pszTS == 'r' || *pszTS == 'R') {
				++pszTS;
				month = 3;
			} else if(*pszTS == 'y' || *pszTS == 'Y') {
				++pszTS;
				month = 5;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'a':
	case 'A':
		if(*pszTS == 'p' || *pszTS == 'P') {
			++pszTS;
			if(*pszTS == 'r' || *pszTS == 'R') {
				++pszTS;
				month = 4;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else if(*pszTS == 'u' || *pszTS == 'U') {
			++pszTS;
			if(*pszTS == 'g' || *pszTS == 'G') {
				++pszTS;
				month = 8;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 's':
	case 'S':
		if(*pszTS == 'e' || *pszTS == 'E') {
			++pszTS;
			if(*pszTS == 'p' || *pszTS == 'P') {
				++pszTS;
				month = 9;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'o':
	case 'O':
		if(*pszTS == 'c' || *pszTS == 'C') {
			++pszTS;
			if(*pszTS == 't' || *pszTS == 'T') {
				++pszTS;
				month = 10;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'n':
	case 'N':
		if(*pszTS == 'o' || *pszTS == 'O') {
			++pszTS;
			if(*pszTS == 'v' || *pszTS == 'V') {
				++pszTS;
				month = 11;
			} else
				ABORT_FINALIZE(RS_RET_INVLD_TIME);
		} else
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		break;
	case 'd':
	case 'D':
		if(*pszTS == 'e' || *pszTS == 'E') {
			++pszTS;
			if(*pszTS == 'c' || *pszTS == 'C') {
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

	lenStr -= 3;

	/* done month */

	if(lenStr == 0 || *pszTS++ != ' ')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	--lenStr;

	/* we accept a slightly malformed timestamp when receiving. This is
	 * we accept one-digit days
	 */
	if(*pszTS == ' ') {
		--lenStr;
		++pszTS;
	}

	day = srSLMGParseInt32(&pszTS, &lenStr);
	if(day < 1 || day > 31)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(lenStr == 0 || *pszTS++ != ' ')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	--lenStr;

	/* time part */
	hour = srSLMGParseInt32(&pszTS, &lenStr);
	if(hour > 1970 && hour < 2100) {
		/* if so, we assume this actually is a year. This is a format found
		 * e.g. in Cisco devices.
		 * (if you read this 2100+ trying to fix a bug, congratulate me
		 * to how long the code survived - me no longer ;)) -- rgerhards, 2008-11-18
		 */
		year = hour;

		/* re-query the hour, this time it must be valid */
		if(lenStr == 0 || *pszTS++ != ' ')
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		--lenStr;
		hour = srSLMGParseInt32(&pszTS, &lenStr);
	}

	if(hour < 0 || hour > 23)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(lenStr == 0 || *pszTS++ != ':')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	--lenStr;
	minute = srSLMGParseInt32(&pszTS, &lenStr);
	if(minute < 0 || minute > 59)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	if(lenStr == 0 || *pszTS++ != ':')
		ABORT_FINALIZE(RS_RET_INVLD_TIME);
	--lenStr;
	second = srSLMGParseInt32(&pszTS, &lenStr);
	if(second < 0 || second > 60)
		ABORT_FINALIZE(RS_RET_INVLD_TIME);

	/* we provide support for an extra ":" after the date. While this is an
	 * invalid format, it occurs frequently enough (e.g. with Cisco devices)
	 * to permit it as a valid case. -- rgerhards, 2008-09-12
	 */
	if(lenStr > 0 && *pszTS == ':') {
		++pszTS; /* just skip past it */
		--lenStr;
	}
	if(lenStr > 0) {
		if(*pszTS != ' ') /* if it is not a space, it can not be a "good" time - 2010-02-22 rgerhards */
			ABORT_FINALIZE(RS_RET_INVLD_TIME);
		++pszTS; /* just skip past it */
		--lenStr;
	}

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
	*pLenStr = lenStr;

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
int formatTimestampToMySQL(struct syslogTime *ts, char* pBuf)
{
	/* currently we do not consider localtime/utc. This may later be
	 * added. If so, I recommend using a property replacer option
	 * and/or a global configuration option. However, we should wait
	 * on user requests for this feature before doing anything.
	 * rgerhards, 2007-06-26
	 */
	assert(ts != NULL);
	assert(pBuf != NULL);

	pBuf[0] = (ts->year / 1000) % 10 + '0';
	pBuf[1] = (ts->year / 100) % 10 + '0';
	pBuf[2] = (ts->year / 10) % 10 + '0';
	pBuf[3] = ts->year % 10 + '0';
	pBuf[4] = (ts->month / 10) % 10 + '0';
	pBuf[5] = ts->month % 10 + '0';
	pBuf[6] = (ts->day / 10) % 10 + '0';
	pBuf[7] = ts->day % 10 + '0';
	pBuf[8] = (ts->hour / 10) % 10 + '0';
	pBuf[9] = ts->hour % 10 + '0';
	pBuf[10] = (ts->minute / 10) % 10 + '0';
	pBuf[11] = ts->minute % 10 + '0';
	pBuf[12] = (ts->second / 10) % 10 + '0';
	pBuf[13] = ts->second % 10 + '0';
	pBuf[14] = '\0';
	return 15;

}

int formatTimestampToPgSQL(struct syslogTime *ts, char *pBuf)
{
	/* see note in formatTimestampToMySQL, applies here as well */
	assert(ts != NULL);
	assert(pBuf != NULL);

	pBuf[0] = (ts->year / 1000) % 10 + '0';
	pBuf[1] = (ts->year / 100) % 10 + '0';
	pBuf[2] = (ts->year / 10) % 10 + '0';
	pBuf[3] = ts->year % 10 + '0';
	pBuf[4] = '-';
	pBuf[5] = (ts->month / 10) % 10 + '0';
	pBuf[6] = ts->month % 10 + '0';
	pBuf[7] = '-';
	pBuf[8] = (ts->day / 10) % 10 + '0';
	pBuf[9] = ts->day % 10 + '0';
	pBuf[10] = ' ';
	pBuf[11] = (ts->hour / 10) % 10 + '0';
	pBuf[12] = ts->hour % 10 + '0';
	pBuf[13] = ':';
	pBuf[14] = (ts->minute / 10) % 10 + '0';
	pBuf[15] = ts->minute % 10 + '0';
	pBuf[16] = ':';
	pBuf[17] = (ts->second / 10) % 10 + '0';
	pBuf[18] = ts->second % 10 + '0';
	pBuf[19] = '\0';
	return 19;
}


/**
 * Format a syslogTimestamp to just the fractional seconds.
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string terminator). If 0 is returend, an error occured.
 * The buffer must be at least 7 bytes large.
 * rgerhards, 2008-06-06
 */
int formatTimestampSecFrac(struct syslogTime *ts, char* pBuf)
{
	int iBuf;
	int power;
	int secfrac;
	short digit;

	assert(ts != NULL);
	assert(pBuf != NULL);

	iBuf = 0;
	if(ts->secfracPrecision > 0)
	{	
		power = tenPowers[(ts->secfracPrecision - 1) % 6];
		secfrac = ts->secfrac;
		while(power > 0) {
			digit = secfrac / power;
			secfrac -= digit * power;
			power /= 10;
			pBuf[iBuf++] = digit + '0';
		}
	} else {
		pBuf[iBuf++] = '0';
	}
	pBuf[iBuf] = '\0';

	return iBuf;
}


/**
 * Format a syslogTimestamp to a RFC3339 timestamp string (as
 * specified in syslog-protocol).
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string terminator). If 0 is returend, an error occured.
 */
int formatTimestamp3339(struct syslogTime *ts, char* pBuf)
{
	int iBuf;
	int power;
	int secfrac;
	short digit;

	BEGINfunc
	assert(ts != NULL);
	assert(pBuf != NULL);

	/* start with fixed parts */
	/* year yyyy */
	pBuf[0] = (ts->year / 1000) % 10 + '0';
	pBuf[1] = (ts->year / 100) % 10 + '0';
	pBuf[2] = (ts->year / 10) % 10 + '0';
	pBuf[3] = ts->year % 10 + '0';
	pBuf[4] = '-';
	/* month */
	pBuf[5] = (ts->month / 10) % 10 + '0';
	pBuf[6] = ts->month % 10 + '0';
	pBuf[7] = '-';
	/* day */
	pBuf[8] = (ts->day / 10) % 10 + '0';
	pBuf[9] = ts->day % 10 + '0';
	pBuf[10] = 'T';
	/* hour */
	pBuf[11] = (ts->hour / 10) % 10 + '0';
	pBuf[12] = ts->hour % 10 + '0';
	pBuf[13] = ':';
	/* minute */
	pBuf[14] = (ts->minute / 10) % 10 + '0';
	pBuf[15] = ts->minute % 10 + '0';
	pBuf[16] = ':';
	/* second */
	pBuf[17] = (ts->second / 10) % 10 + '0';
	pBuf[18] = ts->second % 10 + '0';

	iBuf = 19; /* points to next free entry, now it becomes dynamic! */

	if(ts->secfracPrecision > 0) {
		pBuf[iBuf++] = '.';
		power = tenPowers[(ts->secfracPrecision - 1) % 6];
		secfrac = ts->secfrac;
		while(power > 0) {
			digit = secfrac / power;
			secfrac -= digit * power;
			power /= 10;
			pBuf[iBuf++] = digit + '0';
		}
	}

	if(ts->OffsetMode == 'Z') {
		pBuf[iBuf++] = 'Z';
	} else {
		pBuf[iBuf++] = ts->OffsetMode;
		pBuf[iBuf++] = (ts->OffsetHour / 10) % 10 + '0';
		pBuf[iBuf++] = ts->OffsetHour % 10 + '0';
		pBuf[iBuf++] = ':';
		pBuf[iBuf++] = (ts->OffsetMinute / 10) % 10 + '0';
		pBuf[iBuf++] = ts->OffsetMinute % 10 + '0';
	}

	pBuf[iBuf] = '\0';

	ENDfunc
	return iBuf;
}

/**
 * Format a syslogTimestamp to a RFC3164 timestamp sring.
 * The caller must provide the timestamp as well as a character
 * buffer that will receive the resulting string. The function
 * returns the size of the timestamp written in bytes (without
 * the string termnator). If 0 is returend, an error occured.
 * rgerhards, 2010-03-05: Added support to for buggy 3164 dates,
 * where a zero-digit is written instead of a space for the first
 * day character if day < 10. syslog-ng seems to do that, and some
 * parsing scripts (in migration cases) rely on that.
 */
int formatTimestamp3164(struct syslogTime *ts, char* pBuf, int bBuggyDay)
{
	static char* monthNames[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
					"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	int iDay;
	assert(ts != NULL);
	assert(pBuf != NULL);
	
	pBuf[0] = monthNames[(ts->month - 1)% 12][0];
	pBuf[1] = monthNames[(ts->month - 1) % 12][1];
	pBuf[2] = monthNames[(ts->month - 1) % 12][2];
	pBuf[3] = ' ';
	iDay = (ts->day / 10) % 10; /* we need to write a space if the first digit is 0 */
	pBuf[4] = (bBuggyDay || iDay > 0) ? iDay + '0' : ' ';
	pBuf[5] = ts->day % 10 + '0';
	pBuf[6] = ' ';
	pBuf[7] = (ts->hour / 10) % 10 + '0';
	pBuf[8] = ts->hour % 10 + '0';
	pBuf[9] = ':';
	pBuf[10] = (ts->minute / 10) % 10 + '0';
	pBuf[11] = ts->minute % 10 + '0';
	pBuf[12] = ':';
	pBuf[13] = (ts->second / 10) % 10 + '0';
	pBuf[14] = ts->second % 10 + '0';
	pBuf[15] = '\0';
	return 16;	/* traditional: number of bytes written */
}


/**
 * convert syslog timestamp to time_t
 */
time_t syslogTime2time_t(struct syslogTime *ts)
{
	long MonthInDays, NumberOfYears, NumberOfDays, i;
	int utcOffset;
	time_t TimeInUnixFormat;

	/* Counting how many Days have passed since the 01.01 of the
	 * selected Year (Month level), according to the selected Month*/

	switch(ts->month)
	{
		case 1:
			MonthInDays = 0;         //until 01 of January
			break;
		case 2:
			MonthInDays = 31;        //until 01 of February - leap year handling down below!
			break;
		case 3:
			MonthInDays = 59;        //until 01 of March
			break;
		case 4:
			MonthInDays = 90;        //until 01 of April
			break;
		case 5:
			MonthInDays = 120;       //until 01 of Mai
			break;
		case 6:
			MonthInDays = 151;       //until 01 of June
			break;
		case 7:
			MonthInDays = 181;       //until 01 of July
			break;
		case 8:
			MonthInDays = 212;       //until 01 of August
			break;
		case 9:
			MonthInDays = 243;       //until 01 of September
			break;
		case 10:
			MonthInDays = 273;       //until 01 of Oktober
			break;
		case 11:
			MonthInDays = 304;       //until 01 of November
			break;
		case 12:
			MonthInDays = 334;       //until 01 of December
			break;
		default: /* this cannot happen (and would be a program error)
		          * but we need the code to keep the compiler silent.
			  */
			MonthInDays = 0;	/* any value fits ;) */
			break;
	}	


	/*	1) Counting how many Years have passed since 1970
		2) Counting how many Days have passed since the 01.01 of the selected Year
			(Day level) according to the Selected Month and Day. Last day doesn't count,
			it should be until last day
		3) Calculating this period (NumberOfDays) in seconds*/

	NumberOfYears = ts->year - 1970;										
	NumberOfDays = MonthInDays + ts->day - 1;
	TimeInUnixFormat = NumberOfYears * 31536000 + NumberOfDays * 86400;

	/* Now we need to adjust the number of years for leap
	 * year processing. If we are in Jan or Feb, this year
	 * will never be considered - because we haven't arrived
	 * at then end of Feb right now. [Feb, 29th in a leap year
	 * is handled correctly, because the day (29) is correctly
	 * added to the date serial]
	 */
	if(ts->month < 3)
		NumberOfYears--;

	/*...AND ADDING ONE DAY FOR EACH YEAR WITH 366 DAYS
	 * note that we do not handle 2000 any special, as it was a
	 * leap year. The current code works OK until 2100, when it will
	 * break. As we do not process future dates, we accept that fate...
	 * the whole thing could be refactored by a table-based approach.
	 */
	for(i = 1;i <= NumberOfYears; i++)
	{
	/*	If i = 2 we have 1972, which was a Year with 366 Days
		and if (i + 2) Mod (4) = 0 we have a Year after 1972
		which is also a Year with 366 Days (repeated every 4 Years) */
		if ((i == 2) || (((i + 2) % 4) == 0))
		{	/*Year with 366 Days!!!*/
			TimeInUnixFormat += 86400;
		}	
	}

	/*Add Hours, minutes and seconds */
	TimeInUnixFormat += ts->hour*60*60;
	TimeInUnixFormat += ts->minute*60;
	TimeInUnixFormat += ts->second;
	/* do UTC offset */
	utcOffset = ts->OffsetHour*3600 + ts->OffsetMinute*60;
	if(ts->OffsetMode == '+')
		utcOffset *= -1; /* if timestamp is ahead, we need to "go back" to UTC */
	TimeInUnixFormat += utcOffset;
	return TimeInUnixFormat;
}


/**
 * format a timestamp as a UNIX timestamp; subsecond resolution is
 * discarded.
 * Note that this code can use some refactoring. I decided to use it
 * because mktime() requires an upfront TZ update as it works on local
 * time. In any case, it is worth reconsidering to move to mktime() or
 * some other method.
 * Important: pBuf must point to a buffer of at least 11 bytes.
 * rgerhards, 2012-03-29
 */
int formatTimestampUnix(struct syslogTime *ts, char *pBuf)
{
	snprintf(pBuf, 11, "%u", (unsigned) syslogTime2time_t(ts));
	return 11;
}


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
	pIf->GetTime = getTime;
	pIf->timeval2syslogTime = timeval2syslogTime;
	pIf->ParseTIMESTAMP3339 = ParseTIMESTAMP3339;
	pIf->ParseTIMESTAMP3164 = ParseTIMESTAMP3164;
	pIf->formatTimestampToMySQL = formatTimestampToMySQL;
	pIf->formatTimestampToPgSQL = formatTimestampToPgSQL;
	pIf->formatTimestampSecFrac = formatTimestampSecFrac;
	pIf->formatTimestamp3339 = formatTimestamp3339;
	pIf->formatTimestamp3164 = formatTimestamp3164;
	pIf->formatTimestampUnix = formatTimestampUnix;
	pIf->syslogTime2time_t = syslogTime2time_t;
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
