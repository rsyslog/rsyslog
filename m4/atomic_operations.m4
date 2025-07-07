# rsyslog
#
# atomic_operations.m4 - autoconf macro to check if compiler supports atomic
# operations
#
# rgerhards, 2008-09-18, added based on
# http://svn.apache.org/repos/asf/apr/apr/trunk/configure.in
#
#
AC_DEFUN([RS_ATOMIC_OPERATIONS],
[AC_CACHE_CHECK([whether the compiler provides atomic builtins], [ap_cv_atomic_builtins],
[AC_TRY_RUN([
#include <sys/types.h>
int main()
{
    unsigned long val = 1010, tmp, *mem = &val;
    time_t tval = 1010, ttmp, *tmem = &tval;

    if (__sync_fetch_and_add(&val, 1010) != 1010 || val != 2020)
        return 1;
    tmp = val;
    if (__sync_fetch_and_sub(mem, 1010) != tmp || val != 1010)
        return 1;
    if (__sync_sub_and_fetch(&val, 1010) != 0 || val != 0)
        return 1;
    tmp = 3030;
    if (__sync_val_compare_and_swap(mem, 0, tmp) != 0 || val != tmp)
        return 1;
    if (__sync_lock_test_and_set(&val, 4040) != 3030)
        return 1;
    mem = &tmp;
    if (__sync_val_compare_and_swap(&mem, &tmp, &val) != &tmp)
        return 1;

    if (__sync_fetch_and_add(&tval, 1010) != 1010 || tval != 2020)
        return 1;
    ttmp = tval;
    if (__sync_fetch_and_sub(tmem, 1010) != ttmp || tval != 1010)
        return 1;
    if (__sync_sub_and_fetch(&tval, 1010) != 0 || tval != 0)
        return 1;
    ttmp = 3030;
    if (__sync_val_compare_and_swap(tmem, 0, ttmp) != 0 || tval != ttmp)
        return 1;
    if (__sync_lock_test_and_set(&tval, 4040) != 3030)
        return 1;
    tmem = &ttmp;
    if (__sync_val_compare_and_swap(&tmem, &ttmp, &tval) != &ttmp)
        return 1;

    __sync_synchronize();

    if (mem != &val)
        return 1;

    if (tmem != &tval)
        return 1;

    return 0;
}], [ap_cv_atomic_builtins=yes], [ap_cv_atomic_builtins=no], [ap_cv_atomic_builtins=no])])

if test "$ap_cv_atomic_builtins" = "yes"; then
    AC_DEFINE(HAVE_ATOMIC_BUILTINS, 1, [Define if compiler provides atomic builtins])
fi

])
