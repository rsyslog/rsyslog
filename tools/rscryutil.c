/* This is a tool for processing rsyslog encrypted log files.
 *
 * Copyright 2013-2019 Adiscon GmbH
 *
 * This file is part of rsyslog.
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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either exprs or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rsyslog.h"
#include "libcry_common.h"


#ifdef ENABLE_LIBGCRYPT
    #include <gcrypt.h>
    #include "libgcry.h"
#endif

#ifdef ENABLE_OPENSSL_CRYPTO_PROVIDER
    #include <openssl/evp.h>
    #include "libossl.h"
#endif


#ifdef ENABLE_LIBGCRYPT
typedef struct {
    int cry_algo;
    int cry_mode;
    gcry_cipher_hd_t gcry_chd;
} gcry_data;
#endif

#ifdef ENABLE_OPENSSL_CRYPTO_PROVIDER
typedef struct {
    const EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *chd;
} ossl_data;
#endif

/* rscryutil related config parameters and internals */
typedef struct rscry_config {
    /* General parameters */
    enum { MD_DECRYPT, MD_WRITE_KEYFILE } mode;
    int verbose;
    size_t blkLength;
    char *keyfile;
    char *keyprog;
    int randomKeyLen;
    char *key;
    unsigned keylen;
    int optionForce;

    /* Library specific parameters */
    enum { LIB_GCRY, LIB_OSSL } lib;
    union {
#ifdef ENABLE_LIBGCRYPT
        gcry_data gcry;
#endif
#ifdef ENABLE_OPENSSL_CRYPTO_PROVIDER
        ossl_data ossl;
#endif
    } libData;

} rscry_config;

rscry_config cnf;

static int initConfig(const char *name) {
    cnf.mode = MD_DECRYPT;
    cnf.lib = LIB_GCRY;
    cnf.verbose = 0;
    cnf.blkLength = 0;
    cnf.keyfile = NULL;
    cnf.keyprog = NULL;
    cnf.randomKeyLen = -1;
    cnf.key = NULL;
    cnf.keylen = 0;
    cnf.optionForce = 0;

    /* If no library is set, we are using the default value. gcry must be the last
        so it remains backwards compatible. */
    if (name == NULL) {
#ifdef ENABLE_OPENSSL_CRYPTO_PROVIDER
        name = "ossl";
#endif
#ifdef ENABLE_LIBGCRYPT
        name = "gcry";
#endif
    }

    if (name && strcmp(name, "gcry") == 0) { /* Use the libgcrypt lib */
#ifdef ENABLE_LIBGCRYPT
        cnf.lib = LIB_GCRY;
        cnf.libData.gcry.cry_algo = GCRY_CIPHER_AES128;
        cnf.libData.gcry.cry_mode = GCRY_CIPHER_MODE_CBC;
        cnf.libData.gcry.gcry_chd = NULL;
#else
        fprintf(stderr, "rsyslog was not compiled with libgcrypt support.\n");
        return 1;
#endif
    } else if (name && strcmp(name, "ossl") == 0) { /* Use the openssl lib */
#ifdef ENABLE_OPENSSL_CRYPTO_PROVIDER
        cnf.lib = LIB_OSSL;
        cnf.libData.ossl.cipher = EVP_aes_128_cbc();
        cnf.libData.ossl.chd = NULL;
#else
        fprintf(stderr, "rsyslog was not compiled with libossl support.\n");
        return 1;
#endif
    } else {
        fprintf(stderr, "invalid option for lib: %s\n", name);
        return 1;
    }

    return 0;
}

/* We use some common code which expects rsyslog runtime to be
 * present, most importantly for debug output. As a stand-alone
 * tool, we do not really have this. So we do some dummy defines
 * in order to satisfy the needs of the common code.
 */
int Debug = 0;
#ifndef DEBUGLESS
void r_dbgprintf(const char *srcname __attribute__((unused)), const char *fmt __attribute__((unused)), ...) {};
#endif
void srSleep(int a __attribute__((unused)), int b __attribute__((unused)));
/* prototype (avoid compiler warning) */
void srSleep(int a __attribute__((unused)), int b __attribute__((unused))) {}
/* this is not really needed by any of our code */
long randomNumber(void);
/* prototype (avoid compiler warning) */
long randomNumber(void) {
    return 0l;
}
/* this is not really needed by any of our code */

/* rectype/value must be EIF_MAX_*_LEN+1 long!
 * returns 0 on success or something else on error/EOF
 */
static int eiGetRecord(FILE *eifp, char *rectype, char *value) {
    int r;
    unsigned short i, j;
    char buf[EIF_MAX_RECTYPE_LEN + EIF_MAX_VALUE_LEN + 128];
    /* large enough for any valid record */

    if (fgets(buf, sizeof(buf), eifp) == NULL) {
        r = 1;
        goto done;
    }

    for (i = 0; i < EIF_MAX_RECTYPE_LEN && buf[i] != ':'; ++i)
        if (buf[i] == '\0') {
            r = 2;
            goto done;
        } else
            rectype[i] = buf[i];
    rectype[i] = '\0';
    j = 0;
    for (++i; i < EIF_MAX_VALUE_LEN && buf[i] != '\n'; ++i, ++j)
        if (buf[i] == '\0') {
            r = 3;
            goto done;
        } else
            value[j] = buf[i];
    value[j] = '\0';
    r = 0;
done:
    return r;
}

static int eiCheckFiletype(FILE *eifp) {
    char rectype[EIF_MAX_RECTYPE_LEN + 1];
    char value[EIF_MAX_VALUE_LEN + 1];
    int r;

    if ((r = eiGetRecord(eifp, rectype, value)) != 0) goto done;
    if (strcmp(rectype, "FILETYPE") || strcmp(value, RSGCRY_FILETYPE_NAME)) {
        fprintf(stderr,
                "invalid filetype \"cookie\" in encryption "
                "info file\n");
        fprintf(stderr, "\trectype: '%s', value: '%s'\n", rectype, value);
        r = 1;
        goto done;
    }
    r = 0;
done:
    return r;
}

static int eiGetIV(FILE *eifp, char *iv, size_t leniv) {
    char rectype[EIF_MAX_RECTYPE_LEN + 1];
    char value[EIF_MAX_VALUE_LEN + 1];
    size_t valueLen;
    unsigned short i, j;
    int r;
    unsigned char nibble;

    if ((r = eiGetRecord(eifp, rectype, value)) != 0) goto done;
    if (strcmp(rectype, "IV")) {
        fprintf(stderr,
                "no IV record found when expected, record type "
                "seen is '%s'\n",
                rectype);
        r = 1;
        goto done;
    }
    valueLen = strlen(value);
    if (valueLen / 2 != leniv) {
        fprintf(stderr, "length of IV is %lld, expected %lld\n", (long long)valueLen / 2, (long long)leniv);
        r = 1;
        goto done;
    }

    for (i = j = 0; i < valueLen; ++i) {
        if (value[i] >= '0' && value[i] <= '9')
            nibble = value[i] - '0';
        else if (value[i] >= 'a' && value[i] <= 'f')
            nibble = value[i] - 'a' + 10;
        else {
            fprintf(stderr, "invalid IV '%s'\n", value);
            r = 1;
            goto done;
        }
        if (i % 2 == 0)
            iv[j] = nibble << 4;
        else
            iv[j++] |= nibble;
    }
    r = 0;
done:
    return r;
}

static int eiGetEND(FILE *eifp, off64_t *offs) {
    char rectype[EIF_MAX_RECTYPE_LEN + 1] = "";
    char value[EIF_MAX_VALUE_LEN + 1];
    int r;

    if ((r = eiGetRecord(eifp, rectype, value)) != 0) goto done;
    if (strcmp(rectype, "END")) {
        fprintf(stderr,
                "no END record found when expected, record type "
                "seen is '%s'\n",
                rectype);
        r = 1;
        goto done;
    }
    *offs = atoll(value);
    r = 0;
done:
    return r;
}

/* LIBGCRYPT RELATED STARTS */
#ifdef ENABLE_LIBGCRYPT
static int gcryInit(FILE *eifp) {
    int r = 0;
    gcry_error_t gcryError;
    char iv[4096];

    cnf.blkLength = gcry_cipher_get_algo_blklen(cnf.libData.gcry.cry_algo);  // EVP_CIPHER_CTX_get_block_size
    if (cnf.blkLength > sizeof(iv)) {
        fprintf(stderr,
                "internal error[%s:%d]: block length %lld too large for "
                "iv buffer\n",
                __FILE__, __LINE__, (long long)cnf.blkLength);
        r = 1;
        goto done;
    }
    if ((r = eiGetIV(eifp, iv, cnf.blkLength)) != 0) goto done;

    size_t keyLength = gcry_cipher_get_algo_keylen(cnf.libData.gcry.cry_algo);  // EVP_CIPHER_get_key_length
    assert(cnf.key != NULL); /* "fix" clang 10 static analyzer false positive */
    if (strlen(cnf.key) != keyLength) {
        fprintf(stderr,
                "invalid key length; key is %u characters, but "
                "exactly %llu characters are required\n",
                cnf.keylen, (long long unsigned)keyLength);
        r = 1;
        goto done;
    }

    gcryError = gcry_cipher_open(&cnf.libData.gcry.gcry_chd, cnf.libData.gcry.cry_algo, cnf.libData.gcry.cry_mode, 0);
    if (gcryError) {
        fprintf(stderr, "gcry_cipher_open failed:  %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
        r = 1;
        goto done;
    }

    gcryError = gcry_cipher_setkey(cnf.libData.gcry.gcry_chd, cnf.key, keyLength);
    if (gcryError) {
        fprintf(stderr, "gcry_cipher_setkey failed:  %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
        r = 1;
        goto done;
    }

    gcryError = gcry_cipher_setiv(cnf.libData.gcry.gcry_chd, iv, cnf.blkLength);
    if (gcryError) {
        fprintf(stderr, "gcry_cipher_setiv failed:  %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
        r = 1;
        goto done;
    }
done:
    return r;
}

static void removePadding(char *buf, size_t *plen) {
    unsigned len = (unsigned)*plen;
    unsigned iSrc, iDst;
    char *frstNUL;

    frstNUL = memchr(buf, 0x00, *plen);
    if (frstNUL == NULL) goto done;
    iDst = iSrc = frstNUL - buf;

    while (iSrc < len) {
        if (buf[iSrc] != 0x00) buf[iDst++] = buf[iSrc];
        ++iSrc;
    }

    *plen = iDst;
done:
    return;
}

static void gcryDecryptBlock(FILE *fpin, FILE *fpout, off64_t blkEnd, off64_t *pCurrOffs) {
    gcry_error_t gcryError;
    size_t nRead, nWritten;
    size_t toRead;
    size_t leftTillBlkEnd;
    char buf[64 * 1024];

    leftTillBlkEnd = blkEnd - *pCurrOffs;
    while (1) {
        toRead = sizeof(buf) <= leftTillBlkEnd ? sizeof(buf) : leftTillBlkEnd;
        toRead = toRead - toRead % cnf.blkLength;
        nRead = fread(buf, 1, toRead, fpin);
        if (nRead == 0) break;
        leftTillBlkEnd -= nRead, *pCurrOffs += nRead;
        gcryError = gcry_cipher_decrypt(cnf.libData.gcry.gcry_chd,  // gcry_cipher_hd_t
                                        buf,  // void *
                                        nRead,  // size_t
                                        NULL,  // const void *
                                        0);  // size_t
        if (gcryError) {
            fprintf(stderr, "gcry_cipher_decrypt failed:  %s/%s\n", gcry_strsource(gcryError),
                    gcry_strerror(gcryError));
            return;
        }
        removePadding(buf, &nRead);
        nWritten = fwrite(buf, 1, nRead, fpout);
        if (nWritten != nRead) {
            perror("fpout");
            return;
        }
    }
}

static int gcryDoDecrypt(FILE *logfp, FILE *eifp, FILE *outfp) {
    off64_t blkEnd;
    off64_t currOffs = 0;
    int r = 1;
    int fd;
    struct stat buf;

    while (1) {
        /* process block */
        if (gcryInit(eifp) != 0) goto done;
        /* set blkEnd to size of logfp and proceed. */
        if ((fd = fileno(logfp)) == -1) {
            r = -1;
            goto done;
        }
        if ((r = fstat(fd, &buf)) != 0) goto done;
        blkEnd = buf.st_size;
        r = eiGetEND(eifp, &blkEnd);
        if (r != 0 && r != 1) goto done;
        gcryDecryptBlock(logfp, outfp, blkEnd, &currOffs);
        gcry_cipher_close(cnf.libData.gcry.gcry_chd);
    }
    r = 0;
done:
    return r;
}

#else
// Dummy function definitions
static int gcryDoDecrypt(FILE __attribute__((unused)) * logfp,
                         FILE __attribute__((unused)) * eifp,
                         FILE __attribute__((unused)) * outfp) {
    return 0;
}
static int rsgcryAlgoname2Algo(char *const __attribute__((unused)) algoname) {
    return 0;
}
static int rsgcryModename2Mode(char *const __attribute__((unused)) modename) {
    return 0;
}
#endif
/* LIBGCRYPT RELATED ENDS */

#ifdef ENABLE_OPENSSL_CRYPTO_PROVIDER
static int osslInit(FILE *eifp) {
    int r = 0;
    char iv[4096];
    size_t keyLength;

    if ((cnf.libData.ossl.chd = EVP_CIPHER_CTX_new()) == NULL) {
        fprintf(stderr, "internal error[%s:%d]: EVP_CIPHER_CTX_new failed\n", __FILE__, __LINE__);
        r = 1;
        goto done;
    }

    cnf.blkLength = EVP_CIPHER_get_block_size(cnf.libData.ossl.cipher);
    if (cnf.blkLength > sizeof(iv)) {
        fprintf(stderr,
                "internal error[%s:%d]: block length %lld too large for "
                "iv buffer\n",
                __FILE__, __LINE__, (long long)cnf.blkLength);
        r = 1;
        goto done;
    }
    if ((r = eiGetIV(eifp, iv, cnf.blkLength)) != 0) goto done;

    keyLength = EVP_CIPHER_get_key_length(cnf.libData.ossl.cipher);
    assert(cnf.key != NULL); /* "fix" clang 10 static analyzer false positive */
    if (strlen(cnf.key) != keyLength) {
        fprintf(stderr,
                "invalid key length; key is %u characters, but "
                "exactly %llu characters are required\n",
                cnf.keylen, (long long unsigned)keyLength);
        r = 1;
        goto done;
    }

    if ((r = EVP_DecryptInit_ex(cnf.libData.ossl.chd, cnf.libData.ossl.cipher, NULL, (uchar *)cnf.key, (uchar *)iv)) !=
        1) {
        fprintf(stderr, "EVP_DecryptInit_ex failed:  %d\n", r);
        goto done;
    }

    if ((r = EVP_CIPHER_CTX_set_padding(cnf.libData.ossl.chd, 0)) != 1) {
        fprintf(stderr, "EVP_CIPHER_set_padding failed:  %d\n", r);
        goto done;
    }

    r = 0;
done:
    return r;
}

static void osslDecryptBlock(FILE *fpin, FILE *fpout, off64_t blkEnd, off64_t *pCurrOffs) {
    size_t nRead, nWritten;
    size_t toRead;
    size_t leftTillBlkEnd;
    uchar buf[64 * 1024];
    uchar outbuf[64 * 1024];
    int r, tmplen, outlen;

    leftTillBlkEnd = blkEnd - *pCurrOffs;
    while (1) {
        toRead = sizeof(buf) <= leftTillBlkEnd ? sizeof(buf) : leftTillBlkEnd;
        toRead = toRead - toRead % cnf.blkLength;
        nRead = fread(buf, 1, toRead, fpin);
        if (nRead == 0) break;
        leftTillBlkEnd -= nRead, *pCurrOffs += nRead;

        r = EVP_DecryptUpdate(cnf.libData.ossl.chd, outbuf, &tmplen, buf, nRead);
        if (r != 1) {
            fprintf(stderr, "EVP_DecryptUpdate failed: %d\n", r);
            return;
        }
        outlen = tmplen;

        nWritten = fwrite(outbuf, sizeof(unsigned char), (size_t)outlen, fpout);
        if (nWritten != (size_t)outlen) {
            perror("fpout");
            return;
        }
    }

    r = EVP_DecryptFinal_ex(cnf.libData.ossl.chd, outbuf + tmplen, &tmplen);
    if (r != 1) {
        fprintf(stderr, "EVP_DecryptFinal_ex failed: %d\n", r);
        return;
    }
    outlen += tmplen;
}

static int osslDoDecrypt(FILE *logfp, FILE *eifp, FILE *outfp) {
    off64_t blkEnd;
    off64_t currOffs = 0;
    int r = 1;
    int fd;
    struct stat buf;

    while (1) {
        /* process block */
        if (osslInit(eifp) != 0) goto done;
        /* set blkEnd to size of logfp and proceed. */
        if ((fd = fileno(logfp)) == -1) {
            r = -1;
            goto done;
        }
        if ((r = fstat(fd, &buf)) != 0) goto done;
        blkEnd = buf.st_size;
        r = eiGetEND(eifp, &blkEnd);
        if (r != 0 && r != 1) goto done;
        osslDecryptBlock(logfp, outfp, blkEnd, &currOffs);
        EVP_CIPHER_CTX_free(cnf.libData.ossl.chd);
    }
    r = 0;
done:
    return r;
}


#else
// Dummy function definitions
static int osslDoDecrypt(FILE __attribute__((unused)) * logfp,
                         FILE __attribute__((unused)) * eifp,
                         FILE __attribute__((unused)) * outfp) {
    return 0;
}
#endif

static void decrypt(const char *name) {
    FILE *logfp = NULL, *eifp = NULL;
    int r = 0;
    char eifname[4096];

    if (!strcmp(name, "-")) {
        fprintf(stderr, "decrypt mode cannot work on stdin\n");
        goto err;
    } else {
        if ((logfp = fopen(name, "r")) == NULL) {
            perror(name);
            goto err;
        }
        snprintf(eifname, sizeof(eifname), "%s%s", name, ENCINFO_SUFFIX);
        eifname[sizeof(eifname) - 1] = '\0';
        if ((eifp = fopen(eifname, "r")) == NULL) {
            perror(eifname);
            goto err;
        }
        if (eiCheckFiletype(eifp) != 0) goto err;
    }

    if (cnf.lib == LIB_GCRY) {
        if ((r = gcryDoDecrypt(logfp, eifp, stdout)) != 0) goto err;
    } else if (cnf.lib == LIB_OSSL) {
        if ((r = osslDoDecrypt(logfp, eifp, stdout)) != 0) goto err;
    }

    fclose(logfp);
    logfp = NULL;
    fclose(eifp);
    eifp = NULL;
    return;

err:
    fprintf(stderr, "error %d processing file %s\n", r, name);
    if (eifp != NULL) fclose(eifp);
    if (logfp != NULL) fclose(logfp);
}

static void write_keyfile(char *fn) {
    int fd;
    int r;
    mode_t fmode;

    fmode = O_WRONLY | O_CREAT;
    if (!cnf.optionForce) fmode |= O_EXCL;
    if (fn == NULL) {
        fprintf(stderr, "program error: keyfile is NULL");
        exit(1);
    }
    if ((fd = open(fn, fmode, S_IRUSR)) == -1) {
        fprintf(stderr, "error opening keyfile ");
        perror(fn);
        exit(1);
    }
    if ((r = write(fd, cnf.key, cnf.keylen)) != (ssize_t)cnf.keylen) {
        fprintf(stderr, "error writing keyfile (ret=%d) ", r);
        perror(fn);
        exit(1);
    }
    close(fd);
}

static void getKeyFromFile(const char *fn) {
    const int r = cryGetKeyFromFile(fn, &cnf.key, &cnf.keylen);
    if (r != 0) {
        perror(fn);
        exit(1);
    }
}

static void getRandomKey(void) {
    int fd;
    cnf.keylen = cnf.randomKeyLen;
    cnf.key = malloc(cnf.randomKeyLen); /* do NOT zero-out! */
    /* if we cannot obtain data from /dev/urandom, we use whatever
     * is present at the current memory location as random data. Of
     * course, this is very weak and we should consider a different
     * option, especially when not running under Linux (for Linux,
     * unavailability of /dev/urandom is just a theoretic thing, it
     * will always work...).  -- TODO -- rgerhards, 2013-03-06
     */
    if ((fd = open("/dev/urandom", O_RDONLY)) >= 0) {
        if (read(fd, cnf.key, cnf.randomKeyLen) != cnf.randomKeyLen) {
            fprintf(stderr,
                    "warning: could not read sufficient data "
                    "from /dev/urandom - key may be weak\n");
        };
        close(fd);
    }
}

static void setKey(void) {
    if (cnf.randomKeyLen != -1)
        getRandomKey();
    else if (cnf.keyfile != NULL)
        getKeyFromFile(cnf.keyfile);
    else if (cnf.keyprog != NULL)
        cryGetKeyFromProg(cnf.keyprog, &cnf.key, &cnf.keylen);
    if (cnf.key == NULL) {
        fprintf(stderr, "ERROR: key must be set via some method\n");
        exit(1);
    }
}

/* Retrieve algorithm and mode from the choosen library. In libgcrypt,
this is done in two steps (AES128 + CBC). However, other libraries expect this to be
expressed in a single step, e.g. AES-128-CBC in openssl */
static void setAlgoMode(char *algo, char *mode) {
    if (cnf.lib == LIB_GCRY) { /* Set algorithm and mode for gcrypt */
#ifdef ENABLE_LIBGCRYPT
        if (algo != NULL) {
            cnf.libData.gcry.cry_algo = rsgcryAlgoname2Algo(algo);
            if (cnf.libData.gcry.cry_algo == GCRY_CIPHER_NONE) {
                fprintf(stderr,
                        "ERROR: algorithm \"%s\" is not "
                        "known/supported\n",
                        algo);
                exit(1);
            }
        }
        if (mode != NULL) {
            cnf.libData.gcry.cry_mode = rsgcryModename2Mode(mode);
            if (cnf.libData.gcry.cry_mode == GCRY_CIPHER_MODE_NONE) {
                fprintf(stderr,
                        "ERROR: cipher mode \"%s\" is not "
                        "known/supported\n",
                        mode);
                exit(1);
            }
        }
#endif
    } else if (cnf.lib == LIB_OSSL) {
#ifdef ENABLE_OPENSSL_CRYPTO_PROVIDER
        if (algo != NULL) {
            cnf.libData.ossl.cipher = EVP_CIPHER_fetch(NULL, algo, NULL);
            if (cnf.libData.ossl.cipher == NULL) {
                fprintf(stderr,
                        "ERROR: cipher \"%s\" is not "
                        "known/supported\n",
                        algo);
                exit(1);
            }
        }
#endif
    }
}

static struct option long_options[] = {{"verbose", no_argument, NULL, 'v'},
                                       {"version", no_argument, NULL, 'V'},
                                       {"decrypt", no_argument, NULL, 'd'},
                                       {"force", no_argument, NULL, 'f'},
                                       {"write-keyfile", required_argument, NULL, 'W'},
                                       {"key", required_argument, NULL, 'K'},
                                       {"generate-random-key", required_argument, NULL, 'r'},
                                       {"keyfile", required_argument, NULL, 'k'},
                                       {"key-program", required_argument, NULL, 'p'},
                                       {"algo", required_argument, NULL, 'a'},
                                       {"mode", required_argument, NULL, 'm'},
                                       {"lib", required_argument, NULL, 'l'},
                                       {NULL, 0, NULL, 0}};

static const char *short_options = "a:dfk:K:m:p:r:vVW:l:";

int main(int argc, char *argv[]) {
    int opt;
    char *newKeyFile = NULL;
    char *lib = NULL;
    char *algo = NULL, *mode = NULL;

    /* We need preprocessing to determine, which crypto library is going to be used */
    while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1 && lib == NULL) {
        switch (opt) {
            case 'l':
                lib = optarg;
                break;
            default:
                break;
        }
    }

    /* Once we reach this point, we have library specific internals set */
    if (initConfig(lib)) exit(1);

    optind = 1;
    while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                cnf.mode = MD_DECRYPT;
                break;
            case 'W':
                cnf.mode = MD_WRITE_KEYFILE;
                newKeyFile = optarg;
                break;
            case 'k':
                cnf.keyfile = optarg;
                break;
            case 'p':
                cnf.keyprog = optarg;
                break;
            case 'f':
                cnf.optionForce = 1;
                break;
            case 'r':
                cnf.randomKeyLen = atoi(optarg);
                if (cnf.randomKeyLen > 64 * 1024) {
                    fprintf(stderr,
                            "ERROR: keys larger than 64KiB are "
                            "not supported\n");
                    exit(1);
                }
                break;
            case 'K':
                fprintf(stderr,
                        "WARNING: specifying the actual key "
                        "via the command line is highly insecure\n"
                        "Do NOT use this for PRODUCTION use.\n");
                cnf.key = optarg;
                cnf.keylen = strlen(cnf.key);
                break;
            case 'a':
                algo = optarg;
                break;
            case 'm':
                mode = optarg;
                break;
            case 'v':
                cnf.verbose = 1;
                break;
            case 'V':
                fprintf(stderr, "rscryutil " VERSION "\n");
                exit(0);
                break;
            case 'l':
                break;
            case '?':
                break;
            default:
                fprintf(stderr, "getopt_long() returns unknown value %d\n", opt);
                return 1;
        }
    }

    setKey();
    setAlgoMode(algo, mode);
    assert(cnf.key != NULL);

    if (cnf.mode == MD_WRITE_KEYFILE) {
        if (optind != argc) {
            fprintf(stderr,
                    "ERROR: no file parameters permitted in "
                    "--write-keyfile mode\n");
            exit(1);
        }
        write_keyfile(newKeyFile);
    } else {
        if (optind == argc)
            decrypt("-");
        else {
            for (int i = optind; i < argc; ++i) decrypt(argv[i]);
        }
    }

    assert(cnf.key != NULL);
    memset(cnf.key, 0, cnf.keylen); /* zero-out key store */
    cnf.keylen = 0;
    return 0;
}
