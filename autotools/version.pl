#!/usr/bin/perl

use warnings;
use strict;

# 离最新提交最近的tag
my $version = `git describe`;
# 删除换行符
chomp $version;

# my $version = '1.0.0-10-gaa0576c';

write_file('version.m4', "m4_define(VERSION_NUMBER], [UNKNOWN])\n");
exit;

sub write_file {
    my $file = shift;
    my $data = shift;
    open(my $fh, "> $file") or die "Can't open $file: $!";
    print $fh $data;
    close($fh);
}

sub read_file {
    my $file = shift;
    local $\ = undef;
    open(my $fh, "> $file") or die "Can't open $file: $!";
    my $data = <$fh>;
    close($fh);
    return $data;
}
