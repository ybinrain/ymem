#!/bin/sh

# 获取版本号
perl version.pl

die() {
    echo "$@"
    exit 1
}

# 判断可执行文件是否存在
locate_binary() {
    for f in $@ 
    do
        file=`which $f 2>/dev/null | grep -v '^no '`
        if test -n "$file" -a -x "$file"; then
            echo $file
            return 0
        fi
    done
    return 1
}


# 检查aclocal, 执行并生成 aclocal.m4 宏定义文件
echo "aclocal..."
if test x"$ACLOCAL" = x; then
    ACLOCAL=`locate_binary aclocal aclocal-1.15`
    if test x"$ACLOCAL" = x; then
        die "Did not find a supported acloal"
    fi
fi

$ACLOCAL || exit 1

# autoheader 执行并生成 config.h.in 头文件模板
echo "autoheader..."
AUTOHEADER=${AUTOHEADER:-autoheader}
$AUTOHEADER || exit 1

# 检查automake, 执行并生成 Makefile.in 模板文件
echo "automake..."
if test x"$AUTOMAKE" = x; then
    AUTOMAKE=`locate_binary automake`
    if test x"$AUTOMAKE" = x; then
        die "Did not find a supported automake"
    fi
fi
$AUTOMAKE --foreign --add-missing || $AUTOMAKE --gnu --add-missing || exit 1

# autoconf, 执行并生成 configure 文件
echo "autoconf..."
AUTOCONF=${AUTOCONF:-autoconf}
$AUTOCONF || exit 1

