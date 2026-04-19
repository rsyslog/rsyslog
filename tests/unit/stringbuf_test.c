#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "rsyslog.h"
#include "obj.h"
#include "srUtils.h"
#include "stringbuf.h"
#include "unicode-helper.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                                \
        }                                                                            \
    } while (0)

void LogError(const int iErrno __attribute__((unused)),
              const int iErrCode __attribute__((unused)),
              const char *fmt __attribute__((unused)),
              ...) {}

rsRetVal objGetObjInterface(obj_if_t *pIf) {
    memset(pIf, 0, sizeof(*pIf));
    return RS_RET_OK;
}

rsRetVal srUtilItoA(char *pBuf, int iLenBuf, number_t iToConv) {
    int r = snprintf(pBuf, iLenBuf, "%lld", (long long)iToConv);

    /* Keep the stub strict: truncation and rare encoding errors are both failures. */
    return (r < 0 || r >= iLenBuf) ? RS_RET_INVALID_VALUE : RS_RET_OK;
}

/*
 * Pull the implementation into this test translation unit so Automake does not
 * need to manage dependency files for sources outside tests/ during distcheck.
 */
#include "../../runtime/stringbuf.c"
#include "../../compat/asprintf.c"

static int test_append_parts(void) {
    cstr_t *str = NULL;
    uchar *buf = NULL;
    const rs_cstr_part_t parts[] = {
        {(const uchar *)"alpha", strlen("alpha")},
        {(const uchar *)"", 0},
        {(const uchar *)"-", 1},
        {(const uchar *)"beta", strlen("beta")},
    };

    CHECK(rsCStrConstruct(&str) == RS_RET_OK);
    CHECK(rsCStrAppendParts(str, parts, sizeof(parts) / sizeof(parts[0])) == RS_RET_OK);
    cstrFinalize(str);
    CHECK(cstrConvSzStrAndDestruct(&str, &buf, 0) == RS_RET_OK);
    CHECK(strcmp((char *)buf, "alpha-beta") == 0);
    free(buf);

    CHECK(rsCStrConstruct(&str) == RS_RET_OK);
    CHECK(rsCStrAppendParts(str, NULL, 0) == RS_RET_OK);
    cstrFinalize(str);
    CHECK(cstrConvSzStrAndDestruct(&str, &buf, 0) == RS_RET_OK);
    CHECK(strcmp((char *)buf, "") == 0);
    free(buf);

    return 0;
}

static int test_vasprintf_helper(char **buf, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

static int test_vasprintf_helper(char **buf, const char *fmt, ...) {
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = vasprintf(buf, fmt, ap);
    va_end(ap);

    return ret;
}

static int test_asprintf_compat(void) {
    char *buf = NULL;

    CHECK(asprintf(&buf, "%s:%d", "value", 42) == (int)strlen("value:42"));
    CHECK(strcmp(buf, "value:42") == 0);
    free(buf);
    buf = NULL;

    CHECK(test_vasprintf_helper(&buf, "%s/%s", "left", "right") == (int)strlen("left/right"));
    CHECK(strcmp(buf, "left/right") == 0);
    free(buf);

    return 0;
}

static int test_cstr_copy_helper(void) {
    char dst[5];
    char raw_src[5] = {'a', 'b', 'c', 'd', 'e'};
    uchar udst[4];
    uchar uraw_src[4] = {'x', 'y', 'z', 'w'};

    CHECK(rs_cstr_copy(dst, "hi", sizeof(dst)) == 2);
    CHECK(strcmp(dst, "hi") == 0);

    CHECK(rs_cstr_copy(dst, "abcdef", sizeof(dst)) == sizeof(dst) - 1);
    CHECK(strcmp(dst, "abcd") == 0);

    CHECK(rs_cstr_copy(dst, raw_src, sizeof(dst)) == sizeof(dst) - 1);
    CHECK(memcmp(dst, "abcd", sizeof(dst) - 1) == 0);
    CHECK(dst[sizeof(dst) - 1] == '\0');

    CHECK(u_cstr_copy(udst, (const uchar *)"ok", sizeof(udst)) == 2);
    CHECK(strcmp((char *)udst, "ok") == 0);

    CHECK(u_cstr_copy(udst, uraw_src, sizeof(udst)) == sizeof(udst) - 1);
    CHECK(memcmp(udst, "xyz", sizeof(udst) - 1) == 0);
    CHECK(udst[sizeof(udst) - 1] == '\0');

    CHECK(rs_cstr_copy(dst, "ignored", 0) == 0);

    return 0;
}

static int test_empty_finalize_and_getsz(void) {
    cstr_t *str = NULL;

    CHECK(rsCStrConstruct(&str) == RS_RET_OK);
    cstrFinalize(str);
    CHECK(rsCStrLen(str) == 0);
    CHECK(strcmp((char *)rsCStrGetSzStrNoNULL(str), "") == 0);
    rsCStrDestruct(&str);
    CHECK(str == NULL);

    return 0;
}

static int test_destructive_convert_semantics(void) {
    cstr_t *str = NULL;
    uchar *buf = NULL;

    CHECK(rsCStrConstruct(&str) == RS_RET_OK);
    CHECK(rsCStrAppendStr(str, (uchar *)"ab") == RS_RET_OK);
    CHECK(cstrAppendChar(str, 'c') == RS_RET_OK);
    cstrFinalize(str);
    CHECK(cstrConvSzStrAndDestruct(&str, &buf, 0) == RS_RET_OK);
    CHECK(str == NULL);
    CHECK(strcmp((char *)buf, "abc") == 0);
    free(buf);

    CHECK(rsCStrConstruct(&str) == RS_RET_OK);
    cstrFinalize(str);
    CHECK(cstrConvSzStrAndDestruct(&str, &buf, 0) == RS_RET_OK);
    CHECK(str == NULL);
    CHECK(strcmp((char *)buf, "") == 0);
    free(buf);

    CHECK(rsCStrConstruct(&str) == RS_RET_OK);
    cstrFinalize(str);
    CHECK(cstrConvSzStrAndDestruct(&str, &buf, 1) == RS_RET_OK);
    CHECK(str == NULL);
    CHECK(buf == NULL);

    return 0;
}

static int test_setszstr_requires_finalize(void) {
    cstr_t *str = NULL;

    CHECK(rsCStrConstruct(&str) == RS_RET_OK);
    CHECK(rsCStrSetSzStr(str, (uchar *)"new-value") == RS_RET_OK);
    cstrFinalize(str);
    CHECK(rsCStrLen(str) == strlen("new-value"));
    CHECK(strcmp((char *)rsCStrGetSzStrNoNULL(str), "new-value") == 0);

    CHECK(rsCStrSetSzStr(str, NULL) == RS_RET_OK);
    cstrFinalize(str);
    CHECK(rsCStrLen(str) == 0);
    CHECK(strcmp((char *)rsCStrGetSzStrNoNULL(str), "") == 0);

    rsCStrDestruct(&str);
    return 0;
}

static int test_locate_in_szstr_basic_cases(void) {
    cstr_t *needle = NULL;

    CHECK(rsCStrConstructFromszStr(&needle, (uchar *)"needle") == RS_RET_OK);
    CHECK(rsCStrLocateInSzStr(needle, (uchar *)"hayneedlehay") == 3);
    CHECK(rsCStrLocateInSzStr(needle, (uchar *)"haystack") == -1);
    rsCStrDestruct(&needle);

    CHECK(rsCStrConstruct(&needle) == RS_RET_OK);
    CHECK(rsCStrLocateInSzStr(needle, (uchar *)"anything") == 0);
    rsCStrDestruct(&needle);

    return 0;
}

static int test_locate_in_szstr_needle_longer_than_haystack(void) {
    cstr_t *needle = NULL;

    CHECK(rsCStrConstructFromszStr(&needle, (uchar *)"much-longer-needle") == RS_RET_OK);
    CHECK(rsCStrLocateInSzStr(needle, (uchar *)"short") == -1);
    rsCStrDestruct(&needle);

    return 0;
}

static int test_prefix_suffix_helpers(void) {
    cstr_t *pattern = NULL;

    CHECK(rsCStrConstructFromszStr(&pattern, (uchar *)"prefix") == RS_RET_OK);
    CHECK(rsCStrSzStrStartsWithCStr(pattern, (uchar *)"prefix-value", strlen("prefix-value")) == 0);
    CHECK(rsCStrSzStrStartsWithCStr(pattern, (uchar *)"value-prefix", strlen("value-prefix")) != 0);
    rsCStrDestruct(&pattern);

    CHECK(rsCStrConstructFromszStr(&pattern, (uchar *)".log") == RS_RET_OK);
    CHECK(rsCStrSzStrEndsWithCStr(pattern, (uchar *)"app.log", strlen("app.log")) == 0);
    CHECK(rsCStrSzStrEndsWithCStr(pattern, (uchar *)"app.txt", strlen("app.txt")) != 0);
    rsCStrDestruct(&pattern);

    return 0;
}

int main(void) {
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"empty_finalize_and_getsz", test_empty_finalize_and_getsz},
        {"destructive_convert_semantics", test_destructive_convert_semantics},
        {"setszstr_requires_finalize", test_setszstr_requires_finalize},
        {"locate_in_szstr_basic_cases", test_locate_in_szstr_basic_cases},
        {"locate_in_szstr_needle_longer_than_haystack", test_locate_in_szstr_needle_longer_than_haystack},
        {"prefix_suffix_helpers", test_prefix_suffix_helpers},
        {"append_parts", test_append_parts},
        {"asprintf_compat", test_asprintf_compat},
        {"cstr_copy_helper", test_cstr_copy_helper},
    };
    size_t i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (tests[i].fn() != 0) {
            fprintf(stderr, "FAILED: %s\n", tests[i].name);
            return 1;
        }
    }

    printf("stringbuf tests passed (%zu cases)\n", sizeof(tests) / sizeof(tests[0]));
    return 0;
}
