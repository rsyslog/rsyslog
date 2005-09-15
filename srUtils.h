/*! \file srUtils.h
 *  \brief General, small utilities that fit nowhere else.
 * 
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 * \date    2003-09-09
 *          Coding begun.
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
#ifndef __LIB3195_SRUTILS_H_INCLUDED__
#define __LIB3195_SRUTILS_H_INCLUDED__ 1


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
rsRetVal srUtilItoA(char *pBuf, int iLenBuf, int iToConv);

#endif
