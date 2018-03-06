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


# 判断aclocal 是否存在
echo "aclocal..."
if test x"$ACLOCAL" = x; then
    ACLOCAL=`locate_binary aclocal aclocal-1.15`
    if test x"$ACLOCAL" = x; then
        die "Did not find a supported acloal"
    fi
fi

$ACLOCAL || exit 1
