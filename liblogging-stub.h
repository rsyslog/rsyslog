/*! \file liblogging.h
 *  \brief global objects and defines
 *
 * THIS IS A STUB MODULE TAKEN FROM LIBLOGGING. It will be replaced
 * once liblogging is fully integrated in rsyslog. rgerhards 2004-11-17.
 *
 * Include this header in your application!
 *
 * \author  Rainer Gerhards <rgerhards@adiscon.com>
 * \date    2003-08-04
 *          0.1 version created.
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
#ifndef __LIB3195_LIBLOGGING_H_INCLUDED__
#define __LIB3195_LIBLOGGING_H_INCLUDED__ 1
#include <stdio.h> /* debug only */

#ifdef __cplusplus
extern "C" {
#endif


enum srRetVal_				/** return value. All methods return this if not specified otherwise */
{
	SR_RET_ERR = -1,		/**< error occured, call error handler for details */
	SR_RET_REMAIN_WIN_TOO_SMALL = -2,	/**< can't send, because not enough octets left in RFC3081 window --> wait for SEQ */
	SR_RET_INVALID_HANDLE = -3,	/**< caller provided an invalid handle, e.g. NULL pointer or pointer to wrong object. 
								 *   This is only checked on the API layer. Internally, all functions check this via
								 *   assert(), so protection is only available during debugging. This is a performance
								 *   decision.
								 */
	SR_RET_INVALID_DESTRUCTOR = -4, /**< used, when the caller must provide a pointer to a destructor function but does not */
	SR_RET_NOT_FOUND = - 5,		/**< a requested element was not found. May not be an error depending on what operation was
								 **    to be carried out... */
    SR_RET_OUT_OF_MEMORY = -6,	/**< memory allocation failed */
    SR_RET_XML_INVALID_PARAMTAG = -7,	/**< malformed tag parameter, most probably quotes are missing before value or intervening space */
    SR_RET_XML_INVALID_TERMINATOR = -8,	/**< missing terminator sequence, e.g. a "param='no_terminator" - notice the missing closing quote */
	SR_RET_XML_MALFORMED = -9,	/**< the XML is malformed (no specific reason given) */
	SR_RET_MISSING_CLOSE_BRACE = -10,	/**< closing brace in <tag /> missing */
	SR_RET_XML_MISSING_CLOSETAG = -11,	/**< close tag is missing or syntax invalid <tag>...</closetag> */
	SR_RET_XML_MISSING_OPENTAG = -12,	/**< open tag is missing, e.g <tag />othertag> */
	SR_RET_XML_TAG_MISMATCH = -13,	/**< start and close tag name do not match (<tag>...</othertag>) */
	SR_RET_XML_INVALID_CDATA_HDR = -14, /**< invalid start of CDATA, e.g. <![CDTA... */
	SR_RET_XML_INVALID_CDATA_TRAIL = -14, /**< invalid CDATA trailer, , e.g. "<![CDATA[test]>" (one ] missing)  */
	SR_RET_PEER_NONOK_RESPONSE = -15, /**< was waiting for a BEEP peer to respond "ok", but received other response */
	SR_RET_PEER_INVALID_PROFILE = -16, /**< BEEP Peer did either not support the profile or did not return the correct on in response to a start message */
	SR_RET_PEER_NO_URI = -17,	/**< BEEP peer did not inclulde a mandatory URI element in its xml response */
	SR_RET_PEER_NO_PROFILE = -18,/**< BEEP peer did not include a mandatory PROFILE element in its XML reponse */
	SR_RET_PEER_NO_GREETING	= -19, /**< BEEP peer did not send a (correctly formatted) greeting message */
	SR_RET_PEER_DOESNT_SUPPORT_PROFILE = -20, /**< the reqeusted profile is not part of the peers profile set (from greeting message) */
	SR_RET_INVALID_FRAME_STATE = -21,	/**< a frame object has an invalid frame state (pFram->iState) */
	SR_RET_PROFILE_ALREADY_SET = -22,	/**< a profile should be assgined to a channel which already has one */
	SR_RET_INVALID_CHAN_STATE = -23,	/**< a channel object has an invalid or unexpected (at this point) channel state */
	SR_RET_INVALID_GREETING = -24,		/**< greeting message is (somehow) invalid, e.g. does not start with RPY) */
	SR_RET_INVALID_CHAN0_MESG = -25,	/**< channel 0 received an invalid or out-of-sequence message */
	SR_RET_START_MISSING_NUMBER = -26,	/**< a start message is missing the "number=" parameter */
	SR_RET_START_INVALID_NUMBER = -27,	/**< a start message's "number=" parameter specifies an invalid number */
	SR_RET_START_EXISTING_NUMBER = -28,	/**< a start message's "number=" parameter specifies a channel number which already exisits! */
	SR_RET_NO_VALUE = -29,				/**< a linked list was asked to provide an uValue, but there was none. Not necessarily an error, depends on caller's needs. */
	SR_RET_START_EVEN_NUMBER = -30,		/**< an initiator sent a even-numbered start number, which is invalid as of rfc 3080 2.3.1.2 */
	SR_RET_NO_PROFILE_RQSTD = -31,		/**< no profile at all was requested in a start message (may be caused by invalid format) */
	SR_RET_WARNING_START_NO_PROFMATCH = -32,/**< WARNING only (no error) - during a start, no matching profile was found and the beep peer has been sent such an ERR frame */
	SR_RET_ERR_EVENT_HANDLER_MISSING = -33,/**< a required (profile) event handler is missing */
	SR_RET_ACKNO_ZERO = -34,			/**< caller-provided ackno is zero. This can not be by design. */
	SR_RET_CHAN_DOESNT_EXIST = -35,		/**< a chanel specified does not exist */
	SR_RET_ALREADY_LISTENING = -36,		/**< caller tried to start an listener on the API object, but it is already one running */
	SR_RET_INVALID_OPTVAL = -37,		/**< invalid option value was provided to srAPISetOption() (param 2) */
	SR_RET_INVALID_LIB_OPTION = -38,	/**< invalid option was provided to srAPISetOption() (param 1) */
	SR_RET_NULL_POINTER_PROVIDED = -39,	/**< the caller has provided a NULL-Pointer where none were expected/allowed */
	SR_RET_PROPERTY_NOT_AVAILABLE = -40,/**< a property asked for is not available, e.g. the TIMESTAMP in a not-wellformed message */
	SR_RET_UNSUPPORTED_FORMAT = -41,	/**< a non-supported format was requested by the caller */
	SR_RET_UNALLOCATABLE_BUFFER = -42,	/**< the operation can not be completed because a buffer that is under user control would need to be deallocated */
	SR_RET_PRIO_OUT_OF_RANGE = -43,		/**< a priority value is outside the allowed range */
	SR_RET_FACIL_OUT_OF_RANGE = -44,	/**< a facility value is outside the allowed range */
	SR_RET_INVALID_TAG = -45,			/**< a syslog TAG value (string) provided was invalid, e.g. over 32 chars or included invalid chars */
	SR_RET_NULL_MSG_PROVIDED = -46,		/**< caller provided a NULL pointer where a pointer to a MSG was expected */
	SR_RET_ERR_RECEIVE = -47,			/**< an error occured during message receive. no more detail is available */
	SR_RET_UNEXPECTED_HDRCMD = -48,		/**< an invalid HDR Command was received (e.g. an "MSG" in response to another "MSG") */
	SR_RET_PEER_INDICATED_ERROR = -49,	/**< the PEER send an error response */
	SR_RET_PROVIDED_BUFFER_TOO_SMALL = -50,/**< the caller provided a buffer, but the called function sees the size of this buffer is too small - operation not carried out */
	SR_RET_INVALID_PARAM = -51,			/**< an invalid parameter was provided to a method */

	/* socket error codes */
	SR_RET_SOCKET_ERR = -1001,			/**< generic error generated by socket subsystem */
	SR_RET_CANT_BIND_SOCKET = -1002,	/**< socket bind() operation failed */
	SR_RET_INVALID_SOCKET = -1003,		/**< invalid socket */
	SR_RET_CONNECTION_CLOSED = -1004,	/**< the remote peer closed the connection */
	SR_RET_INVALID_OS_SOCKETS_VERSION = -1005,/**< the operating system socket subsystem version is inappropriate (this error will most probably occur under Win32, only) */
	SR_RET_CAN_NOT_INIT_SOCKET = -1006,	/**< socket() failed for whatever reason... */
	SR_RET_UXDOMSOCK_CHMOD_ERR = -1007,	/**< chmod() failed on Unix Domain Socket */

	/* BEEP frame format & other errors */
	SR_RET_INVALID_HDRCMD = -2001,			/**< invalid HDRCMD (e.g. "MSG", "RPY",...) */
	SR_RET_INVALID_WAITING_SP_CHAN = -2002,	/**< invalid SP before channo */
	SR_RET_INVALID_CHANNO = -2003,			/**< invalid channo */
	SR_RET_INVALID_WAITING_SP_MSGNO = -2004,/**< now the space before the next header */
	SR_RET_INVALID_IN_MSGNO = -2005,		/**< and the next (numeric) header */
	SR_RET_INVALID_WAITING_SP_MORE = -2006,	/**< now the space before the next header */
	SR_RET_INVALID_IN_MORE = -2007,			/**< and the next (char) header */
	SR_RET_INVALID_WAITING_SP_SEQNO = -2008,/**< now the space before the next header */
	SR_RET_INVALID_IN_SEQNO = -2009,		/**< and the next (numeric) header */
	SR_RET_INVALID_WAITING_SP_SIZE = -2010,	/**< now the space before the next header */
	SR_RET_INVALID_IN_SIZE = -2011,			/**< and the next (numeric) header */
	SR_RET_INVALID_WAITING_SP_ANSNO = -2012,/**< now the space before the next header */
	SR_RET_INVALID_IN_ANSNO = -2013,		/**< and the next (numeric) header */
	SR_RET_INVALID_WAITING_HDRCR = -2014,	/**< awaiting the HDR's CR */
	SR_RET_INVALID_WAITING_HDRLF = -2015,	/**< awaiting the HDR's LF */
	SR_RET_INVALID_IN_PAYLOAD = -2016,		/**< reading payload area */
	SR_RET_INVALID_WAITING_END1 = -2017,	/**< waiting for the 1st HDR character */
	SR_RET_INVALID_WAITING_END2 = -2018,	/**< waiting for the 2nd HDR character */
	SR_RET_INVALID_WAITING_END3 = -2019,	/**< waiting for the 3rd HDR character */
	SR_RET_INVALID_WAITING_END4 = -2020,	/**< waiting for the 4th HDR character */
	SR_RET_INVALID_WAITING_END5 = -2021,	/**< waiting for the 5th HDR character */
	SR_RET_INVALID_WAITING_SP_ACKNO = -2022,/**< now the space before the next header */
	SR_RET_INVALID_WAITING_SP_WINDOW = -2023,/**< now the space before the next header */
	SR_RET_INAPROPRIATE_HDRCMD = -2024,		/**< the beep header was syntactically correct, but could not be used semantically */
	SR_RET_OVERSIZED_FRAME = -2025,			/**< the frame's "size" field specifies a size that does not fit into the current windows. Eventually malicous. */

	/* and finally the OK state... */
	SR_RET_OK = 0			/**< operation successful */
};
/** friendly type for global return value */
typedef enum srRetVal_ srRetVal;


typedef unsigned SBchannel; /**< our own type for channel numbers */
typedef unsigned SBseqno; /**< our own type for sequence numbers */
typedef unsigned SBansno; /**< our own type for answer numbers */
typedef unsigned SBsize; /**< our own type for frame size */
typedef unsigned SBmsgno; /**< our own type for message numbers */
typedef unsigned SBackno; /**< our own type for acknowledgment numbers */
typedef unsigned SBwindow; /**< our own type for window sizes*/


/** Object ID. These are for internal checking. Each
 * object is assigned a specific ID. This is contained in
 * all Object structs (just like C++ RTTI). We can use 
 * this field to see if we have been passed a correct ID.
 * Other than that, there is currently no other use for
 * the object id.
 */
enum srObjectID
{
	OID_Freed = -1,		/**< assigned, when an object is freed. If this
						 *   is seen during a method call, this is an
						 *   invalid object pointer!
						 */
	OID_Invalid = 0,	/**< value created by calloc(), so do not use ;) */
	/* The 0xCDAB is a debug aid. It helps us find object IDs in memory
	 * dumps (on X86, this is ABCD in the dump ;)
	 * If you are on an embedded device and you would like to save space
	 * make them 1 byte only.
	 */
	OIDsbFram = 0xCDAB0001,
	OIDsbChan = 0xCDAB0002,
	OIDsbMesg = 0xCDAB0003,
	OIDsbSess = 0xCDAB0004,
	OIDsbSock = 0xCDAB0005,
	OIDsbProf = 0xCDAB0006,
	OIDsrAPI  = 0xCDAB0007,
	OIDsrSLMG = 0xCDAB0008,
	OIDsbNVTR = 0xCDAB0009,
	OIDsbNVTE = 0xCDAB000A,
	OIDsbStrB = 0xCDAB000B,
	OIDsbLstn = 0xCDAB000C,
	OIDsbPSSR = 0xCDAB000D,
	OIDsbPSRC = 0xCDAB000E
};
typedef enum srObjectID srObjID;


/**
 * Allowed options for 3195 client profile
 * selection.
 */
enum srOPTION3195Profiles_
{
	USE_3195_PROFILE_ANY = 1,
	USE_3195_PROFILE_RAW_ONLY = 2,
	USE_3195_PROFILE_COOKED_ONLY = 3
};
typedef enum srOPTION3195Profiles_ srOPTION3195Profiles;

/**
 * Library options. Modify library behaviour.
 * Currently, no options are defined.
 */
enum srOPTION
{
	srOPTION_INVALID = 0,		/**< invalid option, if seen, caller made an error */
	/**
	 * This option tells the library if it should instruct the
	 * socket parts of it to call the OS socket initialiser.
	 * Under Win32, for example, this is only allowed once per
	 * process. So if the lib is integrated into a process that 
	 * already uses sockets, the sockets initializer (WSAStartup()
     * in this case) should not be called.
	 * This is a global setting that NEEDS TO BE SET BEFORE the API 
	 * object is created!!!!
	 */
	srOPTION_CALL_OS_SOCKET_INITIALIZER = 1,
	/**
	 * This option tells the library which RFC 3195 Profile
	 * should be used when talking to the remote peer. This
	 * option so far is only used in the client parts of
	 * liblogging. See \ref srOPTION3195Profiles for
	 * allowed values.
	 */
	 srOPTION_3195_ALLOWED_CLIENT_PROFILES = 2,
	 /**
	  * This options tells the lib if a UDP listener should
	  * be started. Option value must be TRUE or FALSE.
	  */
	 srOPTION_LISTEN_UDP = 3,
	 /**
	  * Sets the UDP listener port. Is only used if the
	  * UDP listener is activated.
	  */
     srOPTION_UPD_LISTENPORT = 4,
	 /**
	  * This option tells the lib if a UNIX domain socket
	  * listener should be started. Supported values are
	  * TRUE and FALSE.
	  */
     srOPTION_LISTEN_UXDOMSOCK = 5,
	 /**
	  * Provides a pointer to the UNIX domain socket that
	  * should be listened to (if listening is enabled).
	  */
     srOPTION_UXDOMSOCK_LISTENAME = 6,
	 /**
	  * This option tells the lib if a BEEP
	  * listener should be started. Supported values are
	  * TRUE and FALSE.
	  */
     srOPTION_LISTEN_BEEP = 7,
	 /**
	  * Sets the BEEP listener port. Is only used if the
	  * UDP listener is activated.
	  */
     srOPTION_BEEP_LISTENPORT = 8
};
typedef enum srOPTION SRoption;

/**
 * This macro should be used to free objects. 
 * Microsoft VC automatically sets _DEBUG if compiling
 * with debug settings. For other environments, you may
 * want to add this to a debug makefile.
 */
#if DEBUGLEVEL > 0
#define SRFREEOBJ(x) {(x)->OID = OID_Freed; free(x);}
#else
#define SRFREEOBJ(x) free(x)
#endif

/** 
 * Integer values for the various supported
 * BEEP headers.
 */
enum BEEPHdrID_ 
{
	BEEPHDR_UNKNOWN = 0,
	BEEPHDR_ANS,
	BEEPHDR_ERR,
    BEEPHDR_MSG,
	BEEPHDR_NUL,
	BEEPHDR_RPY,
	BEEPHDR_SEQ
};
typedef enum BEEPHdrID_ BEEPHdrID;

#ifdef __cplusplus
}
#endif

/* from liblogging config.h */
#define STRINGBUF_ALLOC_INCREMENT 128
#endif
