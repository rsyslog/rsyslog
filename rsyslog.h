/* Header file with global definitions for the whole
 * rsyslog project (including all subprojects like
 * rfc3195d).
 * Begun 2005-09-15 RGerhards
*/
#ifndef INCLUDED_RSYSLOG_H
#define INCLUDED_RSYSLOG_H

/* ############################################################# *
 * #                    Config Settings                        # *
 * ############################################################# */
#define RS_STRINGBUF_ALLOC_INCREMENT 128

/* ############################################################# *
 * #                  End Config Settings                      # *
 * ############################################################# */

#ifndef	NOLARGEFILE
#	undef _LARGEFILE_SOURCE  
#	undef _LARGEFILE64_SOURCE  
#	undef _FILE_OFFSET_BITS
#	define _LARGEFILE_SOURCE  
#	define _LARGEFILE64_SOURCE  
#	define _FILE_OFFSET_BITS 64
#endif

/* The error codes below are orginally "borrowed" from
 * liblogging. As such, we reserve values up to -2999
 * just in case we need to borrow something more ;)
*/
enum rsRetVal_				/** return value. All methods return this if not specified otherwise */
{
	RS_RET_NOT_IMPLEMENTED = -7,	/**< implementation is missing (probably internal error or lazyness ;)) */
	RS_RET_OUT_OF_MEMORY = -6,	/**< memory allocation failed */
	RS_RET_PROVIDED_BUFFER_TOO_SMALL = -50,/**< the caller provided a buffer, but the called function sees the size of this buffer is too small - operation not carried out */
	RS_RET_TRUE = -1,
	RS_RET_FALSE = -2,
	RS_RET_ERR = -3000,	/**< generic failure */
	RS_TRUNCAT_TOO_LARGE = -3001, /**< truncation operation where too many chars should be truncated */
	RS_RET_FOUND_AT_STRING_END = -3002, /**< some value found, but at the last pos of string */
	RS_RET_NOT_FOUND = -3003, /**< some requested value not found */
	RS_RET_MISSING_TRAIL_QUOTE = -3004, /**< an expected trailing quote is missing */
	RS_RET_NO_DIGIT = -3005,	/**< an digit was expected, but none found (mostly parsing) */
	RS_RET_NO_MORE_DATA = -3006,	/**< insufficient data, e.g. end of string during parsing */
	RS_RET_INVALID_IP = -3007,	/**< invalid ip found where valid was expected */
	RS_RET_OBJ_CREATION_FAILED = - 3008, /**< the creation of an object failed (no details available) */
	RS_RET_PARAM_ERROR = -1000,	/**< invalid parameter in call to function */
	RS_RET_MISSING_INTERFACE = -1001,/**< interface version mismatch, required missing */
	/* return states for config file processing */
	RS_RET_NONE = -2000,		/**< some value is not available - not necessarily an error */
	RS_RET_CONFLINE_UNPROCESSED = -2001,/**< config line was not processed, pass to other module */
	RS_RET_DISCARDMSG = -2002,	/**< discard message (no error state, processing request!) */
	RS_RET_INCOMPATIBLE = -2003,	/**< function not compatible with requested feature */
	RS_RET_NOENTRY = -2004,		/**< do not create an entry for (whatever) - not necessary an error */
	RS_RET_NO_SQL_STRING = -2005,	/**< string is not suitable for use as SQL */
	RS_RET_DISABLE_ACTION = -2006,  /**< action requests that it be disabled */
	RS_RET_SUSPENDED = -2007,  /**< something was suspended, not neccesarily an error */
	RS_RET_RQD_TPLOPT_MISSING = -2008,/**< a required template option is missing */
	RS_RET_INVALID_VALUE = -2009,/**< some value is invalid (e.g. user-supplied data) */
	RS_RET_INVALID_INT = -2010,/**< invalid integer */
	RS_RET_INVALID_CMD = -2011,/**< invalid command */
	RS_RET_VAL_OUT_OF_RANGE = -2012, /**< value out of range */
	RS_RET_FOPEN_FAILURE = -2013,	/**< failure during fopen, for example file not found - see errno */
	RS_RET_END_OF_LINKEDLIST = -2014,	/**< end of linked list, not an error, but a status */
	RS_RET_CHAIN_NOT_PERMITTED = -2015, /**< chaining (e.g. of config command handlers) not permitted */
	RS_RET_INVALID_PARAMS = -2016,/**< supplied parameters are invalid */
	RS_RET_EMPTY_LIST = -2017, /**< linked list is empty */
	RS_RET_FINISHED = -2018, /**< some opertion is finished, not an error state */
	RS_RET_INVALID_SOURCE = -2019, /**< source (address) invalid for some reason */
	RS_RET_ADDRESS_UNKNOWN = -2020, /**< an address is unknown - not necessarily an error */
	RS_RET_MALICIOUS_ENTITY = -2021, /**< there is an malicious entity involved */
	RS_RET_OK_DELETE_LISTENTRY = 1,	/**< operation successful, but callee requested the deletion of an entry (special state) */
	RS_RET_OK = 0			/**< operation successful */
};
typedef enum rsRetVal_ rsRetVal; /**< friendly type for global return value */

/* some helpful macros to work with srRetVals.
 * Be sure to call the to-be-returned variable always "iRet" and
 * the function finalizer always "finalize_it".
 */
#define CHKiRet(code) if((iRet = code) != RS_RET_OK) goto finalize_it
/* macro below is to be used if we need our own handling, eg for cleanup */
#define CHKiRet_Hdlr(code) if((iRet = code) != RS_RET_OK)
/* macro below is used in conjunction with CHKiRet_Hdlr, else use ABORT_FINALIZE */
#define FINALIZE goto finalize_it;
#define DEFiRet rsRetVal iRet = RS_RET_OK
#define ABORT_FINALIZE(errCode)			\
	do {					\
		iRet = errCode;			\
		goto finalize_it;		\
	} while (0)

/** Object ID. These are for internal checking. Each
 * object is assigned a specific ID. This is contained in
 * all Object structs (just like C++ RTTI). We can use 
 * this field to see if we have been passed a correct ID.
 * Other than that, there is currently no other use for
 * the object id.
 */
enum rsObjectID
{
	OIDrsFreed = -1,		/**< assigned, when an object is freed. If this
				 *   is seen during a method call, this is an
				 *   invalid object pointer!
				 */
	OIDrsInvalid = 0,	/**< value created by calloc(), so do not use ;) */
	/* The 0xEFCD is a debug aid. It helps us find object IDs in memory
	 * dumps (on X86, this is CDEF in the dump ;)
	 * If you are on an embedded device and you would like to save space
	 * make them 1 byte only.
	 */
	OIDrsCStr = 0xEFCD0001,
	OIDrsPars = 0xEFCD0002
};
typedef enum rsObjectID rsObjID;

/* support to set object types */
#ifdef NDEBUG
#define rsSETOBJTYPE(pObj, type)
#define rsCHECKVALIDOBJECT(x, type)
#else
#define rsSETOBJTYPE(pObj, type) pObj->OID = type;
#define rsCHECKVALIDOBJECT(x, type) {assert(x != NULL); assert(x->OID == type);}
#endif

/**
 * This macro should be used to free objects. 
 * It aids in interpreting dumps during debugging.
 */
#ifdef NDEBUG
#define RSFREEOBJ(x) free(x)
#else
#define RSFREEOBJ(x) {(x)->OID = OIDrsFreed; free(x);}
#endif

/* get rid of the unhandy "unsigned char"
 */
typedef unsigned char uchar;

/* for the time being, we do our own portability handling here. It
 * looks like autotools either does not yet support checks for it, or
 * I wasn't smart enough to find them ;) rgerhards, 2007-07-18
 */
#ifndef __GNUC__
#  define  __attribute__(x)  /*NOTHING*/
#endif

#endif /* multi-include protection */
/*
 * vi:set ai:
 */
