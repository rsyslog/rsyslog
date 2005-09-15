/*! \file stringbuf.h
 *  \brief The counted string object
 *
 * This is the byte-counted string class for rsyslog. It is a replacement
 * for classical \0 terminated string functions. We introduce it in
 * the hope it will make the program more secure, obtain some performance
 * and, most importantly, lay they foundation for syslog-protocol, which
 * requires strings to be able to handle embedded \0 characters.
 *
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 * \date    2005-09-07
 *          Initial version  begun.
 *
 * All functions in this "class" start with rsCStr (rsyslog Counted String).
 * Copyright 2005
 *     Rainer Gerhards and Adiscon GmbH. All Rights Reserved.
 *     This code is placed under the GPL.
 */
#ifndef _STRINGBUF_H_INCLUDED__
#define _STRINGBUF_H_INCLUDED__ 1

#define sbSTRBCHECKVALIDOBJECT(x) {assert(x != NULL); assert(x->OID == OIDrsCStr);}

/** 
 * The dynamic string buffer object.
 */
struct rsCStrObject
{	
	rsObjID OID;		/**< object ID */
	char *pBuf;		/**< pointer to the string buffer, may be NULL if string is empty */
	char *pszBuf;		/**< pointer to the sz version of the string (after it has been created )*/
	int iBufSize;		/**< current maximum size of the string buffer */
	int iBufPtr;		/**< pointer (index) of next character position to be written to. */
	int iStrLen;		/**< length of the string in characters. */
	int iAllocIncrement;	/**< the amount of bytes the string should be expanded if it needs to */
};
typedef struct rsCStrObject rsCStrObj;


/**
 * Construct a rsCStr object.
 */
rsCStrObj *rsCStrConstruct(void);
rsRetVal rsCStrConstructFromszStr(rsCStrObj **ppThis, char *sz);

/**
 * Destruct the string buffer object.
 */
void rsCStrDestruct(rsCStrObj *pThis);

/**
 * Append a character to an existing string. If necessary, the
 * method expands the string buffer.
 *
 * \param c Character to append to string.
 */
rsRetVal rsCStrAppendChar(rsCStrObj *pThis, char c);

/**
 * Finish the string buffer. That means, the string
 * is returned to the caller and then the string
 * buffer is destroyed. The caller is reponsible for
 * freeing the returned string pointer.
 *
 * After calling this method, the string buffer object
 * is destroyed and thus the provided handle (pThis)
 * can no longer be used.
 *
 * \retval pointer to \0 terminated string. May be NULL
 *         (empty string) and MUST be free()ed by caller.
 */
void rsCStrFinish(rsCStrObj *pThis);

/**
 * Append a string to the buffer.
 *
 * \param psz pointer to string to be appended. Must not be NULL.
 */
rsRetVal rsCStrAppendStr(rsCStrObj *pThis, char* psz);

/**
 * Set a new allocation incremet. This will influence
 * the allocation the next time the string will be expanded.
 * It can be set and changed at any time. If done immediately
 * after custructing the StrB object, this will also be
 * the inital allocation.
 *
 * \param iNewIncrement The new increment size
 *
 * \note It is possible to use a very low increment, e.g. 1 byte.
 *       This can generate a considerable overhead. We highly 
 *       advise not to use an increment below 32 bytes, except
 *       if you are very well aware why you are doing it ;)
 */
void rsCStrSetAllocIncrement(rsCStrObj *pThis, int iNewIncrement);

/**
 * Append an integer to the string. No special formatting is
 * done.
 */
rsRetVal rsCStrAppendInt(rsCStrObj *pThis, int i);


char*  rsCStrConvSzStrAndDestruct(rsCStrObj *pThis);
int rsCStrLen(rsCStrObj *pThis);
#endif
