# 要求 autoconf 版本不小于2.69
AC_PREREQ([2.69])
m4_include([version.m4])
# 初始化 autoconf
AC_INIT([ymemd], [v1.0.0], [zyb0807@gmail.com])

# 检查源文件是否存在
AC_CONFIG_SRCDIR([memcached.c])

# 初始化 automake
# foreign 表示不采用gnu 标准发布格式
# 需要先执行 aclocal 生成 AM_INIT_AUTOMAKE 宏定义
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

# 根据 config.h.in 生成 config.h 文件
# autoheader 根据 configure.ac 生成 config.h.in 文件
AM_CONFIG_HEADER([config.h])

# 编译器检查
AC_PROG_CC


dnl
dnl 
dnl
AC_DEFUN([DETECT_CLANG], 
[
    AC_MSG_CHECKING([for clang in use])
    #AC_COMPILE_IFELSE(
    #[
    #],
    #[CLANG=yes],[CLANG=no])
    AC_MSG_RESULT([$CLANG])

])
DETECT_CLANG([],[])

# 根据 Makefile.in 生成 Makefile 文件
# automake 根据 configure.ac Makefile.am 生成 Makefile.in 文件
AC_CONFIG_FILES([Makefile])

# 输出 config.h/Makefile 文件
AC_OUTPUT
