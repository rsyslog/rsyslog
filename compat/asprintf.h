#ifndef INCLUDED_COMPAT_ASPRINTF_H
#define INCLUDED_COMPAT_ASPRINTF_H

#include <stdarg.h>
#include <stdio.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(HAVE_DECL_ASPRINTF) || !HAVE_DECL_ASPRINTF
    #if defined(__GNUC__)
int asprintf(char **strp, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    #else
int asprintf(char **strp, const char *fmt, ...);
    #endif
#endif

#if !defined(HAVE_DECL_VASPRINTF) || !HAVE_DECL_VASPRINTF
    #if defined(__GNUC__)
int vasprintf(char **strp, const char *fmt, va_list ap) __attribute__((format(printf, 2, 0)));
    #else
int vasprintf(char **strp, const char *fmt, va_list ap);
    #endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_COMPAT_ASPRINTF_H */
