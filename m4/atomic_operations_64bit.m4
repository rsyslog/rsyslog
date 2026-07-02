# rsyslog
#
# atomic_operations.m4 - autoconf macro to check if compiler supports atomic
# operations
#
# rgerhards, 2008-09-18, added based on
# http://svn.apache.org/repos/asf/apr/apr/trunk/configure.in
#
#
AC_DEFUN([RS_ATOMIC_OPERATIONS_64BIT],
[AC_CACHE_CHECK([whether the compiler provides atomic builtins for 64 bit data types], [ap_cv_atomic_builtins_64],
[AC_TRY_RUN([
int main()
{
    unsigned long long val = 1010, tmp, *mem = &val;
    void *ptr = &val, *expected_ptr, *new_ptr;

    if (__atomic_fetch_add(&val, 1010, __ATOMIC_SEQ_CST) != 1010 || val != 2020)
        return 1;

    tmp = val;

    if (__atomic_fetch_sub(mem, 1010, __ATOMIC_SEQ_CST) != tmp || val != 1010)
        return 1;

    if (__atomic_sub_fetch(&val, 1010, __ATOMIC_SEQ_CST) != 0 || val != 0)
        return 1;

    tmp = 3030;

    {
        unsigned long long expected = 0;
        if (!__atomic_compare_exchange_n(mem, &expected, tmp, 0,
                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) ||
            val != tmp)
            return 1;
    }
    __atomic_store_n(&val, 4040, __ATOMIC_RELEASE);
    if (__atomic_load_n(&val, __ATOMIC_ACQUIRE) != 4040)
        return 1;

    expected_ptr = &val;
    new_ptr = &tmp;
    if (!__atomic_compare_exchange_n(&ptr, &expected_ptr, new_ptr, 0,
            __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
        return 1;

    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    if (ptr != &tmp)
        return 1;

    return 0;
}], [ap_cv_atomic_builtins_64=yes], [ap_cv_atomic_builtins_64=no], [ap_cv_atomic_builtins_64=no])])

if test "$ap_cv_atomic_builtins_64" = "yes"; then
    AC_DEFINE(HAVE_ATOMIC_BUILTINS64, 1, [Define if compiler provides 64 bit atomic builtins])
fi

])
