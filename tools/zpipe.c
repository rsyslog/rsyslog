/* zpipe.c: example of proper use of zlib's inflate() and deflate()
   Not copyrighted -- provided to the public domain
   Version 1.5  11 December 2005  Mark Adler 
   Version 2.0  03 June     2009  Rainer Gerhards */

/* RSYSLOG NOTE:
 * This file is primarily been used as a testing aid for rsyslog. We do NOT
 * properly maintain it and it has been brought to our attention that it may
 * have some security issues. However, we prefer not to remove the file as it
 * may turn out to be useful for further testing. All users are advised NOT
 * to base any development on this version here, but rather look for the
 * original zpipe.c by the authors mentioned above.
 *
 * This file is beeing distributed as part of rsyslog, but is just an
 * add-on. Most importantly, rsyslog's copyright does not apply but
 * rather the (non-) copyright stated above.
 */

/* Version history:
   1.0  30 Oct 2004  First version
   1.1   8 Nov 2004  Add void casting for unused return values
                     Use switch statement for inflate() return values
   1.2   9 Nov 2004  Add assertions to document zlib guarantees
   1.3   6 Apr 2005  Remove incorrect assertion in inf()
   1.4  11 Dec 2005  Add hack to avoid MSDOS end-of-line conversions
                     Avoid some compiler warnings for input and output buffers
   2.0  03 Jun 2009  Add hack to support multiple deflate records inside a single
   		     file on inflate. This is needed in order to support reading
		     files created by rsyslog's zip output writer.
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zlib.h"

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
int def(FILE *source, FILE *dest, int level)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, level);
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);
    return Z_OK;
}


/* initialize stream for deflating (we need this in case of 
 * multiple records.
 * rgerhards, 2009-06-03
 */
int doInflateInit(z_stream *strm)
{
    int ret;

    /* allocate inflate state */
    strm->zalloc = Z_NULL;
    strm->zfree = Z_NULL;
    strm->opaque = Z_NULL;
    strm->avail_in = 0;
    strm->next_in = Z_NULL;
    ret = inflateInit(strm);
    return ret;
}


/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int inf(FILE *source, FILE *dest)
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    int len;
    unsigned char *next_in_save;
    unsigned char out[CHUNK];

    ret = doInflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        len = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (len == 0) {
            break;
	}
        strm.avail_in = len;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        strm.avail_out = CHUNK;
        strm.next_out = out;
        do {
	/*	fprintf(stderr, "---inner LOOP---, avail_in %d, avail_out %d Byte 0: %x, 1: %x\n", strm.avail_in, strm.avail_out, *strm.next_in, *(strm.next_in+1));*/
		do {
		    ret = inflate(&strm, Z_NO_FLUSH);
		    assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
		    switch (ret) {
		    case Z_NEED_DICT:
			ret = Z_DATA_ERROR;     /* and fall through */
		    case Z_DATA_ERROR:
		    case Z_MEM_ERROR:
			(void)inflateEnd(&strm);
			return ret;
		    }
		    have = CHUNK - strm.avail_out;
		    if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
			(void)inflateEnd(&strm);
			return Z_ERRNO;
		    }
		} while (strm.avail_out == 0);
		/* handle the case that more than one deflate record is contained
		 * in a single file. -- rgerhards, 2009-06-03
		 */
		if(ret == Z_STREAM_END) {
			len -= strm.total_in;
			if(len > 0) {
				next_in_save = strm.next_in;
				(void)inflateEnd(&strm);
				ret = doInflateInit(&strm);
				if (ret != Z_OK)
				    return ret;
				strm.avail_in = len;
				strm.next_in = next_in_save;
				strm.avail_out = CHUNK;
				strm.next_out = out;
				ret = Z_OK; /* continue outer loop */
			}
		}
	} while (strm.avail_in > 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

/* report a zlib or i/o error */
void zerr(int ret)
{
    fputs("zpipe: ", stdout);
    switch (ret) {
    case Z_ERRNO:
        if (ferror(stdin))
            fputs("error reading stdin\n", stdout);
        if (ferror(stdout))
            fputs("error writing stdout\n", stdout);
        break;
    case Z_STREAM_ERROR:
        fputs("invalid compression level\n", stdout);
        break;
    case Z_DATA_ERROR:
        fputs("invalid or incomplete deflate data\n", stdout);
        break;
    case Z_MEM_ERROR:
        fputs("out of memory\n", stdout);
        break;
    case Z_VERSION_ERROR:
        fputs("zlib version mismatch!\n", stdout);
    }
}

/* compress or decompress from stdin to stdout */
int main(int argc, char **argv)
{
    int ret;

    /* avoid end-of-line conversions */
    SET_BINARY_MODE(stdin);
    SET_BINARY_MODE(stdout);

    /* do compression if no arguments */
    if (argc == 1) {
        ret = def(stdin, stdout, Z_DEFAULT_COMPRESSION);
        if (ret != Z_OK)
            zerr(ret);
        return ret;
    }

    /* do decompression if -d specified */
    else if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        ret = inf(stdin, stdout);
        if (ret != Z_OK)
            zerr(ret);
        return ret;
    }

    /* otherwise, report usage */
    else {
        fputs("zpipe usage: zpipe [-d] < source > dest\n", stdout);
        return 1;
    }
}
