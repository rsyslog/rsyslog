/*! \file srUtils.h
 *  \brief General, small utilities that fit nowhere else.
 * 
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 * \date    2003-09-09
 *          Coding begun.
 *
 * Copyright 2003-2007 Rainer Gerhards and Adiscon GmbH.
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
#ifndef __SRUTILS_H_INCLUDED__
#define __SRUTILS_H_INCLUDED__ 1


/* syslog names */
#ifndef LOG_MAKEPRI
#	define	LOG_MAKEPRI(fac, pri)	(((fac) << 3) | (pri))
#endif
#define INTERNAL_NOPRI	0x10	/* the "no priority" priority */
#define TABLE_NOPRI	0	/* Value to indicate no priority in f_pmask */
#define TABLE_ALLPRI    0xFF    /* Value to indicate all priorities in f_pmask */
#define	LOG_MARK	LOG_MAKEPRI(LOG_NFACILITIES, 0) /* mark "facility" */

typedef struct syslogName_s {
	char	*c_name;
	int	c_val;
} syslogName_t;

extern syslogName_t syslogPriNames[];
extern syslogName_t syslogFacNames[];

/**
 * A reimplementation of itoa(), as this is not available
 * on all platforms. We used the chance to make an interface
 * that fits us well, so it is no longer plain itoa().
 *
 * This method works with the US-ASCII alphabet. If you port this
 * to e.g. EBCDIC, you need to make a small adjustment. Keep in mind,
 * that on the wire it MUST be US-ASCII, so basically all you need
 * to do is replace the constant '0' with 0x30 ;).
 *
 * \param pBuf Caller-provided buffer that will receive the
 *              generated ASCII string.
 *
 * \param iLenBuf Length of the caller-provided buffer.
 *
 * \param iToConv The integer to be converted.
 */
rsRetVal srUtilItoA(char *pBuf, int iLenBuf, number_t iToConv);

/**
 * A method to duplicate a string for which the length is known.
 * Len must be the length in characters WITHOUT the trailing
 * '\0' byte.
 * rgerhards, 2007-07-10
 */
unsigned char *srUtilStrDup(unsigned char *pOld, size_t len);
/**
 * A method to create a directory and all its missing parents for
 * a given file name. Please not that the rightmost element is
 * considered to be a file name and thus NO directory is being created
 * for it.
 * added 2007-07-17 by rgerhards
 */
int makeFileParentDirs(uchar *szFile, size_t lenFile, mode_t mode, uid_t uid, gid_t gid, int bFailOnChown);
int execProg(uchar *program, int bWait, uchar *arg);
void skipWhiteSpace(uchar **pp);
rsRetVal genFileName(uchar **ppName, uchar *pDirName, size_t lenDirName, uchar *pFName,
		     size_t lenFName, long lNum, int lNumDigits);
int getNumberDigits(long lNum);
rsRetVal timeoutComp(struct timespec *pt, long iTimeout);
long timeoutVal(struct timespec *pt);
void mutexCancelCleanup(void *arg);
void srSleep(int iSeconds, int iuSeconds);
char *rs_strerror_r(int errnum, char *buf, size_t buflen);
int decodeSyslogName(uchar *name, syslogName_t *codetab);

/* mutex operations */
/* some macros to cancel-safe lock a mutex (it will automatically be released
 * when the thread is cancelled. This needs to be done as macros because
 * pthread_cleanup_push sometimes is a macro that can not be used inside a function.
 * It's a bit ugly, but works well... rgerhards, 2008-01-20
 */
#define	DEFVARS_mutex_cancelsafeLock int iCancelStateSave
#define mutex_cancelsafe_lock(mut) \
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave); \
		d_pthread_mutex_lock(mut); \
		pthread_cleanup_push(mutexCancelCleanup, mut); \
		pthread_setcancelstate(iCancelStateSave, NULL);
#define mutex_cancelsafe_unlock(mut) pthread_cleanup_pop(1)

/* some useful constants */
#define MUTEX_ALREADY_LOCKED	0
#define LOCK_MUTEX		1
#define DEFVARS_mutexProtection\
	int iCancelStateSave; \
	int bLockedOpIsLocked=0
#define BEGIN_MTX_PROTECTED_OPERATIONS(mut, bMustLock) \
	if(bMustLock == LOCK_MUTEX) { \
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &iCancelStateSave); \
		d_pthread_mutex_lock(mut); \
		bLockedOpIsLocked = 1; \
	}
#define END_MTX_PROTECTED_OPERATIONS(mut) \
	if(bLockedOpIsLocked) { \
		d_pthread_mutex_unlock(mut); \
		pthread_setcancelstate(iCancelStateSave, NULL); \
	}
#endif
