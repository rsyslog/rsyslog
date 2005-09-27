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
/* from liblogging config.h */
#define STRINGBUF_ALLOC_INCREMENT 128

/* ############################################################# *
 * #                  End Config Settings                      # *
 * ############################################################# */

#ifndef	NOLARGEFILE
#	define _GNU_SOURCE
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
	RS_RET_OUT_OF_MEMORY = -6,	/**< memory allocation failed */
	RS_RET_PROVIDED_BUFFER_TOO_SMALL = -50,/**< the caller provided a buffer, but the called function sees the size of this buffer is too small - operation not carried out */
	RS_RET_ERR = -3000,	/**< generic failure */
	RS_TRUNCAT_TOO_LARGE = -3001, /**< truncation operation where too many chars should be truncated */
	RS_RET_FOUND_AT_STRING_END = -3002, /**< some value found, but at the last pos of string */
	RS_RET_NOT_FOUND = -3003, /**< some requested value not found */
	RS_RET_MISSING_TRAIL_QUOTE = -3004, /**< an expected trailing quote is missing */
	RS_RET_NO_DIGIT = -3005,	/**< an digit was expected, but none found (mostly parsing) */
	RS_RET_NO_MORE_DATA = -3006,	/**< insufficient data, e.g. end of string during parsing */
	RS_RET_INVALID_IP = -3007,	/**< invalid ip found where valid was expected */
	RS_RET_OK = 0			/**< operation successful */
};
typedef enum rsRetVal_ rsRetVal; /**< friendly type for global return value */


/** Object ID. These are for internal checking. Each
 * object is assigned a specific ID. This is contained in
 * all Object structs (just like C++ RTTI). We can use 
 * this field to see if we have been passed a correct ID.
 * Other than that, there is currently no other use for
 * the object id.
 */
enum rsObjectID
{
	OID_Freed = -1,		/**< assigned, when an object is freed. If this
				 *   is seen during a method call, this is an
				 *   invalid object pointer!
				 */
	OID_Invalid = 0,	/**< value created by calloc(), so do not use ;) */
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
#define RSFREEOBJ(x) {(x)->OID = OID_Freed; free(x);}
#endif
#endif
