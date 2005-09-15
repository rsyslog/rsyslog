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
	/* start next error at -3000 */
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

/**
 * This macro should be used to free objects. 
 * It aids in interpreting dumps during debugging.
 */
#if DEBUGLEVEL > 0
#define RSFREEOBJ(x) {(x)->OID = OID_Freed; free(x);}
#else
#define RSFREEOBJ(x) free(x)
#endif
#endif
