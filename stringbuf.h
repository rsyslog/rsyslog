/*! \file stringbuf.h
 *  \brief The dynamic stringt buffer helper object.
 *
 * The string buffer object is a slim string handler. It implements
 * a dynamically growing string and can be used whereever data needs
 * to be appended to a string AND it is not known how large the
 * resulting structure is. If you know the string size or can
 * retrieve it quickly, you should NOT use the string buffer
 * object - because it has some overhead not associated with direct
 * string manipulations.
 *
 * This object is used to grow a string. For performance reasons,
 * the termination \0 byte is only written after the caller 
 * indicates the string is completed.
 *
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 * \date    2003-08-08
 *          Initial version  begun.
 *
 * Copyright 2002-2003 
 *     Rainer Gerhards and Adiscon GmbH. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Adiscon GmbH or Rainer Gerhards
 *       nor the names of its contributors may be used to
 *       endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __LIB3195_STRINGBUF_H_INCLUDED__
#define __LIB3195_STRINGBUF_H_INCLUDED__ 1

#define sbSTRBCHECKVALIDOBJECT(x) {assert(x != NULL); assert(x->OID == OIDsbStrB);}


/** 
 * The dynamic string buffer object.
 *
 */
struct sbStrBObject
{	
	srObjID OID;			/**< object ID */
	char *pBuf;			/**< pointer to the string buffer, may be NULL if string is empty */
	int iBufSize;			/**< current maximum size of the string buffer */
	int iBufPtr;			/**< pointer (index) of next character position to be written to. */
	int iAllocIncrement;	/**< the amount of bytes the string should be expanded if it needs to */
};
typedef struct sbStrBObject sbStrBObj;


/**
 * Construct a sbStrB object.
 */
sbStrBObj *sbStrBConstruct(void);

/**
 * Destruct the string buffer object.
 */
void sbStrBDestruct(sbStrBObj *pThis);

/**
 * Append a character to an existing string. If necessary, the
 * method expands the string buffer.
 *
 * \param c Character to append to string.
 */
srRetVal sbStrBAppendChar(sbStrBObj *pThis, char c);

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
char* sbStrBFinish(sbStrBObj *pThis);

/**
 * Append a string to the buffer.
 *
 * \param psz pointer to string to be appended. Must not be NULL.
 */
srRetVal sbStrBAppendStr(sbStrBObj *pThis, char* psz);

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
void sbStrBSetAllocIncrement(sbStrBObj *pThis, int iNewIncrement);

/**
 * Append an integer to the string. No special formatting is
 * done.
 */
srRetVal sbStrBAppendInt(sbStrBObj *pThis, int i);


#endif
