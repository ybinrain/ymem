AC_PREEQ(2.70)              # 文件要求autoconf 的版本
m4_inlucde([version.m4])
m4_include([m4/c99-backport.m4])
AC_INIT([memcached], [VERSION_NUMBER], [zyb0807@gmail.com])     # 初始化
AC_CANONICAL_HOST           # 规范系统名称
AC_CONFIG_SRCDIR([memcached.c])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([config.h])

AC_PROG_CC

AC_CONFIG_FILES(Makefile doc/Makefile)
AC_OUTPUT
