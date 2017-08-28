AC_PREEQ(2.70)              # 文件要求autoconf 的版本
m4_inlucde([version.m4])
m4_include([m4/c99-backport.m4])
AC_INIT([memcached], [VERSION_NUMBER], [zyb0807@gmail.com])     # 初始化
AC_CANONICAL_HOST           # 规范系统名称
AC_CONFIG_SRCDIR([memcached.c])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([config.h])

AC_PROG_CC

dnl **********************************************************************
dnl DETECT_ICC ([ACTION-IF-YES], [ACTION-IF-NO])
dnl
dnl check if this is the Intel ICC compiler, and if so run the ACTION-IF-YES
dnl sets the $ICC variable to "yes" or "no"
dnl **********************************************************************
AC_DEFUN([DETECT_ICC],
[
    ICC="no"
    AC_MSG_CHECKING([for icc in use])
    if test "$GCC" == "yes"; then
        dnl check if this is icc acting as gcc in disguise
        AC_EGREP_CPP([^__INTEL_COMPILER], [__INTEL_COMPILER],
          AC_MSG_RESULT([no])
          [$2],
          AC_MSG_RESULT([yes])
          [$1]
          ICC="yes")
    else
        AC_MSG_RESULT([no])
        [$2]
    fi
])

DETECT_ICC([], [])

AC_CONFIG_FILES(Makefile doc/Makefile)
AC_OUTPUT

